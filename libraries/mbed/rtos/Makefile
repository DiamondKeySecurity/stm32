CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar

###########################################

vpath %.c rtx/TARGET_CORTEX_M
vpath %.S rtx/TARGET_CORTEX_M/TARGET_RTOS_M4_M7/TOOLCHAIN_GCC

SRCS =	rtos/rtos_idle.c \
	HAL_CM.c \
	rt_CMSIS.c \
	rt_Event.c \
	rt_List.c \
	rt_Mailbox.c \
	rt_MemBox.c \
	rt_Mutex.c \
	rt_Robin.c \
	rt_Semaphore.c \
	rt_System.c \
	rt_Task.c \
	rt_Time.c \
	RTX_Conf_CM.c \
	HAL_CM4.S \
	SVC_Table.S

OBJS = $(patsubst %.S,%.o, $(patsubst %.c,%.o, $(SRCS)))

all: librtos.a

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o : %.S
	$(CC) $(CFLAGS) -c -o $@ $<

librtos.a: $(OBJS)
	$(AR) -r $@ $(OBJS)

clean:
	rm -f $(OBJS) librtos.a
