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

typedef struct {
    char vars[MAX_VARS][MAX_NAME];
    long vers[MAX_VARS];
    int len;
} State;

extern void tx_init();
extern void tx_deploy(const char *new_src);
extern void tx_volume_sync(State *in, State *out);
extern long tx_enter(char *rnames[], long rvers[], int rlen,
                     char *wnames[], long wvers[], int wlen);
extern void tx_commit(long sid);
extern void tx_revert(long sid);
extern void tx_state();
extern void tx_free();
