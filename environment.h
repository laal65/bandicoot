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

typedef struct {
    char name[MAX_NAME];
    Head *ret;

    struct {
        int len;
        char *names[MAX_VARS];
    } r, w; /* global variables read and written by the function */

    struct {
        int len;
        char *names[MAX_VARS];
        Head *heads[MAX_VARS];
    } t; /* local (temporary) variables */

    struct {
        char *name;
        Head *head;
        int position;
    } rp; /* relational input parameter */

    struct {
        int len;
        char *names[MAX_ATTRS];
        Type types[MAX_ATTRS];
        int positions[MAX_ATTRS];
    } pp; /* primitive input parameters */

    int slen;
    Rel *stmts[MAX_STMTS];
} Func;

typedef struct {
    struct {
        int len;
        char *names[MAX_TYPES];
        Head *heads[MAX_TYPES];
    } vars, types;

    struct {
        int len;
        char *names[MAX_VARS];
        Func *funcs[MAX_VARS];
    } fns;
} Env;

extern Env *env_new(const char *path, const char *program);
extern void env_free(Env *env);

extern Func *env_func(Env *env, const char *name);
extern Func **env_funcs(Env *env, const char *name, int *cnt);
extern Head *env_head(Env *env, const char *var);
extern int env_compat(Env *old, Env *new);
