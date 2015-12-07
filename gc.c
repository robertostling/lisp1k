#ifndef __GC_C__
#define __GC_C__

#include <stddef.h>

// NOTE: the user should include this file directly, and define the following
// two functions dealing with the GC roots:

// This function should call gc_copy(r, dest, len) for all roots r.
static void gc_copy_roots(void *dest, size_t *len);

// This function should call gc_relink(rs, n) for all arrays of root pointers
// rs (of length n).
static void gc_relink_roots(void);


#include <stdlib.h>
#include <string.h>

#define MIN(x,y)    (((x)<(y))?(x):(y))
#define MAX(x,y)    (((x)>(y))?(x):(y))

#ifdef __LP64__
// 64-bit specific definitions
typedef struct obj_struct {
    uint64_t live   : 1;            // set to 1 during GC if already copied
    uint64_t refs   : 1;            // does the object use references?
    uint64_t binary : 1;            // does the object use raw binary data?
    uint64_t len    : 61;           // number of references (if refs != 0),
                                    // otherwise len*sizeof(obj*) is the
                                    // number of bytes used for binary data
    struct obj_struct *ref[0];      // object data starts here.
                                    // during collection, ref[0] in the old
                                    // object will point to the new object.
                                    // see obj_size() below for details on how
                                    // size information is stored.
} __attribute__((packed)) obj;

// Any object will be aligned according to this function.
// Note that this only applies to the start of the object header, the contents
// start one word (4 or 8 bytes) after the header.
static inline size_t gc_align(size_t n) {
    return (n + 7) & (~(size_t)7);
}
#else
// 32-bit version of the above
typedef struct obj_struct {
    uint32_t live   : 1;
    uint32_t refs   : 1;
    uint32_t binary : 1;
    uint32_t len    : 29;
    struct obj_struct *ref[0];
} __attribute__((packed)) obj;

static inline size_t gc_align(size_t n) {
    return (n + 3) & (~(size_t)3);
}
#endif

typedef struct {
    void *p;                // pointer to heap memory
    size_t used;            // number of bytes currently used
    size_t size;            // current heap size (bytes)
    size_t max_size;        // maximum heap size (bytes)
    size_t last_used;       // number of bytes used after last collection
} heap;

// Number of bytes used by object o.
static inline size_t obj_size(const obj *o) {
    if (o->refs)
        return sizeof(*o) + o->len*sizeof(o) +
            ((o->binary)? sizeof(obj*) + (size_t)(o->ref[o->len]): 0);
    else
        return sizeof(*o) + o->len;
}

static inline size_t obj_binary_size(const obj *o) {
    if (o->binary) return (o->refs)? (size_t)(o->ref[o->len]) : o->len;
    else return 0;
}

static inline void* obj_binary_ptr(obj *o) {
    if (o->binary) return (o->refs)? o->ref + (o->len + 1) : o->ref;
    else return NULL;
}

static inline size_t obj_n_refs(const obj *o) {
    return (o->refs)? o->len : 0;
}

// Copy the object o to the new heap (at position *len).
static void gc_copy(obj *o, void *dest, size_t *len) {
    if (o->live) return;
    const size_t size = obj_size(o);
    obj *new_o = (obj*)(dest + (*len));
    memcpy(new_o, o, size);
    o->live = 1;
    *len = gc_align(*len + size);
    if (o->refs) {
        size_t i;
        for (i=0; i<o->len; i++) gc_copy(o->ref[i], dest, len);
    }
    o->ref[0] = new_o;
}

// Update references after copying to a new heap.
static inline void gc_relink(obj **refs, size_t len) {
    size_t i;
    for (i=0; i<len; i++) refs[i] = refs[i]->ref[0];
}

// Update references for all objects in the new heap.
static void gc_relink_heap(void *p, size_t len) {
    size_t base;

    for (base=0; base<len; ) {
        obj *o = p + base;
        if (o->refs) gc_relink(o->ref, o->len);
        base = gc_align(base + obj_size(o));
    }
}

static void gc_create_heap(heap *h, size_t size, size_t max_size) {
    h->p = malloc(size);
    h->used = 0;
    h->size = size;
    h->max_size = max_size;
    h->last_used = size;
}

static void gc_collect(heap *h) {
    size_t new_size = MIN(h->max_size,
                          MAX(h->size, gc_align(3*h->last_used/2)));
    size_t len = 0;
    void *dest = malloc(new_size);
    gc_copy_roots(dest, &len);
    gc_relink_heap(dest, len);
    gc_relink_roots();
    free(h->p);
    h->p = dest;
    h->used = len;
    h->size = new_size;
    h->last_used = len;
}

static obj *gc_alloc(heap *h, size_t size) {
    while (gc_align(h->used + size) >= h->size) gc_collect(h);
    obj *o = (obj*)(h->p + h->used);
    h->used = gc_align(h->used + size);
    return o;
}

#endif

