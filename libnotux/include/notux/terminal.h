#pragma once
#include <notux/libc.h>
#define term_puts(s)            nx_puts(s)
#define term_putc(c)            nx_putchar(c)
#define term_set_fg(c)          nx_term_set_fg(c)
#define term_set_bg(c)          nx_term_set_bg(c)
#define term_reset_color()      nx_term_reset_color()
#define term_readline(buf,n)    (nx_gets(buf,n) ? (int)nx_strlen(buf) : -1)
#define term_read_password(b,n) nx_term_read_password(b,n)
#define sched_sleep_ms(ms)      nx_sleep(ms)
#define num_to_str(n,buf,base)  /* implemented in libc */ \
    do { \
        uint64_t _v=(uint64_t)(n); \
        char *_b=(buf); int _i=0; \
        if(!_v){_b[0]='0';_b[1]='\0';break;} \
        char _t[24]; \
        while(_v){int _r=(int)(_v%(base));_t[_i++]=(char)(_r<10?'0'+_r:'A'+_r-10);_v/=(base);} \
        for(int _a=0,_c=_i-1;_a<_c;_a++,_c--){char _x=_t[_a];_t[_a]=_t[_c];_t[_c]=_x;} \
        for(int _j=0;_j<=_i;_j++)_b[_j]=_t[_j]; \
    } while(0)
#define str_to_num(s)           ((uint64_t)nx_strtol(s,NULL,10))
