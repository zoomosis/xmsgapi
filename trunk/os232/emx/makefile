# XMSGAPI library makefile
# EMX 0.9 (32-bit OS/2)

CC=gcc
CFLAGS=-pipe -W -Wall
CDEFS=-D__OS2__ -D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
COPT=-O2
LIB=ar
RANLIB=ranlib
SRC_DIR=..\..\src
TARGET=xmsgapi.a

OBJS=api_sdm.o sq_area.o sq_hash.o sq_help.o sq_idx.o \
	sq_kill.o sq_lock.o sq_misc.o sq_msg.o sq_read.o \
	sq_uid.o sq_write.o api_jam.o stamp.o os232.o \
	msgapi.o putword.o qksort.o

# Thunderbirds are go!

{$(SRC_DIR)}.c.o:
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -c $<
	$(LIB) r $(TARGET) $@

$(TARGET): $(OBJS)

clean:
	del *.o
	del $(TARGET)
