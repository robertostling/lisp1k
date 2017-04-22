#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>

#include "mem.c"
#include "core.c"

// ( -- string )
void core_token(void) {
    const size_t max_len = 0x100;
    size_t len = 0;
    char buf[max_len];
    int c;

    while (isspace(c = getchar()));
    if (c == EOF) {
        push(NIL);
        return;
    }
    if (c == '(' || c == ')') {
        buf[0] = c; buf[1] = 0;
        push(new_string(buf));
        return;
    }
    if (c == '"') {
        do {
            buf[len++] = c;
            if (len >= max_len-2) {
                error(1, 0, "Token too long!");
            }
            c = getchar();
        } while (c != EOF && c != '"');
        if (c == EOF) {
            push(NIL);
            return;
        }
        buf[len++] = c;
    } else {
        do {
            buf[len++] = c;
            if (len >= max_len-1) {
                error(1, 0, "Token too long!");
            }
            c = getchar();
        } while (!isspace(c) && c != EOF && c != '(' && c != ')');

        if (c == '(' || c == ')') {
            ungetc(c, stdin);
        }
    }

    buf[len] = 0;
    push(new_string(buf));
}

// ( string -- atom )
void core_atom(void) {
    obj_assert_type(TOS, TYPE_STRING);
    native_symbol *data = obj_binary_ptr(TOS);
    char *endptr = NULL;

    if (data->x[0] == '"') {
        size_t len = strlen(data->x);
        char buf[len-1];
        memcpy(buf, data->x + 1, len-2);
        buf[len-2] = 0;
        TOS = new_string(buf);
        return;
    }

    int64_t integer = strtoll(data->x, &endptr, 10);
    if (*endptr == 0 && data->x[0] != 0) {
        TOS = new_integer(integer);
        return;
    }

    double real = strtod(data->x, &endptr);
    if (*endptr == 0 && data->x[0] != 0) {
        TOS = new_real(real);
        return;
    }

    TOS = new_symbol(data->x);
}

void core_parse(void);

// ( -- [false/expr::core_parse_rec() true] )
void core_parse_rec(void) {
    core_parse();               // [false/nil/expr true]
    if (TOS == FALSE) return;   // false
    if (TOS == NIL) {
        push(TRUE);             // nil true
        return;
    }
    core_drop();                // expr
    core_parse_rec();           // expr [false/exprs true]
    if (TOS == FALSE) {
        core_nip();             // false
        return;
    }
    core_drop();                // expr exprs
    core_cons();                // expr::exprs
    push(TRUE);                 // expr::exprs true
}

// ( -- [false/nil/expr true] )
// When called from the top level, TOS can have the following values:
//      true    -- everything is OK, NOS contains expression
//      false   -- unexpected EOF or syntax error
//      nil     -- unexpected )
void core_parse(void) {
    core_token();               // token
    if (TOS == NIL) {           // nil
        TOS = FALSE;            // false
        return;
    }

    native_symbol *token = obj_binary_ptr(TOS);
    if (!strcmp(token->x, "(")) {
        core_drop();
        core_parse_rec();       // [false/expr true]
    } else if (!strcmp(token->x, ")")) {
        core_drop();
        push(NIL);              // nil
    } else {
        core_atom();            // atom
        push(TRUE);             // atom true
    }
}

void print_expr(obj *o) {
    if (o == NIL) printf("()");
    else if (o == TRUE) printf("<true>");
    else if (o == FALSE) printf("<false>");
    else {
        int first = 1;
        switch (obj_type(o)) {
            case TYPE_SYMBOL:
                printf("%s", ((native_symbol*)obj_binary_ptr(o))->x);
                break;
            case TYPE_STRING:
                printf("\"%s\"", ((native_symbol*)obj_binary_ptr(o))->x);
                break;
            case TYPE_CONS:
                putchar('(');
                while (o != NIL) {
                    if (obj_type(TAIL(o)) != TYPE_CONS &&
                        obj_type(TAIL(o)) != TYPE_NIL)
                    {
                        putchar('<');
                        print_expr(HEAD(o));
                        putchar(' ');
                        print_expr(TAIL(o));
                        putchar('>');
                        break;
                    }
                    if (! first) putchar(' ');
                    print_expr(HEAD(o));
                    first = 0;
                    o = TAIL(o);
                }
                putchar(')');
                break;
            case TYPE_INTEGER:
                printf("%" PRId64 "", ((native_integer*)obj_binary_ptr(o))->x);
                break;
            case TYPE_REAL:
                printf("%g", ((native_real*)obj_binary_ptr(o))->x);
                break;
            case TYPE_LAMBDA:
                putchar('\\');
                print_expr(o->ref[0]);
                putchar('.');
                print_expr(o->ref[1]);
                break;
            default:
                printf("<atom:%d>", obj_type(o));
                break;
        }
    }
}

// ( expr -- nil )
void core_print(void) {
    print_expr(pop()); putchar('\n');
    push(NIL);
}

// ( env expr1 -- expr2 )
void eval(void) {
    native_type expr_type = obj_type(TOS);
    if (expr_type == TYPE_SYMBOL) {
        core_swap();
        core_over();
        core_lookup();
        if (TOS == FALSE) {
            error(1, 0, "Unknown symbol: \"%s\"",
                    ((native_symbol*)obj_binary_ptr(NOS))->x);
        }
        core_drop();
        core_nip();
        return;
    } else if (expr_type == TYPE_CONS) {
        native_symbol *sym = obj_binary_ptr(HEAD(TOS));
        if (sym->type == TYPE_SYMBOL && !strcmp(sym->x, "lambda")) {
                                // env lambda::vars::body::nil
            core_tail();        // env vars::body::nil
            core_dup();         // env vars::body::nil vars::body::nil
            core_head();        // env vars::body::nil vars
            core_swap();        // env vars vars::body::nil
            core_tail();        // env vars body::nil
            core_head();        // env vars body
            core_rot();         // vars body env
            core_lambda();      // lambda(vars,body,env)
            return;
        } else if (sym->type == TYPE_SYMBOL && !strcmp(sym->x, "quote")) {
            core_nip();
            TOS = HEAD(TAIL(TOS));
            return;
        } else if (sym->type == TYPE_SYMBOL && !strcmp(sym->x, "if")) {
                                // env if::cond::then::else::nil
            core_tail();        // env cond::then::else::nil
            core_over();
            core_over();
            core_head();        // env cond::then::else::nil env cond
            eval();             // env cond::then::else::nil true/false
            obj_assert_type(TOS, TYPE_BOOL);
            if (pop() == TRUE) {// env cond::then::else::nil
                core_tail();    // env then::else::nil
                core_head();    // env then
                eval();         // result
            } else {
                core_tail();    // env then::else::nil
                core_tail();    // env else::nil
                core_head();    // env else
                eval();         // result
            }
            return;
        } else if (sym->type == TYPE_SYMBOL && !strcmp(sym->x, "define")) {
                                // env define::var::exp::nil
            core_tail();        // env var::exp::nil
            core_dup();
            core_head();        // env var::exp::nil var
            core_swap();        // env var var::exp::nil
            core_tail();        // env var exp::nil
            core_head();        // env var exp
            core_swap();        // env exp var
            core_rot();         // exp var env
            core_rot();         // var env exp
            eval();             // var result
            push(GLOBAL);       // var result global
            core_rot();
            core_rot();         // global var result
            core_extend();      // (var result)::global
            GLOBAL = pop();
            push(NIL);
            return;
        } else if (sym->type == TYPE_LAMBDA) {
            core_swap();                // l::args env
            push(HEAD(NOS)->ref[2]);    // l::args env env'
            core_append();              // l::args env++env'
            core_swap();                // env++env' l::args

            push(HEAD(TOS)->ref[0]);    // env l::args vars
            push(TAIL(NOS));            // env l::args vars args
            push(PICK(3));              // env l::args vars args env
            while (NOS != NIL && NNOS != NIL) {
                push(PICK(4));          // env l::args vars args env env
                push(HEAD(NNOS));       // env l::args vars args env env arg
                eval();                 // env l::args vars args env res
                push(HEAD(PICK(3)));    // env l::args vars args env res var
                core_swap();            // env l::args vars args env var res
                core_extend();          // env l::args vars args env
                NOS = TAIL(NOS);
                NNOS = TAIL(NNOS);      // env l::args vars args env
            }
            core_nip();
            core_nip();                 // env l::args env'
            core_swap();
            core_head();                // env env' lambda(vars,body,env)
            TOS = TOS->ref[1];          // env env' body
            core_rot();
            core_drop();                // env' body
            eval();
        } else if (sym->type == TYPE_NATFUN) {
            core_decons();              // env natfun args
            size_t base = sptr+1;       // base: env natfun
            while (TOS != NIL) {        // env natfun args
                core_decons();          // env natfun arg args
                core_swap();
                push(stack[base+1]);    // env natfun args arg env
                core_swap();            // env natfun args env arg
                eval();                 // env natfun args res1
                core_swap();            // env natfun res1 [...] args
            }
            core_drop();                // env natfun res1 [...]
            push(stack[base]);          // env natfun res1 [...] natfun
            core_execute();             // env natfun retval
            core_nip();
            core_nip();                 // retval
        } else {
            core_over();                // env fun::args env
            push(HEAD(NOS));            // env fun::args env fun
            eval();                     // env fun::args fun'
            if (obj_type(TOS) != TYPE_LAMBDA && obj_type(TOS) != TYPE_NATFUN)
            {
                error(1, 0, "Trying to evaluate type %d\n", obj_type(TOS));
            }
            core_swap();
            core_tail();                // env fun' args
            core_cons();                // env fun'::args
            //printf("env = "); print_expr(NOS);
            //printf("\nexpr = "); print_expr(TOS);
            //putchar('\n');
            eval();
        }
        return;
    } else {
        core_nip();                 // expr1
        return;
    }
}

// ( -- map )
void core_global(void) {
    push(GLOBAL);
}

// ( map -- )
void core_setglobal(void) {
    GLOBAL = pop();
}

#define DEFINE_NATFUN(name,fun) \
    push(new_symbol(name)); \
    push(new_natfun(fun)); \
    core_extend();

static void initialize(void) {
    gc_create_heap(&main_heap, 0x1000, 0x100000);

    roots[ROOT_NIL]     = new_nil();
    roots[ROOT_TRUE]    = new_bool(1);
    roots[ROOT_FALSE]   = new_bool(0);
    roots[ROOT_GLOBAL]  = NIL;

    push(GLOBAL);
    DEFINE_NATFUN("cons",       core_cons);
    DEFINE_NATFUN("decons",     core_decons);
    DEFINE_NATFUN("head",       core_head);
    DEFINE_NATFUN("tail",       core_tail);
    DEFINE_NATFUN("++",         core_append);
    DEFINE_NATFUN("=",          core_eq);
    DEFINE_NATFUN("<",          core_lt);
    DEFINE_NATFUN("+",          core_plus);
    DEFINE_NATFUN("*",          core_mul);
    DEFINE_NATFUN("/",          core_div);
    DEFINE_NATFUN("neg",        core_neg);
    DEFINE_NATFUN("extend",     core_extend);
    DEFINE_NATFUN("lookup",     core_lookup);
    DEFINE_NATFUN("global",     core_global);
    DEFINE_NATFUN("global!",    core_setglobal);
    DEFINE_NATFUN("parse",      core_parse);
    DEFINE_NATFUN("eval",       eval);
    DEFINE_NATFUN("print",      core_print);
    DEFINE_NATFUN("swap",       core_swap);
    DEFINE_NATFUN("dup",        core_dup);
    DEFINE_NATFUN("drop",       core_drop);
    DEFINE_NATFUN("over",       core_over);
    DEFINE_NATFUN("nip",        core_nip);
    DEFINE_NATFUN("rot",        core_rot);
    DEFINE_NATFUN("execute",    core_execute);
    GLOBAL = pop();
}

int main(void) {
    initialize();

    for (;;) {
        core_parse();
        if (TOS == TRUE) {
            core_drop();
            push(GLOBAL);
            core_swap();
            eval();
            if (obj_type(TOS) == TYPE_NATFUN) {
                core_execute();
            }
            //printf("result: "); print_expr(pop()); putchar('\n');
        } else {
            if (!feof(stdin)) {
                printf("Error!\n");
            }
            break;
        }
    }

    return 0;
}

