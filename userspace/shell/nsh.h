/*
 * nsh.h — Notux shell, public/shared declarations.
 */

#ifndef NSH_H
#define NSH_H

#define NSH_VERSION      "0.8.0"
#define NSH_HISTORY_MAX  100
#define NSH_PATH_MAX     256

/* ── shared state ────────────────────────────────────────────── */
extern char g_cwd[NSH_PATH_MAX];
extern char g_history[NSH_HISTORY_MAX][NSH_PATH_MAX];
extern int  g_history_count;
extern int  g_history_pos;

/* ── terminal / string helpers (nsh.c) ───────────────────────── */
void     nsh_print_banner(void);
void     nsh_err(const char *fmt, ...);
void     nsh_ok(const char *fmt, ...);
int      nsh_getline(char *buf, int n);       /* line input w/ echo on */
int      nsh_getline_noecho(char *buf, int n);
void     nsh_split(char *line, char **args, int *argc, int max);
int      nsh_find_program(const char *name, char *out, int outn);
void     nsh_save_history_line(const char *line);

/* ── command dispatch (nsh_enhanced.c) ───────────────────────── */
int nsh_is_builtin(const char *name);
int nsh_dispatch(char *line);

/* ── tab completion (nsh_enhanced.c) ─────────────────────────── */
void nsh_complete(const char *prefix);

#endif /* NSH_H */