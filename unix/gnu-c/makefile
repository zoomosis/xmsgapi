# XMSGAPI library makefile for GNU C (or compatible)
# Works with BSD Make or GNU Make

CFLAGS=-pipe -W -Wall
CDEFS=-DUNIX -DMSGAPI_NO_JAM
#CDEFS=-DUNIX -I../../../jamlib
COPT=-O2 -g
LIB=ar
RANLIB=ranlib
SRC_DIR=../../src
TARGET=libxmsgapi.a
VPATH=$(SRC_DIR)

OBJS=	api_sdm.o sq_area.o sq_hash.o sq_help.o sq_idx.o sq_kill.o sq_lock.o \
	sq_misc.o sq_msg.o sq_read.o sq_uid.o sq_write.o api_jam.o stamp.o \
	msgapi.o qksort.o patmat.o unix.o putword.o

# Thunderbirds are go!

.c.o:
	$(CC) $(CFLAGS) $(COPTS) $(CDEFS) -c $<
	$(LIB) r $(TARGET) $@

$(TARGET): $(OBJS)
	$(RANLIB) $(TARGET)

clean:
	rm -f *.o $(TARGET)
