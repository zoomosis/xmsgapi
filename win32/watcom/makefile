# XMSGAPI library makefile
# Watcom C++ 11.x (32-bit Windows)
# Use wmake -ms

CC=wcl386
CFLAGS=-zq -wx -bt=nt -l=nt
CDEFS=-D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
COPT=-oneatx
LIB=wlib
LFLAGS=-q
SRC_DIR=..\..\src
TARGET=xmsgapi.lib

OBJS=api_sdm.obj sq_area.obj sq_hash.obj sq_help.obj sq_idx.obj sq_kill.obj \
	sq_lock.obj sq_misc.obj sq_msg.obj sq_read.obj sq_uid.obj \
	sq_write.obj api_jam.obj stamp.obj win32.obj msgapi.obj qksort.obj \
	putword.obj

# Thunderbirds are go!

{$(SRC_DIR)}.c.obj:
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -c $<
	$(LIB) $(LFLAGS) $(TARGET) -+$@

$(TARGET): $(OBJS)

clean:
	del *.obj
	del *.bak
	del $(TARGET)
