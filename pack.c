/*
Copyright 2008-2010 Ostap Cherkashin
Copyright 2008-2010 Julius Chrobak

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
#include "array.h"
#include "string.h"
#include "memory.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"

static int valid_id(const char *id)
{
    int len = 1;
    while (*id != '\0') {
        if (!((*id >= 'a' && *id <= 'z') || (*id >= 'A'&& *id <= 'Z')) &&
            !(*id >= '0' && *id <= '9') &&
            !(*id == '_'))
            return 0;

        if (++len == MAX_NAME)
            return 0;

        id++;
    }

    return 1;
}

/*
FIXME: report pack errors
Rel *err = head_pack(...)
Rel *err = rel_pack_sep(...)

static Rel *head_pack(char *buf, char *names[], Head **out);
extern Rel *rel_pack_sep(char *buf, Head **res_h, TBuf *res_b);
*/
static Head *head_pack(char *buf, char *names[])
{
    char *attrs[MAX_ATTRS], *name_type[MAX_ATTRS];

    int attrs_len = str_split(buf, ",", attrs, MAX_ATTRS);
    if (attrs_len < 1)
        return NULL;

    Type types[MAX_ATTRS];
    char *copy[MAX_ATTRS];
    for (int i = 0; i < attrs_len; ++i) {
        int cnt = str_split(attrs[i], " :\t", name_type, MAX_ATTRS);
        if (cnt != 2 || !valid_id(name_type[0]))
            return NULL;

        int bad_type;
        types[i] = type_from_str(str_trim(name_type[1]), &bad_type);
        if (bad_type)
            return NULL;

        names[i] = copy[i] = str_trim(name_type[0]);
    }

    return head_new(copy, types, attrs_len);
}

extern TBuf *rel_pack_sep(char *buf, Head **res_h)
{
    Head *h = NULL;
    TBuf *tbuf = NULL, *res_tbuf = NULL;
    char **lines = NULL, *str_vals[MAX_ATTRS], *names[MAX_ATTRS];

    int len;
    lines = str_split_big(buf, "\n", &len);
    if (len < 1)
        goto failure;

    if ((h = head_pack(lines[0], names)) == NULL)
        goto failure;

    if (str_len(lines[len - 1]) == 0)
        len--;

    tbuf = tbuf_new();

    int i = 1;
    for (; i < len; ++i) {
        int attrs = str_split(lines[i], ",", str_vals, MAX_ATTRS);
        if (attrs != h->len)
            goto failure;

        int v_ints[attrs];
        double v_reals[attrs];
        long long v_longs[attrs];
        int v_int_cnt = 0, v_real_cnt = 0, v_long_cnt = 0;
        Value v[attrs];

        for (int j = 0; j < attrs; ++j) {
            int pos = 0; Type t;
            int res = head_attr(h, names[j], &pos, &t);
            if (!res)
                goto failure;

            int error = 0;
            if (t == Int) {
                v_ints[v_int_cnt] = str_int(str_vals[j], &error);
                v[pos] = val_new_int(&v_ints[v_int_cnt++]);
            } else if (t == Real) {
                v_reals[v_real_cnt] = str_real(str_vals[j], &error);
                v[pos] = val_new_real(&v_reals[v_real_cnt++]);
            } else if (t == Long) {
                v_longs[v_long_cnt] = str_long(str_vals[j], &error);
                v[pos] = val_new_long(&v_longs[v_long_cnt++]);
            } else if (t == String) {
                error = str_len(str_vals[j]) > MAX_STRING;
                v[pos] = val_new_str(str_vals[j]);
            }

            if (error)
                goto failure;
        }

        tbuf_add(tbuf, tuple_new(v, attrs));
    }

    *res_h = h;
    h = NULL;

    res_tbuf = tbuf;
    tbuf = NULL;

failure:
    if (lines != NULL)
        mem_free(lines);
    if (h != NULL)
        mem_free(h);
    if (tbuf != NULL) {
        Tuple *t;
        while ((t = tbuf_next(tbuf)) != NULL)
            tuple_free(t);

        mem_free(tbuf);
    }

    return res_tbuf;
}

static int head_unpack(char *dest, Head *h)
{
    int off = 0;
    for (int i = 0; i < h->len; ++i) {
        off += str_print(dest + off,
                         "%s %s",
                         h->names[i],
                         type_to_str(h->types[i]));

        if ((i + 1) != h->len)
            dest[off++] = ',';
    }
    dest[off++] = '\n';
    dest[off] = '\0';

    return off;
}

static int tuple_unpack(char *dest, Tuple *t, int len, Type type[])
{
    int off = 0;
    for (int i = 0; i < len; i++) {
        if (i != 0)
            dest[off++] = ',';

        Value v = tuple_attr(t, i);
        off += val_to_str(dest + off, v, type[i]);
    }

    dest[off] = '\0';

    return off;
}

extern int rel_unpack(Rel *r, char *buf, int size, int iteration)
{
    int off = 0;

    if (r == NULL || r->body == NULL)
        return off;

    if (iteration == 0)
        off = head_unpack(buf, r->head);

    int len = r->head->len;
    Type *types = r->head->types;

    int tsize = len * MAX_STRING + len;
    Tuple *t = NULL;
    while ((size - off) > tsize && (t = tbuf_next(r->body)) != NULL) {
        off += tuple_unpack(buf + off, t, len, types);
        tuple_free(t);

        buf[off++] = '\n';
    }

    buf[off] = '\0';

    if (t == NULL && r->body != NULL) {
        tbuf_free(r->body);
        r->body = NULL;
    }

    return off;
}
