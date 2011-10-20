# XMSGAPI library makefile
# GNU C (mingw) (32-bit Windows)

CC=gcc
CFLAGS=-pipe -Wall -W -c -mno-cygwin
CDEFS=-D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
COPT=-O2
LIB=ar
RANLIB=ranlib
SRC_DIR=../../src
TARGET=libxmsgapi.a

OBJS=api_sdm.o sq_area.o sq_hash.o sq_help.o sq_idx.o sq_kill.o sq_lock.o \
	sq_misc.o sq_msg.o sq_read.o sq_uid.o sq_write.o api_jam.o \
	stamp.o msgapi.o qksort.o win32.o

# Thunderbirds are go!

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) $(SRC_DIR)/$*.c
	$(LIB) r $(TARGET) $@

$(TARGET): $(OBJS)
	$(RANLIB) $(TARGET)

clean:
	rm -f *.o $(TARGET)
