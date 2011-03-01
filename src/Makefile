all: secretfs

clean:
	$(RM) secretfs *.o

CFLAGS := $(CFLAGS) -O0 -g -D_FILE_OFFSET_BITS=64
LDFLAGS := $(LDFLAGS) -lfuse -lgfshare

secretfs: secretfs.o sharefarm.o
