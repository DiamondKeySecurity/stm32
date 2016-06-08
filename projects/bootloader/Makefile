PROG = bootloader

OBJS = crc32.o dfu.o

BOARD_OBJS = \
	./stm-init.o \
	$(TOPLEVEL)/stm-uart.o \
	$(TOPLEVEL)/stm-flash.o \
	$(TOPLEVEL)/syscalls.o \
	$(BOARD_DIR)/system_stm32f4xx.o \
	$(BOARD_DIR)/stm32f4xx_hal_msp.o \
	./startup_stm32f429xx.o \
	./stm32f4xx_it.o

LIBS = $(MBED_DIR)/libstmf4.a

all: $(PROG:=.elf)

%.elf: %.o $(BOARD_OBJS) $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $^ -o $@ -T$(BOOTLOADER_LDSCRIPT) -g -Wl,-Map=$*.map
	$(OBJCOPY) -O ihex $*.elf $*.hex
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(OBJDUMP) -St $*.elf >$*.lst
	$(SIZE) $*.elf

clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.hex
	rm -f *.bin
	rm -f *.map
	rm -f *.lst
