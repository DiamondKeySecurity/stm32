/*
 * stm-kermit.c
 * ------------
 * Interface to the E-Kermit library.
 *
 * Copyright (C) 1995, 2011, 
 * Trustees of Columbia University in the City of New York.
 * All rights reserved.
 *
 * Copyright (c) 2016, NORDUnet A/S All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the copyright holders nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This file is based on unixio.c and main.c from the E-Kermit distribution,
 * with extensive modifications for the Cryptech environment.
 */

#include <ctype.h>
#include <string.h>

#include "cdefs.h"
#include "debug.h"
#include "platform.h"
#include "kermit.h"

/* The following symbols, defined in kermit.h, conflict with stm32f429xx.h.
 * Neither one is used in the file.
 */
#undef CR
#undef SUCCESS

#include "stm-uart.h"
#include "ringbuf.h"
#include "stm-kermit.h"

static ringbuf_t uart_ringbuf;

/* current character received from UART */
static uint8_t uart_rx;

/* Callback for HAL_UART_Receive_IT(). Must be public. */
void HAL_UART1_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ringbuf_write_char(&uart_ringbuf, uart_rx);
    HAL_UART_Receive_IT(huart, &uart_rx, 1);
}

static void uart_init(void)
{
    ringbuf_init(&uart_ringbuf);
    HAL_UART_Receive_IT(&huart_mgmt, &uart_rx, 1);
}

#ifdef DEBUG
/* This is public because it's prototyped in debug.h */
void
dodebug(int fc, UCHAR * label, UCHAR * sval, long nval)
{
    if (label == (UCHAR *)"==========")
        uart_send_string("\r\n");
    uart_send_integer(HAL_GetTick(), 8);
    uart_send_char(' ');
    
    if (!label)
        label = (UCHAR *)"";

    switch (fc) {			/* Function code */
    case DB_OPN:			/* Open debug log */
	uart_send_string("DEBUG LOG OPEN\r\n");
	return;
    case DB_MSG:			/* Write a message */
        uart_send_string((char *)label);
        if (nval) {
            uart_send_char(' ');
            uart_send_integer(nval, 1);
        }
        uart_send_string("\r\n");
	return;
    case DB_CHR:			/* Write label and character */
        uart_send_string((char *)label);
        uart_send_string("=[");
        uart_send_char((uint8_t)nval);
        uart_send_string("]\r\n");
	return;
    case DB_PKT:			/* Log a packet */
        uart_send_string((char *)label);
        uart_send_char(' ');
        uart_send_integer(nval, 1);
        uart_send_char('[');
        /* It would be great if we could interpret the packet.
         * For now, just dump the raw data.
         */
        for (int i = 0; i < nval; ++i, ++sval) {
            if (*sval == '\\')
                uart_send_string("\\\\");
            else if (isprint(*sval))
                uart_send_char(*sval);
            else {
                uart_send_char('\\');
                uart_send_hex(*sval, 2);
            }
        }
        uart_send_string("]\r\n");
	return;
    case DB_LOG:			/* Write label and string or number */
        uart_send_string((char *)label);
	if (sval) {
            uart_send_char('[');
            uart_send_string((char *)sval);
            uart_send_char(']');
        }
	else {
            uart_send_char('=');
            uart_send_integer(nval, 1);
        }
        uart_send_string("\r\n");
	return;
    case DB_CLS:			/* Close debug log */
        return;
    }
}
#endif

/*  R E A D P K T  --  Read a Kermit packet from the communications device  */
/*
  Call with:
  k   - Kermit struct pointer
  p   - pointer to read buffer
  len - length of read buffer

  When reading a packet, this function looks for start of Kermit packet
  (k->r_soh), then reads everything between it and the end of the packet
  (k->r_eom) into the indicated buffer.  Returns the number of bytes read, or:
  0   - timeout or other possibly correctable error;
  -1   - fatal error, such as loss of connection, or no buffer to read into.
*/

static int
readpkt(struct k_data * k, UCHAR *p, int len)
{
    int n;
    short flag;
    UCHAR x, c;
#ifdef DEBUG
    UCHAR * p2;
#endif

#ifdef F_CTRLC
    short ccn;
    ccn = 0;
#endif

    flag = n = 0;                       /* Init local variables */

#ifdef DEBUG
    p2 = p;
#endif

    uart_init();

    while (1) {
        /* wait for the next character */
        while (ringbuf_read_char(&uart_ringbuf, &x) == 0) { ; }

        c = (k->parity) ? x & 0x7f : x & 0xff; /* Strip parity */

#ifdef F_CTRLC
	/* In remote mode only: three consecutive ^C's to quit */
        if (k->remote && c == (UCHAR) 3) {
            if (++ccn > 2) {
		debug(DB_MSG,"readpkt ^C^C^C",0,0);
		return(-1);
	    }
        } else {
	    ccn = 0;
	}
#endif

        if (!flag && c != k->r_soh)	/* No start of packet yet */
            continue;                     /* so discard these bytes. */
        if (c == k->r_soh) {		/* Start of packet */
            flag = 1;                   /* Remember */
            continue;                   /* But discard. */
        } else if (c == k->r_eom	/* Packet terminator */
		   || c == '\012'	/* 1.3: For HyperTerminal */
            ) {
#ifdef DEBUG
	    debug(DB_PKT,"RPKT",p2,n);
#endif
            return(n);
        } else {                        /* Contents of packet */
            if (n++ > k->r_maxlen)	/* Check length */
                return(0);
            else
                *p++ = x & 0xff;
        }
    }
    debug(DB_MSG,"READPKT FAIL (end)",0,0);
    return(-1);
}

/*  T X _ D A T A  --  Writes n bytes of data to communication device.  */
/*
  Call with:
  k = pointer to Kermit struct.
  p = pointer to data to transmit.
  n = length.
  Returns:
  X_OK on success.
  X_ERROR on failure to write - i/o error.
*/
static int 
tx_data(struct k_data * k, UCHAR *p, int n)
{
    return (uart_send_bytes(STM_UART_MGMT, p, n) == HAL_OK) ? X_OK : X_ERROR;
}

__weak int kermit_openfile(UCHAR * name) { return X_OK; }

/*  O P E N F I L E  --  Open output file  */
/*
  Call with:
  Pointer to filename.
  Size in bytes.
  Creation date in format yyyymmdd hh:mm:ss, e.g. 19950208 14:00:00
  Mode: 1 = read, 2 = create, 3 = append.
  Returns:
  X_OK on success.
  X_ERROR on failure, including rejection based on name, size, or date.    
*/
static int
openfile(struct k_data * k, UCHAR * s, int mode)
{
    int rc;

    switch (mode) {
    case 2:				/* Write (create) */
        rc = kermit_openfile(s); 
	debug(DB_LOG,"openfile write ",s,rc);
	return(rc);
    default:
	debug(DB_LOG,"openfile unsupported mode",0,mode);
        return(X_ERROR);
    }
}

__weak int kermit_writefile(UCHAR * s, int n) { return X_OK; }

/*  W R I T E F I L E  --  Write data to file  */
/*
  Call with:
  Kermit struct
  String pointer
  Length
  Returns:
  X_OK on success
  X_ERROR on failure, such as i/o error, space used up, etc
*/
static int
writefile(struct k_data * k, UCHAR * s, int n)
{
    debug(DB_LOG,"writefile binary",0,k->binary);
    return kermit_writefile(s, n);
}

__weak int kermit_closefile(void) { return X_OK; }
__weak int kermit_cancelfile(void) { return X_OK; }

/*  C L O S E F I L E  --  Close output file  */
/*
  Mode = 1 for input file, mode = 2 or 3 for output file.

  For output files, the character c is the character (if any) from the Z
  packet data field.  If it is D, it means the file transfer was canceled
  in midstream by the sender, and the file is therefore incomplete.  This
  routine should check for that and decide what to do.  It should be
  harmless to call this routine for a file that that is not open.
*/
static int
closefile(struct k_data * k, UCHAR c, int mode)
{
    switch (mode) {
    case 2:				/* Closing output file */
	debug(DB_LOG,"closefile (output) name",k->filename,0);
        if (c == 'D')
            return kermit_cancelfile();
        else
            return kermit_closefile();
    default:
	debug(DB_LOG,"closefile unsupported mode",0,mode);
        return(X_ERROR);
    }
}

static UCHAR o_buf[OBUFLEN+8];		/* File output buffer */

int
kermit_main(void)
{
    struct k_data k;                    /* Kermit data structure */
    struct k_response r;                /* Kermit response structure */
    int status, rx_len;
    UCHAR *inbuf;
    short r_slot;

    debug(DB_MSG,"==========",0,0);
    debug(DB_OPN,"debug.log",0,0);
    debug(DB_MSG,"Initializing...",0,0);

/*  Fill in parameters for this run */

    memset(&k, 0, sizeof(k));

    k.remote = 1;			/* 0 = local, 1 = remote */
    k.xfermode = 1;			/* 0 = automatic, 1 = manual */
    k.binary = BINARY;			/* 0 = text, 1 = binary */
    k.parity = P_PARITY;                /* Default parity = PAR_NONE */
    k.bct = 1;                       	/* Block check type */

/*  Fill in the i/o pointers  */
  
    k.obuf   = o_buf;			/* File output buffer */
    k.obuflen = OBUFLEN;		/* File output buffer length */

/* Fill in function pointers */

    k.rxd    = readpkt;			/* for reading packets */
    k.txd    = tx_data;			/* for sending packets */
    k.openf  = openfile;                /* for opening files */
    k.writef = writefile;               /* for writing to output file */
    k.closef = closefile;               /* for closing files */
#ifdef DEBUG
    k.dbf    = dodebug;			/* for debugging */
#endif

/* Initialize Kermit protocol */

    status = kermit(K_INIT, &k, 0, 0, "", &r);
#ifdef DEBUG
    debug(DB_LOG,"init status:",0,status);
    debug(DB_LOG,"version:",k.version,0);
#endif
    if (status == X_ERROR)
        return(FAILURE);
/*
  Now we read a packet ourselves and call Kermit with it.  Normally, Kermit
  would read its own packets, but in the embedded context, the device must be
  free to do other things while waiting for a packet to arrive.  So the real
  control program might dispatch to other types of tasks, of which Kermit is
  only one.  But in order to read a packet into Kermit's internal buffer, we
  have to ask for a buffer address and slot number.

  To interrupt a transfer in progress, set k.cancel to I_FILE to interrupt
  only the current file, or to I_GROUP to cancel the current file and all
  remaining files.  To cancel the whole operation in such a way that the
  both Kermits return an error status, call Kermit with K_ERROR.
*/
    while (status != X_DONE) {
/*
  Here we block waiting for a packet to come in (unless readpkt times out).
  Another possibility would be to call inchk() to see if any bytes are waiting
  to be read, and if not, go do something else for a while, then come back
  here and check again.
*/
        inbuf = getrslot(&k,&r_slot);	/* Allocate a window slot */
        rx_len = k.rxd(&k,inbuf,P_PKTLEN); /* Try to read a packet */
/*
  For simplicity, kermit() ACKs the packet immediately after verifying it was
  received correctly.  If, afterwards, the control program fails to handle the
  data correctly (e.g. can't open file, can't write data, can't close file),
  then it tells Kermit to send an Error packet next time through the loop.
*/
        if (rx_len < 1) {               /* No data was read */
            freerslot(&k,r_slot);	/* So free the window slot */
            if (rx_len < 0)             /* If there was a fatal error */
                return(FAILURE);          /* give up */

	    /* This would be another place to dispatch to another task */
	    /* while waiting for a Kermit packet to show up. */

        }
        /* Handle the input */

        switch (status = kermit(K_RUN, &k, r_slot, rx_len, "", &r)) {
        case X_OK:
#ifdef DEBUG
/*
  This shows how, after each packet, you get the protocol state, file name,
  date, size, and bytes transferred so far.  These can be used in a
  file-transfer progress display, log, etc.
*/
	    debug(DB_LOG,"NAME",r.filename ? r.filename : (UCHAR *)"(NULL)",0);
	    debug(DB_LOG,"DATE",r.filedate ? r.filedate : (UCHAR *)"(NULL)",0);
	    debug(DB_LOG,"SIZE",0,r.filesize);
	    debug(DB_LOG,"STATE",0,r.status);
	    debug(DB_LOG,"SOFAR",0,r.sofar);
#endif
	    /* Maybe do other brief tasks here... */
	    continue;			/* Keep looping */
        case X_DONE:
            debug(DB_LOG,"X_DONE exiting",0,SUCCESS);
	    break;			/* Finished */
        case X_ERROR:
            debug(DB_LOG,"X_ERROR exiting",0,FAILURE);
	    return(FAILURE);		/* Failed */
	}
    }
    return(SUCCESS);
}
