all: secretfs

clean:
	$(RM) secretfs *.o

CFLAGS := $(CFLAGS) -O0 -g -D_FILE_OFFSET_BITS=64
LDLIBS := $(LDLIBS) -lfuse -lgfshare

secretfs: secretfs.o sharefarm.o
