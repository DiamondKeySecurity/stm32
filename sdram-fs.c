#include "sdram-malloc.h"
#include "sdram-fs.h"

/* maximum number of files */
#ifndef SDFS_NFILE
#define SDFS_NFILE 4
#endif

__attribute__((section(".sdram1"))) filetab_t filetab[SDFS_NFILE];
static filetab_t *curfile;
static int nfile;
static int initialized = 0;

/* Initialize the SDRAM file system
 */
int sdfs_init(void)
{
    if (initialized && nfile)
        sdram_free(filetab[0].loc);
    memset((void *)filetab, 0, sizeof(filetab));
    curfile = NULL;
    nfile = 0;
    initialized = 1;
}

/* Return the number of entries in the file table
 */
int sdfs_count(void)
{
    return nfile;
}

/* Return the first entry in the file table
 */
filetab_t *sdfs_first(void)
{
    curfile = filetab;
    return nfile ? curfile : NULL;
}

/* Return the next entry in the file table
 */
filetab_t *sdfs_next(void)
{
    return (++curfile < &filetab[nfile]) ? curfile : NULL;
}

/* Open (create) a new file
 */
int sdfs_open(unsigned char *name)
{
    if (nfile == SDFS_NFILE) {
        curfile = NULL;
        return -1;
    }
    curfile = &filetab[nfile];
    strncpy(curfile->name, (char *)name, sizeof(curfile->name));
    curfile->loc = sdram_malloc(0);
    if (curfile->loc == NULL) {
        curfile = NULL;
        return -1;
    }
    curfile->len = 0;
    ++nfile;
    return 0;
}

/* Write data to the open file
 */
int sdfs_write(unsigned char *buf, int len)
{
    if (curfile == NULL)
        return -1;
    uint8_t *writeptr = sdram_malloc(len);
    if (writeptr == NULL)
        return -1;
    memcpy(writeptr, buf, len);
    curfile->len += len;
    return 0;
}

/* Close the open file
 */
int sdfs_close(void)
{
    if (curfile == NULL)
        return -1;
    int pad = ALIGN - (curfile->len & (ALIGN - 1));
    if (sdram_malloc(pad) == NULL)
        return -1;
    curfile = NULL;
    return 0;
}

/* Close and remove the open file
 */
int sdfs_cancel(void)
{
    if (curfile == NULL)
        return -1;
    sdram_free(curfile->loc);
    memset(curfile, 0, sizeof(*curfile));
    --nfile;
    return 0;
}

