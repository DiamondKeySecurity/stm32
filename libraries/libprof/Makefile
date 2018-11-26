LIB = libprof.a

OBJS = gmon.o profil.o profiler.o memfunc.o

# Don't profile the profiling code, because that way lies madness (and recursion).
CFLAGS := $(subst -pg,,$(CFLAGS))

all: $(LIB)

# But do profile the mem functions
memfunc.o: memfunc.c
	$(CC) $(CFLAGS) -pg -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o : %.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): $(OBJS)
	$(AR) -r $@ $^

clean:
	rm -f $(OBJS) $(LIB)
