/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2011 Julius Chrobak

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

#include "common.h"

static Vars *vars = NULL;
static Vars *rvars = NULL;
static Env *env = NULL;
static Arg arg;

static Rel *pack(char *str, char *names[], Type types[], int len)
{
    char *n[len];
    Type t[len];
    for (int i = 0; i < len; ++i) {
        n[i] = names[i];
        t[i] = types[i];
    }

    Head *head = head_new(n, t, len);
    TBuf *body = NULL;
    Error *err = pack_csv2rel(str, head, &body);
    if (err != NULL)
        fail();

    Rel *res = NULL;
    if (body != NULL) {
        int idx = array_scan(vars->names, vars->len, "___param");
        if (idx < 0)
            fail();

        res = rel_load(head, "___param");
        vars->vals[idx] = body;
    }

    mem_free(head);
    return res;
}

static Rel *load(const char *name)
{
    Head *head = env_head(env, name);
    return rel_load(head, name);
}

static void load_vars()
{
    for (int i = 0; i < rvars->len; ++i) {
        int pos = array_scan(vars->names, vars->len, rvars->names[i]);
        if (pos < 0)
            fail();

        vars->vals[pos] = vol_read(rvars->vols[i],
                                   rvars->names[i],
                                   rvars->vers[i]);
    }
}

static void free_vars()
{
    for (int i = 0; i < vars->len; ++i)
        if (vars->vals[i] != NULL) {
            tbuf_clean(vars->vals[i]);
            tbuf_free(vars->vals[i]);
            vars->vals[i] = NULL;
        }
}

static int equal(Rel *left, const char *name)
{
    Rel *right = load(name);
    Vars *wvars = vars_new(0);

    long long sid = tx_enter("", rvars, wvars);
    load_vars();

    rel_eval(left, vars, &arg);
    rel_eval(right, vars, &arg);

    int res = rel_eq(left, right);

    rel_free(left);
    rel_free(right);
    free_vars();

    tx_commit(sid);

    vars_free(wvars);

    return res;
}

static int count(const char *name)
{
    Rel *r = load(name);
    Vars *wvars = vars_new(0);

    long long sid = tx_enter("", rvars, wvars);
    load_vars();

    rel_eval(r, vars, &arg);

    Tuple *t;
    int i = 0;
    while ((t = tbuf_next(r->body)) != NULL) {
        tuple_free(t);
        i++;
    }

    rel_free(r);
    free_vars();

    tx_commit(sid);

    vars_free(wvars);

    return i;
}

static void test_load()
{
    if (count("empty_r1") != 0)
        fail();
    if (count("one_r1") != 1)
        fail();
    if (count("two_r2") != 2)
        fail();
}

static void test_param()
{
    char *names[] = {"a", "c"};
    Type types[] = {Int, String};
    char param_1[1024];
    /* FIXME: pack should be unique
    str_cpy(param_1, "a,c\n1,one\n2,two\n1,one\n2,two");
     */
    str_cpy(param_1, "a,c\n1,one\n2,two");

    if (!equal(pack(param_1, names, types, 2), "param_1"))
        fail();
}

static int equal_clone(const char *name)
{
    Rel *cp1 = load(name);
    Rel *cp2 = load(name);
    Vars *wvars = vars_new(0);

    long long sid = tx_enter("", rvars, wvars);
    load_vars();

    rel_eval(cp1, vars, &arg);
    rel_eval(cp2, vars, &arg);

    int res = rel_eq(cp1, cp2);

    rel_free(cp1);
    rel_free(cp2);
    free_vars();

    tx_commit(sid);

    vars_free(wvars);

    return res;
}

static void test_clone()
{
    if (!equal_clone("empty_r1"))
        fail();

    if (!equal_clone("storage_r1"))
        fail();
}

static void test_eq()
{
    if (equal(load("empty_r1"), "empty_r2"))
        fail();
    if (!equal(load("empty_r1"), "empty_r1"))
        fail();
    if (!equal(load("empty_r2"), "empty_r2"))
        fail();
    if (!equal(load("equal_r1"), "equal_r1"))
        fail();
    if (equal(load("equal_r1"), "equal_r2"))
        fail();
    if (!equal(load("equal_r1"), "equal_r3"))
        fail();
    if (equal(load("equal_r2"), "equal_r3"))
        fail();
}

static void test_store()
{
    Rel *r = load("one_r1");

    Vars *wvars = vars_new(1);
    vars_add(wvars, "one_r1_cpy", 0, NULL);

    long long sid = tx_enter("", rvars, wvars);

    load_vars();
    rel_eval(r, vars, &arg);
    vol_write(wvars->vols[0], r->body, wvars->names[0], wvars->vers[0]);
    rel_free(r);
    free_vars();

    tx_commit(sid);

    vars_free(wvars);
    if (!equal(load("one_r1"), "one_r1_cpy"))
        fail();

    /* TODO: test reassignment in one statement logic */
}

static void test_select()
{
    int a, b, c;
    Type ta, tb, tc;

    Rel *src = load("select_1");
    head_attr(src->head, "b", &b, &tb);
    Expr *expr = expr_eq(expr_real(1.01), expr_attr(b, tb));
    Rel *r = rel_select(src, expr);

    if (!equal(r, "select_1_res"))
        fail();

    src = load("select_2");
    head_attr(src->head, "a", &a, &ta);
    head_attr(src->head, "b", &b, &tb);
    head_attr(src->head, "c", &c, &tc);
    expr = expr_or(expr_gt(expr_attr(b, tb),
                           expr_real(4.00)),
                   expr_lt(expr_attr(a, ta),
                           expr_int(2)));
    expr = expr_or(expr,
                   expr_lt(expr_attr(c, tc),
                           expr_str("aab")));
    r = rel_select(src, expr);
    if (!equal(r, "select_2_res"))
        fail();

    src = load("select_3");
    head_attr(src->head, "a", &a, &ta);
    head_attr(src->head, "b", &b, &tb);
    expr = expr_and(expr_not(expr_attr(a, ta)),
                    expr_not(expr_eq(expr_attr(b, tb),
                                     expr_real(1.01))));
    r = rel_select(src, expr);
    
    if (!equal(r, "select_3_res"))
        fail();
}

static void test_rename()
{
    char *from[] = {"c", "a", "b"};
    char *to[] = {"new_name", "new_id", "new_some"};

    Rel *r = rel_rename(load("rename_1"), from, to, 3);

    if (!equal(r, "rename_1_res"))
        fail();
}

static void test_extend()
{
    char *names[] = {"d"};
    Expr *exprs[] = {expr_false()};

    Rel *r = rel_extend(load("extend_1"), names, exprs, 1);

    if (!equal(r, "extend_res_1"))
        fail();
}

static void test_join()
{
    Rel *join = rel_join(load("join_1_r1"), load("join_1_r2"));
    if (!equal(join, "join_1_res"))
        fail();

    join = rel_join(load("join_2_r1"), load("join_2_r2"));
    if (!equal(join, "join_2_res"))
        fail();
}

static void test_project()
{
    char *names[] = {"b", "c"};

    Rel *prj = rel_project(load("project_1"), names, 2);
    if (!equal(prj, "project_1_res"))
        fail();

    prj = rel_project(load("project_2"), names, 2);
    if (!equal(prj, "project_2_res"))
        fail();
}

static void test_semidiff()
{
    Rel *sem = rel_diff(load("semidiff_1_l"), load("semidiff_1_r"));
    if (!equal(sem, "semidiff_1_res"))
        fail();

    sem = rel_diff(load("semidiff_2_l"), load("semidiff_2_r"));
    if (!equal(sem, "semidiff_2_res"))
        fail();
}

static void test_summary()
{
    int int_zero = 0, int_minus_one = -1;
    double real_plus_one = 1.0, real_zero = 0.0, real_minus_decimal = -0.99;
    long long long_minus_one = -1, long_zero = 0;

    char *names[] = {"employees", 
                     "min_age", "max_age", "avg_age", "add_age",
                     "min_salary", "max_salary", "avg_salary", "add_salary",
                     "min_birth", "max_birth", "avg_birth", "add_birth"};
    Type types[] =  {Int,
                     Int, Int, Real, Int,
                     Real, Real, Real, Real,
                     Long, Long, Real, Long};

    Rel *r = load("summary_emp");
    int pos1, pos2, pos3;
    Type tp1, tp2, tp3;
    head_attr(r->head, "age", &pos1, &tp1);
    head_attr(r->head, "salary", &pos2, &tp2);
    head_attr(r->head, "birth", &pos3, &tp3);
    Sum *sums[] = {sum_cnt(),
                   /* age calcs */
                   sum_min(pos1, tp1, val_new_int(&int_minus_one)),
                   sum_max(pos1, tp1, val_new_int(&int_minus_one)),
                   sum_avg(pos1, tp1, val_new_real(&real_plus_one)),
                   sum_add(pos1, tp1, val_new_int(&int_minus_one)),
                   /* salary calcs */
                   sum_min(pos2, tp2, val_new_real(&real_zero)),
                   sum_max(pos2, tp2, val_new_real(&real_zero)),
                   sum_avg(pos2, tp2, val_new_real(&real_zero)),
                   sum_add(pos2, tp2, val_new_real(&real_zero)),
                   /* birth calcs */
                   sum_min(pos3, tp3, val_new_long(&long_minus_one)),
                   sum_max(pos3, tp3, val_new_long(&long_zero)),
                   sum_avg(pos3, tp3, val_new_real(&real_minus_decimal)),
                   sum_add(pos3, tp3, val_new_long(&long_zero))
                   };

    Rel *sum = rel_sum(r, load("summary_dep"), names, types, sums, 13);
    if (!equal(sum, "summary_res_1"))
        fail();

    char *unary_names[] = {"min_age", "max_age", "min_salary",
                           "max_salary", "count", "add_salary"};
    Type unary_types[] = {Int, Int, Real, Real, Int, Real};

    r = load("summary_emp");
    Sum *unary_sums[] = {sum_min(pos1, tp1, val_new_int(&int_zero)),
                         sum_max(pos1, tp1, val_new_int(&int_zero)),
                         sum_min(pos2, tp2, val_new_real(&real_zero)),
                         sum_max(pos2, tp2, val_new_real(&real_zero)),
                         sum_cnt(),
                         sum_add(pos2, tp2, val_new_real(&real_zero))};

    sum = rel_sum_unary(r, unary_names, unary_types, unary_sums, 6);
    if (!equal(sum, "summary_res_2"))
        fail();
}

static void test_union()
{
    Rel *un = rel_union(load("union_1_l"), load("union_1_r"));
    if (!equal(un, "union_1_res"))
        fail();

    un = rel_union(load("union_1_r"), load("union_1_l"));
    if (!equal(un, "union_1_res"))
        fail();

    un = rel_union(load("union_2_l"), load("union_2_r"));
    if (!equal(un, "union_2_res"))
        fail();

    un = rel_union(load("union_2_r"), load("union_2_l"));
    if (!equal(un, "union_2_res"))
        fail();
}

static void test_compound()
{
    char *rn_from[] = {"a2"};
    char *rn_to[] = {"a"};
    char *pn[] = {"a2", "c"};

    Rel *tmp = load("compound_1");

    int a;
    Type ta;
    head_attr(tmp->head, rn_to[0], &a, &ta);

    Expr *e[] = { expr_mul(expr_int(10), expr_attr(a, Int)) };
    tmp = rel_extend(tmp, rn_from, e, 1);
    tmp = rel_project(tmp, pn, 2);
    tmp = rel_rename(tmp, rn_from, rn_to, 1);
    tmp = rel_union(tmp, load("compound_1"));

    if (!equal(tmp, "compound_1_res"))
        fail();

    /* compound with a parameter */
    char *names[] = {"car", "date", "email", "name", "phone", "pos", "when"};
    Type types[] = {String, String, String, String, String, Int, Int};
    char union_2_l[1024];
    str_cpy(union_2_l,
            "car,date,email,name,phone,pos,when\n"
            "myCar,28-Jun-2010,myEmail,myName,myPhone,2,14");

    if (!equal(rel_union(pack(union_2_l, names, types, 7),
                         load("union_2_r")),
               "union_2_res"))
        fail();

    str_cpy(union_2_l,
            "car,date,email,name,phone,pos,when\n"
            "myCar,28-Jun-2010,myEmail,myName,myPhone,2,14");
    if (!equal(rel_union(load("union_2_r"),
                         pack(union_2_l, names, types, 7)),
               "union_2_res"))
        fail();
}

static void check_vars(Vars *v)
{
    if (v == NULL || v->len != 3)
        fail();

    if (str_cmp(v->names[0], "a1") != 0 || v->vers[0] != 1)
        fail();
    if (str_cmp(v->names[1], "a2") != 0 || v->vers[1] != 2)
        fail();
    if (str_cmp(v->names[2], "a3") != 0 || v->vers[2] != 3)
        fail();

    for (int i = 0; i < 3; ++i) {
        char exp[MAX_NAME];
        str_print(exp, "%d", i + 3);

        if (str_cmp(v->vols[i], exp) != 0)
            fail();
    }
}

static void test_vars()
{
    Vars *v[] = {vars_new(0), vars_new(1), vars_new(3), vars_new(7)};

    for (unsigned i = 0; i < sizeof(v) / sizeof(Vars*); ++i) {
        vars_add(v[i], "a1", 1, NULL);
        vars_add(v[i], "a2", 2, NULL);
        vars_add(v[i], "a3", 3, NULL);
        for (int j = 0; j < 3; ++j)
            str_print(v[i]->vols[j], "%d", j + 3);

        check_vars(v[i]);

        Vars *c = vars_new(0);
        vars_cpy(c, v[i]);
        check_vars(c);

        const char *file = "bin/relation_vars_test";
        IO *io = sys_open(file, CREATE | WRITE);
        if (vars_write(v[i], io) < 0)
            fail();
        sys_close(io);
        io = sys_open(file, READ);
        Vars *out = vars_read(io);
        if (out == NULL)
            fail();

        check_vars(out);

        vars_free(v[i]);
        vars_free(out);
        vars_free(c);

        sys_close(io);
    }
}

static void test_call()
{
    char *r[] = { "one_r1" };
    char *w[] = { };
    char *t[] = { "___param" };

    /* simulate a function call whih ignores the return value of a function
       see stmt_call for implementation details */
    Head *head = env_head(env, "one_r1");

    Rel *stmts[] = { rel_store("___param", rel_load(head, "one_r1")),
                     rel_store("___param", rel_load(head, "one_r1")) };

    Rel *fn = rel_call(r, 1, w, 0, t, 1,
                       stmts, 2,
                       NULL, 0,
                       NULL, "",
                       NULL);

    Vars *wvars = vars_new(0);
    long long sid = tx_enter("", rvars, wvars);
    vars_free(wvars);

    load_vars();

    rel_eval(fn, vars, &arg);
    rel_free(fn);
    rel_free(stmts[0]);
    rel_free(stmts[1]);
    free_vars();

    tx_commit(sid);
}

int main()
{
    int tx_port = 0;
    char *source = "test/test_defs.b";

    sys_init(0);
    tx_server(source, "bin/state", &tx_port);
    vol_init(0, "bin/volume");

    char *code = sys_load(source);
    env = env_new(source, code);
    mem_free(code);

    int len = 0;
    char **files = sys_list("test/data", &len);

    vars = vars_new(len);
    rvars = vars_new(len);
    for (int i = 0; i < len; ++i) {
        vars_add(rvars, files[i], 0, NULL);
        vars_add(vars, files[i], 0, NULL);
    }
    vars_add(vars, "___param", 0, NULL);

    test_vars();
    test_load();
    test_param();
    test_clone();
    test_eq();
    test_store();
    test_select();
    test_rename();
    test_extend();
    test_join();
    test_project();
    test_semidiff();
    test_summary();
    test_union();
    test_compound();
    test_call();

    tx_free();
    env_free(env);
    mem_free(files);
    vars_free(vars);
    vars_free(rvars);

    return 0;
}
