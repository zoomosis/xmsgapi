# XMSGAPI library makefile
# GNU C (generic)
# Requires GNU Make.
# If you can get it to work with BSD make, send me the changes!

CC=gcc
CFLAGS=-pipe -W -Wall -c
CDEFS=-DUNIX -DMSGAPI_NO_JAM
COPT=-O2
LIB=ar
RANLIB=ranlib
SRC_DIR=../../src
TARGET=libxmsgapi.a

OBJS=api_sdm.o sq_area.o sq_hash.o sq_help.o sq_idx.o sq_kill.o sq_lock.o \
	sq_misc.o sq_msg.o sq_read.o sq_uid.o sq_write.o api_jam.o stamp.o \
	msgapi.o qksort.o patmat.o unix.o putword.o

# Thunderbirds are go!

%.o:
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) $(SRC_DIR)/$*.c
	$(LIB) r $(TARGET) $@

$(TARGET): $(OBJS)
	$(RANLIB) $(TARGET)

clean:
	rm -f *.o $(TARGET)

