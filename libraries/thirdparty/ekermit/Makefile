EKERMIT = ../../../../thirdparty/ekermit

vpath %.c $(EKERMIT)

OBJS = kermit.o stm-kermit.o

LIB = libekermit.a

CFLAGS += -I$(EKERMIT) -DSTATIC=static -DRECVONLY -DMINSIZE #-DDEBUG

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) -r $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIB)
