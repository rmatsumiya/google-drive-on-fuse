CC		= gcc
CFLAGS	= -I/usr/include -I/usr/include/json-c -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -g
LDFLAGS	= -lcurl -ljson-c -lfuse -lglib-2.0 -lmagic
OBJS	= fuse_interface.o gdrive_interface.o http_interface.o oauth_interface.o json_interface.o str.o
PROGRAM	= fuse

all:		$(PROGRAM)

$(PROGRAM):	$(OBJS)
			$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $(PROGRAM)

clean:;		rm -f *.o $(PROGRAM)
