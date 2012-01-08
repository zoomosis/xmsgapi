CFLAGS=-W -Wall -g

# Use SMAPI
#COPT=-DUSE_JAM -DUSE_SMAPI
#CINC=-I../../../src/smapi/smapi -I../../../src/huskylib
#LIBDIR=-L../../../src/smapi/make/ -L../../../src/huskylib/make/
#LIBS=-lsmapi_p -lhuskybsd

# Use XMSGAPI
COPT=-DUSE_JAM
CINC=-I../src
LIBDIR=-L../unix/gnu-c
LIBS=-lxmsgapi

all: sqconver packtest

sqconver: sqconver.c
	$(CC) $(COPT) $(CINC) $(CFLAGS) -o sqconver sqconver.c $(LIBDIR) $(LIBS)

packtest: packtest.c
	$(CC) $(COPT) $(CINC) $(CFLAGS) -o packtest packtest.c

all: sqconver packtest

clean:
	rm -f *.o sqconver packtest
