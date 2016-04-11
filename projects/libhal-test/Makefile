TEST = cores test-bus test-trng test-hash test-aes-key-wrap test-pbkdf2 test-ecdsa test-rsa
#TEST += test-rpc_hash test-rpc_pkey test-rpc_get_version test-rpc_get_random
TEST += test-rpc_server

CFLAGS += -I $(LIBHAL_DIR)
LIBC_OBJS = printf.o gettimeofday.o
LIBS += $(LIBHAL_DIR)/libhal.a $(LIBTFM_DIR)/libtfm.a

all: $(TEST:=.elf)

vpath %.c $(LIBHAL_DIR)/tests $(LIBHAL_DIR)/utils

# .mo extension for files with main() that need to be wrapped as __main()
%.mo: %.c
	$(CC) -c $(CFLAGS) -Dmain=__main -o $@ $<

%.elf: %.mo main.o $(BOARD_OBJS) $(LIBC_OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $*.elf -T$(LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

# don't automatically delete objects, to avoid a lot of unnecessary rebuilding
.SECONDARY: $(BOARD_OBJS) $(LIBC_OBJS)

clean:
	rm -f *.o *.mo
	rm -f *.elf
	rm -f *.hex
	rm -f *.bin
	rm -f *.map
	rm -f *.lst