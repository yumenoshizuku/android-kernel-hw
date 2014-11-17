#
# Makefile for SThreads
#

CC=gcc
CPPFLAGS= -I.
CFLAGS=-g -O2 -Wall
LDFLAGS=-g

.c.s:
	$(CC) $(CFLAGS) $(CPPFLAGS) -S $<

test: test_basic test_starve test_pc

test_basic: test_basic.o  libsthread.a
	$(CC) $(LDFLAGS) -o $@ test_basic.o -L. -lsthread

test_starve: test_starve.o  libsthread.a
	$(CC) $(LDFLAGS) -o $@ test_starve.o -L. -lsthread

test_pc: test_pc.o libsthread.a
	$(CC) $(LDFLAGS) -o $@ test_pc.o -L. -lsthread

libsthread.a: sthread.o sync.o
	$(AR) cr $@ $^

clean:
	$(RM) libsthread.a test_basic test_starve test_pc *.o *~
