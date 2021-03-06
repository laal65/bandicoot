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

/* system initialisation: signal disposition, errno etc. */
extern void sys_init(int log);

/* input/output */
static const int IO_STREAM = 0;
static const int IO_CHUNK = 1;

/* io->stop flags */
static const int IO_ERROR = 1;
static const int IO_CLOSE = 2;
static const int IO_TERM = 4;

struct IO {
    int fd;
    int stop;
    int (*read)(struct IO *io, void *buf, int size);
    int (*write)(struct IO *io, const void *buf, int size);
    void (*close)(struct IO *io);
};
typedef struct IO IO;

extern int sys_read(IO *io, void *buf, int size);
extern int sys_readn(IO *io, void *buf, int size);
extern int sys_write(IO *io, const void *buf, int size);
extern int sys_term(IO *io); /* sends 0 bytes (ie last chunk) */
extern void sys_close(IO *io);

/* filesystem */
static const int CREATE = 0x01;
static const int TRUNCATE = 0x02;
static const int READ = 0x04;
static const int WRITE = 0x08;

extern IO *sys_open(const char *path, int mode);
extern int sys_exists(const char *path);
extern void sys_move(const char *dest, const char *src);
extern void sys_cpy(const char *dest, const char *src);
extern void sys_remove(const char *path);
extern char *sys_load(const char *path);
extern char **sys_list(const char *dir, int *len);
extern int sys_empty(const char *dir);

/* [multi]proc */
static const char PROC_OK = 0x00;
static const char PROC_FAIL = 0x01;

extern int sys_exec(char *const argv[]);
extern int sys_kill(int pid);
extern char sys_wait(int pid);
extern void sys_sleep(int secs);
extern void sys_thread(void *(*fn)(void *arg), void *arg);
extern void sys_exit(char status);
extern void sys_die(const char *msg, ...);

/* networking */
extern void sys_address(char *result, int port);
extern IO *sys_socket(int *port);
extern IO *sys_accept(IO *socket, int chunked);
extern IO *sys_connect(const char *address, int chunked);
extern IO *sys_try_connect(const char *address, int chunked);
extern int sys_iready(IO *io, int millis);
extern void sys_proxy(IO *cio, int *ccnt, IO *pio, int *pcnt);

/* misc */
extern void sys_print(const char *msg, ...);
extern void sys_log(char module, const char *msg, ...);
extern long long sys_millis();
extern void sys_time(char *buf);

/* monitor */
typedef struct {
    int value;
    void *mutex;
    void *cond;
} Mon;

extern Mon *mon_new();
extern void mon_lock(Mon *m);
extern void mon_unlock(Mon *m);
extern void mon_wait(Mon *m, int ms);
extern void mon_signal(Mon *m);
extern void mon_free(Mon *m);
