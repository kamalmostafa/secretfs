all: secretfs

clean:
	$(RM) secretfs *.o

CPPFLAGS := $(CPPFLAGS) -D_FILE_OFFSET_BITS=64
LDLIBS := $(LDLIBS) -lfuse -lgfshare

secretfs: secretfs.o sharefarm.o
