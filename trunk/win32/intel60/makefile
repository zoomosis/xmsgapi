# XMSGAPI library makefile
# Intel C++ 6.0 (32-bit Windows)

CC=icl
CFLAGS=-nologo -W3
CDEFS=-DWIN32 -D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
COPT=-Ox -Gf -MD -Qsox-
LIB=lib
SRC_DIR=..\..\src
TARGET=xmsgapi.lib

OBJS=api_sdm.obj sq_area.obj sq_hash.obj sq_help.obj \
	sq_idx.obj sq_kill.obj sq_lock.obj sq_misc.obj sq_msg.obj \
	sq_read.obj sq_uid.obj sq_write.obj api_jam.obj stamp.obj \
	win32.obj msgapi.obj qksort.obj

# Thunderbirds are go!

{$(SRC_DIR)}.c.obj:
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -c $<

$(TARGET): $(OBJS)
	$(LIB) -nologo -out:$(TARGET) $(OBJS)

clean:
	del *.obj
	del *.bak
	del $(TARGET)
