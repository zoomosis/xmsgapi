# XMSGAPI library makefile
# Borland C++ 5.x (32-bit Windows)

CC=bcc32
CFLAGS=-w -WC -w-8059
CDEFS=-D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
#CDEFS=-DMSGAPI_NO_JAM
COPT=-O2 -d
#COPT=-v -y
LIB=tlib
SRC_DIR=..\..\src
TARGET=xmsgapi.lib

OBJS=api_sdm.obj sq_area.obj sq_hash.obj sq_help.obj sq_idx.obj \
	sq_kill.obj sq_lock.obj sq_misc.obj sq_msg.obj sq_read.obj \
	sq_uid.obj sq_write.obj api_jam.obj stamp.obj win32.obj \
	msgapi.obj putword.obj qksort.obj

# Thunderbirds are go!

{$(SRC_DIR)}.c.obj:
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -c $<
	$(LIB) $(TARGET) -+$@

$(TARGET): $(OBJS)

clean:
	del *.obj
	del *.bak
	del $(TARGET)
