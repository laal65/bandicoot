/*
Copyright 2008-2010 Ostap Cherkashin
Copyright 2008-2012 Julius Chrobak

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "config.h"
#include "system.h"
#include "string.h"
#include "memory.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"

#define e_int(e) ((e)->val.v_int)
#define e_real(e) ((e)->val.v_real)
#define e_long(e) ((e)->val.v_long)
#define e_str(e) ((e)->val.v_str)

typedef struct {
    Expr *left;
    Expr *right;
    void (*op)(Expr *dest, Expr *l, Expr *r);
} C_Binary;

static Expr *do_eval(Expr *e, Tuple *t, Arg *arg)
{
    e->eval(e, t, arg);
    return e;
}

static void eval_noop(Expr *e, Tuple *t, Arg *arg) { }
static void free_noop(Expr *e) { }

static Expr *alloc(Type type,
                   int ctxt_size,
                   void (*eval)(Expr*, Tuple*, Arg*),
                   void (*free)(Expr*))
{
    Expr *res = mem_alloc(sizeof(Expr) + ctxt_size);
    res->type = type;
    res->ctxt = ctxt_size > 0 ? res + 1 : 0;
    res->eval = eval;
    res->free = free;

    return res;
}

extern Expr *expr_int(int val)
{
    Expr *res = alloc(Int, 0, eval_noop, free_noop);
    e_int(res) = val;
    return res;
}

extern Expr *expr_long(long long val)
{
    Expr *res = alloc(Long, 0, eval_noop, free_noop);
    e_long(res) = val;
    return res;
}

extern Expr *expr_real(double val)
{
    Expr *res = alloc(Real, 0, eval_noop, free_noop);
    e_real(res) = val;
    return res;
}

extern Expr *expr_str(const char *val)
{
    Expr *res = alloc(String, 0, eval_noop, free_noop);
    str_cpy(e_str(res), val);
    return res;
}

static void eval_attr(Expr *e, Tuple *t, Arg *arg)
{
    Value v = tuple_attr(t, *((int*) e->ctxt));
    if (e->type == Int)
        e_int(e) = val_int(v);
    else if (e->type == Real)
        e_real(e) = val_real(v);
    else if (e->type == Long)
        e_long(e) = val_long(v);
    else if (e->type == String)
        str_cpy(e_str(e), val_str(v));
}

extern Expr *expr_attr(int pos, Type type)
{
    Expr *res = alloc(type, sizeof(int), eval_attr, free_noop);
    *((int*) res->ctxt) = pos;
    return res;
}

static void eval_param(Expr *e, Tuple *t, Arg *arg)
{
    int i = *((int*) e->ctxt);
    if (e->type == Int)
        e_int(e) = arg->vals[i].v_int;
    else if (e->type == Real)
        e_real(e) = arg->vals[i].v_real;
    else if (e->type == Long)
        e_long(e) = arg->vals[i].v_long;
    else if (e->type == String)
        str_cpy(e_str(e), arg->vals[i].v_str);
}

extern Expr *expr_param(int pos, Type type)
{
    Expr *res = alloc(type, sizeof(int), eval_param, free_noop);
    *((int*) res->ctxt) = pos;
    return res;
}

static void eval_not(Expr *e, Tuple *t, Arg *arg)
{
    e_int(e) = !e_int(do_eval(e->ctxt, t, arg));
}

static void free_unary(Expr *e)
{
    expr_free(e->ctxt);
}

extern Expr *expr_not(Expr *e)
{
    Expr *res = alloc(Int, 0, eval_not, free_unary);
    res->ctxt = e;
    return res;
}

static void eval_binary(Expr *e, Tuple *t, Arg *arg)
{
    C_Binary *c = e->ctxt;
    c->op(e, do_eval(c->left, t, arg), do_eval(c->right, t, arg));
}

static void free_binary(Expr *e)
{
    C_Binary *c = e->ctxt;
    expr_free(c->left);
    expr_free(c->right);
}

static Expr *expr_binary(Type type,
                         Expr *left,
                         Expr *right,
                         void (*op)(Expr *dest, Expr *l, Expr *r))
{
    Expr *res = alloc(type, sizeof(C_Binary), eval_binary, free_binary);

    C_Binary *c = res->ctxt;
    c->left = left;
    c->right = right;
    c->op = op;

    return res;
}

static void op_or(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = e_int(l) || e_int(r);
}

extern Expr *expr_or(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_or);
}

static void op_and(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = e_int(l) && e_int(r);
}

extern Expr *expr_and(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_and);
}

static int cmp(Expr *l, Expr *r)
{
    int res;
    switch (l->type) {
        case Int:
            if (e_int(l) == e_int(r))
                res = 0;
            else
                res = e_int(l) > e_int(r) ? 1 : -1;
            break;
        case Real:
            if (e_real(l) == e_real(r))
                res = 0;
            else
                res = e_real(l) > e_real(r) ? 1 : -1;
            break;
        case Long:
            if (e_long(l) == e_long(r))
                res = 0;
            else
                res = e_long(l) > e_long(r) ? 1 : -1;
            break;
        default:
            res = str_cmp(e_str(l), e_str(r));
    }

    return res;
}

static void op_eq(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = cmp(l, r) == 0;
}

extern Expr *expr_eq(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_eq);
}

static void op_lt(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = cmp(l, r) < 0;
}

extern Expr *expr_lt(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_lt);
}

static void op_lte(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = cmp(l, r) <= 0;
}

extern Expr *expr_lte(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_lte);
}

static void op_gt(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = cmp(l, r) > 0;
}

extern Expr *expr_gt(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_gt);
}

static void op_gte(Expr *dest, Expr *l, Expr *r)
{
    e_int(dest) = cmp(l, r) >= 0;
}

extern Expr *expr_gte(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_gte);
}

static void op_sum(Expr *dest, Expr *l, Expr *r)
{
    if (l->type == Int)
        e_int(dest) = e_int(l) + e_int(r);
    else if (l->type == Long)
        e_long(dest) = e_long(l) + e_long(r);
    else
        e_real(dest) = e_real(l) + e_real(r);
}

extern Expr *expr_sum(Expr *l, Expr *r)
{
    return expr_binary(l->type, l, r, op_sum);
}

static void op_sub(Expr *dest, Expr *l, Expr *r)
{
    if (l->type == Int)
        e_int(dest) = e_int(l) - e_int(r);
    else if (l->type == Long)
        e_long(dest) = e_long(l) - e_long(r);
    else
        e_real(dest) = e_real(l) - e_real(r);
}

extern Expr *expr_sub(Expr *l, Expr *r)
{
    return expr_binary(l->type, l, r, op_sub);
}

static void op_div(Expr *dest, Expr *l, Expr *r)
{
    if (l->type == Int)
        e_int(dest) = e_int(l) / e_int(r);
    else if (l->type == Long)
        e_long(dest) = e_long(l) / e_long(r);
    else
        e_real(dest) = e_real(l) / e_real(r);
}

extern Expr *expr_div(Expr *l, Expr *r)
{
    return expr_binary(l->type, l, r, op_div);
}

static void op_mul(Expr *dest, Expr *l, Expr *r)
{
    if (l->type == Int)
        e_int(dest) = e_int(l) * e_int(r);
    else if (l->type == Long)
        e_long(dest) = e_long(l) * e_long(r);
    else
        e_real(dest) = e_real(l) * e_real(r);
}

extern Expr *expr_mul(Expr *l, Expr *r)
{
    return expr_binary(l->type, l, r, op_mul);
}

static void eval_to_int(Expr *dest, Tuple *t, Arg *arg)
{
    Expr *src = do_eval(dest->ctxt, t, arg);
    if (src->type == Int)
        e_int(dest) = e_int(src);
    else if (src->type == Real)
        e_int(dest) = (int) e_real(src);
    else if (src->type == Long)
        e_int(dest) = (int) e_long(src);
}

static void eval_to_real(Expr *dest, Tuple *t, Arg *arg)
{
    Expr *src = do_eval(dest->ctxt, t, arg);
    if (src->type == Int)
        e_real(dest) = (double) e_int(src);
    else if (src->type == Real)
        e_real(dest) = e_real(src);
    else if (src->type == Long)
        e_real(dest) = (double) e_long(src);
}

static void eval_to_long(Expr *dest, Tuple *t, Arg *arg)
{
    Expr *src = do_eval(dest->ctxt, t, arg);
    if (src->type == Int)
        e_long(dest) = e_int(src);
    else if (src->type == Real)
        e_long(dest) = (long long) e_real(src);
    else if (src->type == Long)
        e_long(dest) = e_long(src);
}

/* TODO: implement to/from string conversions and string concatenation */
extern Expr *expr_conv(Expr *e, Type t)
{
    void (*eval)(Expr *self, Tuple *t, Arg *arg) = NULL;
    if (t == Int)
        eval = eval_to_int;
    else if (t == Real)
        eval = eval_to_real;
    else if (t == Long)
        eval = eval_to_long;

    Expr *res = alloc(t, 0, eval, free_unary);
    res->ctxt = e;

    return res;
}

static void eval_time(Expr *e, Tuple *t, Arg *arg)
{
    e_long(e) = sys_millis();
}

extern Expr *expr_time(long long val)
{
    Expr *res = alloc(Long, 0, eval_time, free_noop);
    return res;
}

static void op_str_index(Expr *dest, Expr *l, Expr *r)
{
    char *lstr = e_str(l);
    char *rstr = e_str(r);

    e_int(dest) = (int) str_idx(lstr, rstr);
}

extern Expr *expr_str_index(Expr *l, Expr *r)
{
    return expr_binary(Int, l, r, op_str_index);
}

extern int expr_bool_val(Expr *e, Tuple *t, Arg *arg)
{
    return e_int(do_eval(e, t, arg));
}

extern Value expr_new_val(Expr *e, Tuple *t, Arg *arg)
{
    do_eval(e, t, arg);

    Value v = {.size = 0, .data = NULL };
    if (e->type == Int)
        v = val_new_int(&(e_int(e)));
    else if (e->type == Real)
        v = val_new_real(&(e_real(e)));
    else if (e->type == Long)
        v = val_new_long(&(e_long(e)));
    else if (e->type == String)
        v = val_new_str(e_str(e));

    return v;
}

extern void expr_free(Expr *e)
{
    e->free(e);
    mem_free(e);
}
