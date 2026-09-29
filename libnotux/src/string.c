/*
 * libnotux — string functions
 */

#include <notux/libc.h>

size_t nx_strlen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

char *nx_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) {}
    return dst;
}

char *nx_strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char *nx_strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++)) {}
    return dst;
}

int nx_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int nx_strncmp(const char *a, const char *b, size_t n) {
    while (n > 0 && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *nx_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

char *nx_strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

char *nx_strstr(const char *hay, const char *needle) {
    if (!*needle) return (char *)hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)hay;
    }
    return NULL;
}

char *nx_strtok(char *s, const char *delim) {
    static char *save;
    if (!s) s = save;
    if (!s) return NULL;

    while (*s && nx_strchr(delim, (unsigned char)*s)) s++;
    if (!*s) { save = NULL; return NULL; }

    char *tok = s;
    while (*s && !nx_strchr(delim, (unsigned char)*s)) s++;
    if (*s) { *s = '\0'; save = s + 1; }
    else save = NULL;
    return tok;
}

long nx_strtol(const char *s, char **end, int base) {
    int neg = 0;
    long v = 0;
    if (!s) { if (end) *end = NULL; return 0; }
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (base == 0) {
        base = 10;
        if (*s == '0') {
            s++;
            if (*s == 'x' || *s == 'X') { base = 16; s++; }
            else base = 8;
        }
    }
    for (;;) {
        int d;
        char c = *s;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        s++;
    }
    if (end) *end = (char *)s;
    return neg ? -v : v;
}

static double parse_frac(const char **pp) {
    double frac = 0.0, scale = 0.1;
    const char *s = *pp;
    while (*s >= '0' && *s <= '9') {
        frac += (*s - '0') * scale;
        scale *= 0.1;
        s++;
    }
    *pp = s;
    return frac;
}

double nx_strtod(const char *s, char **end) {
    if (!s) { if (end) *end = NULL; return 0.0; }
    int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    double v = 0.0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10.0 + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        v += parse_frac(&s);
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        int e_neg = 0;
        if (*s == '-') { e_neg = 1; s++; }
        else if (*s == '+') s++;
        int e = 0;
        while (*s >= '0' && *s <= '9') { e = e * 10 + (*s - '0'); s++; }
        double scale = 1.0;
        for (int i = 0; i < e; i++) scale *= 10.0;
        v = e_neg ? v / scale : v * scale;
    }
    if (end) *end = (char *)s;
    return neg ? -v : v;
}

double nx_atof(const char *s) {
    return nx_strtod(s, NULL);
}