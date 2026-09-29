/*
 * Notux OS — Syscall Numbers
 * libnotux/include/notux/syscalls.h
 *
 * Numbers 0–399 intentionally match Linux x86-64 for compatibility
 * with programs compiled against musl-notux or compat_posix.h.
 * Numbers 400+ are Notux-specific extensions.
 */

#pragma once

/* ── I/O (Linux-compatible) ─────────────────────────────────── */
#define SYS_READ         0
#define SYS_WRITE        1
#define SYS_OPEN         2
#define SYS_CLOSE        3
#define SYS_STAT         4
#define SYS_FSTAT        5
#define SYS_LSTAT        6
#define SYS_SEEK         8   /* lseek */
#define SYS_MMAP         9
#define SYS_MUNMAP      11
#define SYS_BRK         12
#define SYS_IOCTL       16
#define SYS_READV       19
#define SYS_WRITEV      20
#define SYS_PIPE        22
#define SYS_DUP         32
#define SYS_DUP2        33
#define SYS_PAUSE       34

/* ── File system ─────────────────────────────────────────────── */
#define SYS_RENAME      82
#define SYS_MKDIR       83
#define SYS_RMDIR       84
#define SYS_CREAT       85
#define SYS_UNLINK      87
#define SYS_SYMLINK     88
#define SYS_READLINK    89
#define SYS_CHMOD       90
#define SYS_CHOWN       92
#define SYS_GETCWD      79  /* actually getdents on Linux; Notux reuses */
#define SYS_CHDIR       80
#define SYS_OPENDIR    300  /* Notux extension in low range */
#define SYS_READDIR    301
#define SYS_CLOSEDIR   302
#define SYS_TOUCH      303
#define SYS_STATFS     306

/* ── Process ─────────────────────────────────────────────────── */
#define SYS_GETPID      39
#define SYS_GETPPID    110
#define SYS_GETUID     102
#define SYS_GETGID     104
#define SYS_SETUID     105
#define SYS_SETGID     106
#define SYS_FORK        57
#define SYS_EXEC        59  /* execve */
#define SYS_EXIT        60
#define SYS_WAIT        61  /* wait4 */
#define SYS_KILL        62
#define SYS_YIELD      158  /* sched_yield */
#define SYS_SLEEP      162  /* nanosleep repurposed as ms sleep */
#define SYS_GETENV     310
#define SYS_SETENV     311

/* ── Memory ──────────────────────────────────────────────────── */
#define SYS_MPROTECT    10

/* ── Network ─────────────────────────────────────────────────── */
#define SYS_SOCKET      41
#define SYS_CONNECT     42
#define SYS_ACCEPT      43
#define SYS_SEND        44
#define SYS_RECV        45
#define SYS_BIND        49
#define SYS_LISTEN      50
#define SYS_SOCKCLOSE   351
#define SYS_RESOLVE     352  /* DNS */
#define SYS_PING        353

/* ── Terminal / color ────────────────────────────────────────── */
#define SYS_TERM_SETFG  400
#define SYS_TERM_SETBG  401
#define SYS_TERM_CLEAR  402
#define SYS_TERM_MOVE   403
#define SYS_TERM_ROWS   404
#define SYS_TERM_COLS   405
#define SYS_TERM_RDPASS 406  /* read password (no echo) */
#define SYS_TERM_RDLINE 407  /* read line with echo + history */

/* ── Time ────────────────────────────────────────────────────── */
#define SYS_UPTIME      410
#define SYS_GETTIME     411

/* ── Graphics (direct framebuffer access for GUI programs) ───── */
#define SYS_FB_INFO     420  /* returns FramebufferInfo */
#define SYS_FB_BLIT     421  /* blit pixel buffer to screen region */
#define SYS_FB_SETPX    422  /* set single pixel */
#define SYS_WIN_CREATE  430  /* create a GUI window */
#define SYS_WIN_DESTROY 431
#define SYS_WIN_BLIT    432
#define SYS_WIN_MOVE    433
#define SYS_WIN_RESIZE  434

/* ── Audio ───────────────────────────────────────────────────── */
#define SYS_AUDIO_OPEN  440
#define SYS_AUDIO_WRITE 441
#define SYS_AUDIO_CLOSE 442
#define SYS_SPEAKER_BEEP 443  /* PC speaker: freq Hz, duration ms */

/* ── IPC ─────────────────────────────────────────────────────── */
#define SYS_PIPE2       293
#define SYS_SHMGET      450
#define SYS_SHMAT       451
#define SYS_SHMDT       452
#define SYS_SHMCTL      453
#define SYS_MSGGET      454
#define SYS_MSGSND      455
#define SYS_MSGRCV      456

/* ── Services ────────────────────────────────────────────────── */
#define SYS_SVC_START   460
#define SYS_SVC_STOP    461
#define SYS_SVC_STATUS  462
#define SYS_SVC_LIST    463

/* ── System ──────────────────────────────────────────────────── */
#define SYS_REBOOT      169
#define SYS_POWEROFF    470
#define SYS_KLOG        471  /* write to kernel log */
#define SYS_SYSINFO     472  /* returns memory/cpu/proc stats */

/* ── Dynamic linking ─────────────────────────────────────────── */
#define SYS_DLOPEN      480
#define SYS_DLSYM       481
#define SYS_DLCLOSE     482

/* ── Notux-specific helpers ──────────────────────────────────── */
#define SYS_NOTUX_VER   490  /* returns version string */
#define SYS_DEBUG_PRINT 499  /* kernel debug print (dev builds only) */

/* ── Error codes (errno values) ──────────────────────────────── */
#define EPERM    1
#define ENOENT   2
#define ESRCH    3
#define EIO      5
#define EBADF    9
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EEXIST  17
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENFILE  23
#define EMFILE  24
#define ENOSPC  28
#define EPIPE   32
#define ERANGE  34
#define ENOSYS  38
#define ENOTSUP 95
#define ETIMEDOUT 110

/* ── Signals ─────────────────────────────────────────────────── */
#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGTRAP  5
#define SIGABRT  6
#define SIGBUS   7
#define SIGFPE   8
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20
