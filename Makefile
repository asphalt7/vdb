CC      = gcc
CFLAGS  = -Wall -Wextra -Wno-unused-parameter -g
LDFLAGS =

SRCS    = main.c pager.c btree.c table.c repl.c
OBJS    = $(SRCS:.c=.o)
TARGET  = vdb

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
main.o:  main.c  table.h repl.h vdb.h
pager.o: pager.c pager.h vdb.h
btree.o: btree.c btree.h pager.h vdb.h
table.o: table.c table.h btree.h pager.h vdb.h
repl.o:  repl.c  repl.h  table.h btree.h vdb.h

clean:
	rm -f $(OBJS) $(TARGET) *.db

.PHONY: all clean
