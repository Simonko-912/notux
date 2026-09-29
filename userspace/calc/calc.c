/*
 * calc.c — Notux calculator.
 *
 * Full-featured:
 *   - arithmetic with precedence:  + - * / ^
 *   - parentheses
 *   - functions: sqrt, abs, pow, sin, cos, log, log2, floor, ceil, min, max
 *   - integer ops: %, rand, seed
 *   - interactive REPL and one-shot mode:  calc "2+3*4"
 *   - --help
 */

#include <notux/libc.h>
#include <notux/proc.h>
#include <notux/terminal.h>
#include <notux/color.h>

#define CYAN   color_rgb(0,   240, 220)
#define YELLOW color_rgb(255, 220, 80)
#define WHITE  color_rgb(255, 255, 255)
#define RED    color_rgb(255, 90,  90)
#define GREEN  color_rgb(80,  255, 120)

static const char *g_p;   /* cursor into expression */
static int         g_err;

static double parse_expr(void);
static double parse_term(void);
static double parse_factor(void);
static double parse_power(void);

static void skip_ws(void) { while (*g_p == ' ' || *g_p == '\t') g_p++; }

static double parse_number(void) {
    skip_ws();
    char *end = NULL;
    double v = nx_strtod(g_p, &end);
    if (end == g_p) { g_err = 1; return 0.0; }
    g_p = end;
    return v;
}

static double parse_factor(void) {
    skip_ws();
    if (g_err) return 0.0;

    if (*g_p == '-') { g_p++; return -parse_factor(); }
    if (*g_p == '+') { g_p++; return  parse_factor(); }
    if (*g_p == '(') {
        g_p++;
        double v = parse_expr();
        skip_ws();
        if (*g_p == ')') g_p++; else g_err = 1;
        return v;
    }
    if ((*g_p >= '0' && *g_p <= '9') || *g_p == '.')
        return parse_number();

    /* function / constant call */
    char name[16];
    int n = 0;
    while ((*g_p >= 'a' && *g_p <= 'z') || (*g_p >= 'A' && *g_p <= 'Z')) {
        if (n < 15) name[n++] = *g_p;
        g_p++;
    }
    name[n] = '\0';
    if (!n) { g_err = 1; return 0.0; }

    if (nx_strcmp(name, "pi") == 0) return 3.141592653589793;
    if (nx_strcmp(name, "e")  == 0) return 2.718281828459045;

    skip_ws();
    if (*g_p == '(') {
        g_p++;
        double a = parse_expr();
        skip_ws();
        if (*g_p == ',') {
            g_p++;
            double b = parse_expr();
            skip_ws();
            if (*g_p == ')') g_p++; else g_err = 1;
            if (nx_strcmp(name, "pow") == 0)  return nx_pow(a, b);
            if (nx_strcmp(name, "min") == 0)  return (double)nx_min((int64_t)a, (int64_t)b);
            if (nx_strcmp(name, "max") == 0)  return (double)nx_max((int64_t)a, (int64_t)b);
            g_err = 1;
            return 0.0;
        }
        if (*g_p == ')') g_p++; else g_err = 1;
        if (nx_strcmp(name, "sqrt") == 0) return nx_sqrt(a);
        if (nx_strcmp(name, "abs")  == 0) return (double)nx_abs((int64_t)a);
        if (nx_strcmp(name, "sin")  == 0) return nx_sin(a);
        if (nx_strcmp(name, "cos")  == 0) return nx_cos(a);
        if (nx_strcmp(name, "log")  == 0) return nx_log(a);
        if (nx_strcmp(name, "log2") == 0) return nx_log2(a);
        if (nx_strcmp(name, "floor")== 0) return nx_floor(a);
        if (nx_strcmp(name, "ceil") == 0) return nx_ceil(a);
        g_err = 1;
        return 0.0;
    }
    g_err = 1;
    return 0.0;
}

static double parse_power(void) {
    skip_ws();
    double base = parse_factor();
    skip_ws();
    if (*g_p == '^') {
        g_p++;
        double e = parse_power();   /* right-assoc */
        return nx_pow(base, e);
    }
    return base;
}

static double parse_term(void) {
    double v = parse_power();
    for (;;) {
        skip_ws();
        if (*g_p == '*') { g_p++; v *= parse_power(); }
        else if (*g_p == '/') {
            g_p++;
            double d = parse_power();
            if (d == 0.0) { g_err = 1; return v; }
            v /= d;
        } else if (*g_p == '%') {
            g_p++;
            long d = (long)parse_power();
            if (d == 0) { g_err = 1; return v; }
            v = (double)((long)v % d);
        } else break;
    }
    return v;
}

static double parse_expr(void) {
    double v = parse_term();
    for (;;) {
        skip_ws();
        if (*g_p == '+') { g_p++; v += parse_term(); }
        else if (*g_p == '-') { g_p++; v -= parse_term(); }
        else break;
    }
    return v;
}

static double eval_expr(const char *s, int *err) {
    g_p   = s;
    g_err = 0;
    double v = parse_expr();
    skip_ws();
    if (*g_p != '\0') g_err = 1;
    *err = g_err;
    return v;
}

static void banner(void) {
    nx_cprintf(CYAN, 0, "Notux calculator  (e.g. 4+5, 7*8, 10/2, 2^10, sqrt(64))\n");
    nx_cprintf(YELLOW, 0, "functions: sqrt abs pow sin cos log log2 floor ceil min max rand\n");
    nx_cprintf(YELLOW, 0, "constants: pi e    command: seed <n>  reset  quit\n");
}

int main(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc > 1 && (nx_strcmp(argv[1], "--help") == 0 ||
                     nx_strcmp(argv[1], "-h") == 0)) {
        banner();
        proc_exit(0);
        return 0;
    }

    if (argc > 1) {
        int err = 0;
        double r = eval_expr(argv[1], &err);
        char out[64];
        if (err) {
            nx_cprintf(RED, 0, "error: bad expression: %s\n", argv[1]);
        } else {
            nx_sprintf(out, "%.6f", r);
            /* trim trailing zeros */
            char *dot = nx_strchr(out, '.');
            char *e = out + nx_strlen(out) - 1;
            while (dot && e > dot && *e == '0') { *e = '\0'; e--; }
            if (dot && e == dot) *e = '\0';
            nx_cprintf(GREEN, 0, "= %s\n", out);
        }
        proc_exit(err ? 1 : 0);
        return 0;
    }

    banner();
    char line[128];
    for (;;) {
        nx_cprintf(CYAN, 0, "\ncalc> ");
        if (!nx_gets(line, sizeof(line))) break;

        if (nx_strcmp(line, "quit") == 0 || nx_strcmp(line, "exit") == 0) break;
        if (line[0] == '\0') continue;

        if (nx_strncmp(line, "seed", 4) == 0) {
            nx_srand((uint64_t)nx_strtol(line + 4, NULL, 10));
            continue;
        }
        if (nx_strcmp(line, "reset") == 0 || nx_strcmp(line, "help") == 0) {
            banner();
            continue;
        }

        int err = 0;
        double r = eval_expr(line, &err);
        char out[64];
        if (err) {
            nx_cprintf(RED, 0, "error: bad expression\n");
        } else {
            nx_sprintf(out, "%.6f", r);
            char *dot = nx_strchr(out, '.');
            char *e = out + nx_strlen(out) - 1;
            while (dot && e > dot && *e == '0') { *e = '\0'; e--; }
            if (dot && e == dot) *e = '\0';
            nx_cprintf(GREEN, 0, "  = %s\n", out);
        }
    }
    nx_puts("bye.\n");
    proc_exit(0);
    return 0;
}