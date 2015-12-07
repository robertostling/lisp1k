CC=gcc
CFLAGS=-Wall -O0 -g

lisp: lisp.c core.c gc.c mem.c
	$(CC) $(CFLAGS) -o lisp lisp.c

clean:
	rm -f lisp
