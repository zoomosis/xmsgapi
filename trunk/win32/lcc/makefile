# XMSGAPI library makefile
# lcc-win32 (32-bit Windows)

SRC_DIR=../../src
TARGET=xmsgapi.lib
CC=lcc
CFLAGS=-I$(SRC_DIR)
CDEFS=-D__WIN32__ -D__LITTLE_ENDIAN__ -DMSGAPI_NO_JAM
COPT=-O2
LIB=lcclib

# Thunderbirds are go!

$(TARGET):
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Foapi_sdm.obj $(SRC_DIR)/api_sdm.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_area.obj $(SRC_DIR)/sq_area.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_hash.obj $(SRC_DIR)/sq_hash.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_help.obj $(SRC_DIR)/sq_help.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_idx.obj $(SRC_DIR)/sq_idx.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_kill.obj $(SRC_DIR)/sq_kill.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_lock.obj $(SRC_DIR)/sq_lock.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_misc.obj $(SRC_DIR)/sq_misc.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_msg.obj $(SRC_DIR)/sq_msg.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_read.obj $(SRC_DIR)/sq_read.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_uid.obj $(SRC_DIR)/sq_uid.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fosq_write.obj $(SRC_DIR)/sq_write.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Foapi_jam.obj $(SRC_DIR)/api_jam.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Focvtdate.obj $(SRC_DIR)/stamp.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fomsgapi.obj $(SRC_DIR)/msgapi.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Foqksort.obj $(SRC_DIR)/qksort.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fostructrw.obj $(SRC_DIR)/putword.c
	$(CC) $(CFLAGS) $(CDEFS) $(COPT) -Fotdelay.obj $(SRC_DIR)/win32.c
	$(LIB) /verbose $(TARGET) *.obj

clean:
	del *.obj
	del *.bak
	del $(TARGET)
