/*
 * Notux OS — kernel-wide definitions
 * kernel/kernel.h
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NOTUX_VERSION "0.1.0-dev"

/* Kernel panic — never returns */
__attribute__((noreturn)) void kpanic(const char *msg);
void kprintf(const char *fmt, ...);

/* Memory helpers (implemented in heap.c) */
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);
void *kmemset(void *dst, int val, size_t n);
void *kmemcpy(void *dst, const void *src, size_t n);
int   kmemcmp(const void *a, const void *b, size_t n);
char *kstrdup(const char *s);

/* String helpers (implemented in vfs.c) */
size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrncpy(char *dst, const char *src, size_t n);
char  *kstrchr(const char *s, int c);

/* num_to_str: convert integer to decimal string */
static inline void num_to_str(uint64_t n, char *buf, int base) {
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    char tmp[24]; int i=0;
    while (n) { int r=(int)(n%base); tmp[i++]=(char)(r<10?'0'+r:'A'+r-10); n/=base; }
    tmp[i]='\0';
    /* reverse */
    for (int a=0,b=i-1;a<b;a++,b--){char t=tmp[a];tmp[a]=tmp[b];tmp[b]=t;}
    for (int j=0;j<=i;j++) buf[j]=tmp[j];
}

static inline uint64_t str_to_num(const char *s) {
    uint64_t v=0;
    while (*s>='0'&&*s<='9') { v=v*10+(*s-'0'); s++; }
    return v;
}

/* Process forward declaration */
typedef struct Process Process;
typedef int32_t pid_t;

/* Current process (set by scheduler) */
extern Process *current_proc;

/* proc_kill forward decl used in multiple files */
void proc_kill(Process *proc, int signal);
int  proc_kill_pid(pid_t pid, int signal);
void sched_yield(void);
void sched_sleep_ms(uint32_t ms);

/* First-boot application installer (apps_install.c) */
int apps_install_blob(void);
