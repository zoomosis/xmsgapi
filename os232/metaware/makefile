# XMSGAPI library makefile
# MetaWare High C++ 3.2 (32-bit OS/2)

CC=hc
CFLAGS=-w4 -Hmscerr
CDEFS=-D__OS2__ -D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
COPT=-O3
LIB=lib
SRC_DIR=..\..\src
TARGET=xmsgapi.lib

OBJS=api_sdm.obj sq_area.obj sq_hash.obj sq_help.obj sq_idx.obj \
	sq_kill.obj sq_lock.obj sq_misc.obj sq_msg.obj sq_read.obj \
	sq_uid.obj sq_write.obj api_jam.obj stamp.obj os232.obj \
	msgapi.obj putword.obj qksort.obj

# Thunderbirds are go!

{$(SRC_DIR)}.c.obj:
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -c $<
	$(LIB) $(TARGET) -replace $@ -nobackup -nobanner

$(TARGET): $(OBJS)

clean:
	del *.obj
	del $(TARGET)
