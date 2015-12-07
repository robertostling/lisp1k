# lisp1k
Interpreter for a minimal LISP-like language (in 1k lines of C)

## Quick start

To build and test the interpreter, just type:

    make

    ./lisp <test.lisp

## Structure

The interpreter consists of four C files:

 * `gc.c`: a simple copying garbage collector
 * `mem.c`: primitives for the dynamic type system + runtime stack
 * `core.c`: a library of stack machine functions
 * `lisp.c`: the LISP interpreter itself, implemented using the stack machine

In the interest of keeping things simple, only the most basic functionality is
implemented.

