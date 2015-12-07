#ifndef __MEM_C__
#define __MEM_C__

#include <error.h>
#include <alloca.h>

#include "gc.c"

typedef void (*natfun)(void);

#define STACK_SIZE  0x1000

obj *stack[STACK_SIZE];
size_t sptr = STACK_SIZE;

enum {
    ROOT_NIL    = 0,
    ROOT_TRUE,
    ROOT_FALSE,
    ROOT_GLOBAL,
    ROOTS_SIZE
};

obj *roots[ROOTS_SIZE];

typedef enum {
    TYPE_NATFUN = 0,
    TYPE_LAMBDA,
    TYPE_CONS,
    TYPE_INTEGER,
    TYPE_REAL,
    TYPE_SYMBOL,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_NIL
} native_type;

typedef struct {
    native_type type;
    natfun x;
} __attribute__((packed)) native_natfun;

typedef struct {
    native_type type;
    int64_t x;
} __attribute__((packed)) native_integer;

typedef struct {
    native_type type;
    double x;
} __attribute__((packed)) native_real;

typedef struct {
    native_type type;
    char x[];
} __attribute__((packed)) native_symbol;

static inline native_type obj_type(obj *o) {
    return *(native_type*)obj_binary_ptr(o);
}

static inline void obj_assert_type(obj *o, native_type type) {
    if (obj_type(o) != type)
        error(1, 0, "Type error (expected %d, found %d)!", type, obj_type(o));
}

#define RTOS        (rstack[rptr])
#define RNOS        (rstack[rptr+1])

#define PICK(n)     (stack[sptr+(n)])
#define TOS         PICK(0)
#define NOS         PICK(1)
#define NNOS        PICK(2)

#define NIL         (roots[ROOT_NIL])
#define TRUE        (roots[ROOT_TRUE])
#define FALSE       (roots[ROOT_FALSE])
#define GLOBAL      (roots[ROOT_GLOBAL])

#define HEAD(o)     ((o)->ref[0])
#define TAIL(o)     ((o)->ref[1])

heap main_heap;

static obj *pop(void) {
    if (sptr >= STACK_SIZE) {
        error(1, 0, "Stack underflow");
    }
    return stack[sptr++];
}

static void push(obj *o) {
    if (sptr < 1) {
        error(1, 0, "Stack overflow");
    }
    stack[--sptr] = o;
}

static void gc_copy_roots(void *dest, size_t *len) {
    size_t i;
    for (i=sptr; i<STACK_SIZE; i++)
        gc_copy(stack[i], dest, len);
    for (i=0; i<ROOTS_SIZE; i++)
        gc_copy(roots[i], dest, len);
}

static void gc_relink_roots(void) {
    gc_relink(stack + sptr, STACK_SIZE-sptr);
    gc_relink(roots, ROOTS_SIZE);
}

static obj *new_obj(size_t n_refs, size_t binary_size) {
    obj *o;
    if (n_refs) {
        if (binary_size) {
            o = gc_alloc(&main_heap,
                    sizeof(obj) + (n_refs+1)*sizeof(obj*) + binary_size);
            o->ref[n_refs] = (obj*)binary_size;
        } else {
            o = gc_alloc(&main_heap,
                    sizeof(obj) + n_refs*sizeof(obj*));
        }
        o->len = n_refs;
    } else {
        if (binary_size) {
            o = gc_alloc(&main_heap,
                    sizeof(obj) + binary_size);
        } else {
            return NULL;
        }
        o->len = binary_size;
    }
    o->live = 0;
    o->refs = n_refs != 0;
    o->binary = binary_size != 0;
    return o;
}

static obj *new_obj_fill(size_t n_refs, const void *data, size_t binary_size) {
    obj *o = new_obj(n_refs, binary_size);
    memcpy(obj_binary_ptr(o), data, binary_size);
    return o;
}

static obj *new_string(const char *s) {
    size_t size = sizeof(native_symbol) + strlen(s) + 1;
    native_symbol *data = alloca(size);
    data->type = TYPE_STRING;
    strcpy(data->x, s);
    return new_obj_fill(0, data, size);
}

static obj *new_symbol(const char *s) {
    size_t size = sizeof(native_symbol) + strlen(s) + 1;
    native_symbol *data = alloca(size);
    data->type = TYPE_SYMBOL;
    strcpy(data->x, s);
    return new_obj_fill(0, data, size);
}

static obj *new_integer(int64_t x) {
    native_integer data;
    data.type = TYPE_INTEGER;
    data.x = x;
    return new_obj_fill(0, &data, sizeof(data));
}

static obj *new_real(int64_t x) {
    native_real data;
    data.type = TYPE_REAL;
    data.x = x;
    return new_obj_fill(0, &data, sizeof(data));
}

static obj *new_natfun(natfun x) {
    native_natfun data;
    data.type = TYPE_NATFUN;
    data.x = x;
    return new_obj_fill(0, &data, sizeof(data));
}

static obj *new_nil(void) {
    native_type type = TYPE_NIL;
    return new_obj_fill(0, &type, sizeof(type));
}

static obj *new_bool(int x) {
    native_integer data;
    data.x = (x != 0);
    data.type = TYPE_BOOL;
    return new_obj_fill(0, &data, sizeof(data));
}   

#endif

