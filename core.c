#ifndef __CORE_C__
#define __CORE_C__

#include "mem.c"

// ( a b -- b a )
static void core_swap(void) {
    obj *o = TOS;
    TOS = NOS;
    NOS = o;
}

// ( a -- a a )
static void core_dup(void) {
    push(TOS);
}

// ( a -- )
static void core_drop(void) {
    pop();
}

// ( a b -- a b a )
static void core_over(void) {
    push(NOS);
}

// ( a b -- b )
static void core_nip(void) {
    NOS = TOS;
    pop();
}

// ( a b c -- b c a )
static void core_rot(void) {
    obj *o = TOS;
    TOS = NNOS;
    NNOS = NOS;
    NOS = o;
}

// ( -- nil )
static void core_nil(void) {
    push(NIL);
}

// ( a b -- a::b )
static void core_cons(void) {
    obj *o = new_obj(2, sizeof(native_type));
    *(native_type*)obj_binary_ptr(o) = TYPE_CONS;
    TAIL(o) = pop();
    HEAD(o) = pop();
    push(o);
}

// ( a::b -- a b )
static void core_decons(void) {
    core_dup();
    NOS = HEAD(NOS);
    TOS = TAIL(TOS);
}

// ( a::b -- a )
static void core_head(void) {
    obj_assert_type(TOS, TYPE_CONS);
    TOS = HEAD(TOS);
}

// ( a::b -- b )
static void core_tail(void) {
    obj_assert_type(TOS, TYPE_CONS);
    TOS = TAIL(TOS);
}

// ( a b -- a++b )
static void core_append(void) {
    if (NOS == NIL) {
        core_nip();
        return;
    }
    push(HEAD(NOS));        // a b head(a)
    core_rot();             // b head(a) a
    core_tail();            // b head(a) tail(a)
    core_rot();             // head(a) tail(a) b
    core_append();          // head(a) tail(a)++b
    core_cons();            // head(a)::tail(a)++b = a++b
}

// ( a b -- c )
// TODO: handle cycles
static void core_eq(void) {
    if (TOS == NOS) {
        core_drop();
        core_drop();
        push(TRUE);
    } else {
        int result = 0;
        if (obj_n_refs(TOS) == obj_n_refs(NOS) &&
            obj_binary_size(TOS) == obj_binary_size(NOS))
        {
            if (!memcmp(obj_binary_ptr(TOS), obj_binary_ptr(NOS),
                        obj_binary_size(TOS)))
            {
                if (TOS->refs) {
                    result = 1;
                    size_t i;
                    for (i=0; i<TOS->len; i++) {
                        push(TOS->ref[i]);
                        push(NOS->ref[i]);
                        core_eq();
                        if (pop() == FALSE) {
                            result = 0;
                            break;
                        }
                    }
                } else result = 1;
            }
        }
        core_drop();
        core_drop();
        push((result)? TRUE : FALSE);
    }
}

// ( map key -- value true/false )
static void core_lookup(void) {
    while (NOS != NIL) {
        push(HEAD(HEAD(NOS)));
        core_over();
        core_eq();
        if (pop() == TRUE) {
            core_drop();
            TOS = HEAD(TAIL(HEAD(TOS)));
            push(TRUE);
            return;
        } else {
            NOS = TAIL(NOS);
        }
    }
    core_drop();
    core_drop();
    push(FALSE);
}

// ( map key value -- map )
static void core_extend(void) {
    core_nil();
    core_cons();
    core_cons();
    core_swap();
    core_cons();
}

// ( vars body env -- lambda )
static void core_lambda(void) {
    native_type type = TYPE_LAMBDA;
    obj *o = new_obj_fill(3, &type, sizeof(type));
    o->ref[0] = NNOS;
    o->ref[1] = NOS;
    o->ref[2] = TOS;
    core_drop();
    core_drop();
    core_drop();
    push(o);
}

// ( natfun -- )
static void core_execute(void) {
    obj_assert_type(TOS, TYPE_NATFUN);
    native_natfun *nf = obj_binary_ptr(pop());
    nf->x();
}

// ( a b -- a+b )
static void core_plus(void) {
    if (obj_type(NOS) == TYPE_INTEGER && obj_type(TOS) == TYPE_INTEGER) {
        NOS = new_integer(
                    ((native_integer*)obj_binary_ptr(NOS))->x +
                    ((native_integer*)obj_binary_ptr(TOS))->x);
        core_drop();
    } else if(obj_type(NOS) == TYPE_REAL && obj_type(TOS) == TYPE_REAL) {
        NOS = new_real(
                ((native_real*)obj_binary_ptr(NOS))->x +
                ((native_real*)obj_binary_ptr(TOS))->x);
    } else {
        error(1, 0, "Trying to add types %d and %d",
                obj_type(NOS), obj_type(TOS));
    }
}

// ( a b -- a*b )
static void core_mul(void) {
    if (obj_type(NOS) == TYPE_INTEGER && obj_type(TOS) == TYPE_INTEGER) {
        NOS = new_integer(
                    ((native_integer*)obj_binary_ptr(NOS))->x *
                    ((native_integer*)obj_binary_ptr(TOS))->x);
        core_drop();
    } else if(obj_type(NOS) == TYPE_REAL && obj_type(TOS) == TYPE_REAL) {
        NOS = new_real(
                ((native_real*)obj_binary_ptr(NOS))->x *
                ((native_real*)obj_binary_ptr(TOS))->x);
    } else {
        error(1, 0, "Trying to add types %d and %d",
                obj_type(NOS), obj_type(TOS));
    }
}

// ( a b -- a/b )
static void core_div(void) {
    if (obj_type(NOS) == TYPE_INTEGER && obj_type(TOS) == TYPE_INTEGER) {
        NOS = new_integer(
                    ((native_integer*)obj_binary_ptr(NOS))->x /
                    ((native_integer*)obj_binary_ptr(TOS))->x);
        core_drop();
    } else if(obj_type(NOS) == TYPE_REAL && obj_type(TOS) == TYPE_REAL) {
        NOS = new_real(
                ((native_real*)obj_binary_ptr(NOS))->x /
                ((native_real*)obj_binary_ptr(TOS))->x);
    } else {
        error(1, 0, "Trying to add types %d and %d",
                obj_type(NOS), obj_type(TOS));
    }
}

// ( a -- -a )
static void core_neg(void) {
    if (obj_type(TOS) == TYPE_INTEGER) {
        TOS = new_integer(-((native_integer*)obj_binary_ptr(TOS))->x);
    } else if (obj_type(TOS) == TYPE_REAL) {
        TOS = new_real(-((native_real*)obj_binary_ptr(TOS))->x);
    } else {
        error(1, 0, "Trying to negate type %dd", obj_type(TOS));
    }
}

// ( a b -- a<b )
static void core_lt(void) {
    if (obj_type(NOS) == TYPE_INTEGER && obj_type(TOS) == TYPE_INTEGER) {
        NOS = new_bool(
                    ((native_integer*)obj_binary_ptr(NOS))->x <
                    ((native_integer*)obj_binary_ptr(TOS))->x);
        core_drop();
    } else if(obj_type(NOS) == TYPE_REAL && obj_type(TOS) == TYPE_REAL) {
        NOS = new_bool(
                ((native_real*)obj_binary_ptr(NOS))->x <
                ((native_real*)obj_binary_ptr(TOS))->x);
    } else {
        error(1, 0, "Trying to add types %d and %d",
                obj_type(NOS), obj_type(TOS));
    }
}

#endif

