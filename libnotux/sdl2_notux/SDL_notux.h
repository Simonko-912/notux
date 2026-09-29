/*
 * Notux OS — SDL2-notux Framebuffer Port
 * libnotux/sdl2_notux/SDL_notux.h
 *
 * A minimal SDL2 implementation backed by the Notux framebuffer.
 * Games and graphical programs that use SDL2 can compile and run
 * on Notux with no code changes:
 *
 *   gcc $(sdl2-config --cflags --libs) game.c -o game
 *   // (sdl2-config on Notux points to this port)
 *
 * ── What is implemented ────────────────────────────────────────
 *   SDL_Init / SDL_Quit
 *   SDL_CreateWindow / SDL_DestroyWindow
 *   SDL_CreateRenderer / SDL_DestroyRenderer
 *   SDL_RenderClear / SDL_RenderPresent
 *   SDL_SetRenderDrawColor
 *   SDL_RenderDrawRect / SDL_RenderFillRect
 *   SDL_RenderDrawLine / SDL_RenderDrawPoint
 *   SDL_CreateTexture / SDL_DestroyTexture
 *   SDL_UpdateTexture / SDL_RenderCopy
 *   SDL_PollEvent (keyboard + mouse)
 *   SDL_GetKeyboardState / SDL_GetMouseState
 *   SDL_Delay
 *   SDL_GetTicks / SDL_GetTicks64
 *   SDL_Surface (basic blitting)
 *   SDL_LoadBMP / SDL_FreeSurface
 *   SDL_MapRGB / SDL_FillRect
 *
 * ── What is NOT implemented (yet) ─────────────────────────────
 *   SDL_mixer (audio — planned)
 *   SDL_ttf   (use Notux font API instead)
 *   SDL_net   (use notux/net.h directly)
 *   OpenGL    (no GPU driver yet)
 *   Joystick / gamepad
 */

#pragma once
#ifdef __NOTUX__

#include <notux/libc.h>
#include <stdint.h>
#include <stddef.h>

/* ── SDL types ───────────────────────────────────────────────── */
typedef int SDL_bool;
#define SDL_TRUE  1
#define SDL_FALSE 0

typedef int32_t  Sint32;
typedef uint32_t Uint32;
typedef int16_t  Sint16;
typedef uint16_t Uint16;
typedef uint8_t  Uint8;
typedef int64_t  Sint64;
typedef uint64_t Uint64;

/* ── Init flags ──────────────────────────────────────────────── */
#define SDL_INIT_VIDEO      0x00000020
#define SDL_INIT_EVENTS     0x00004000
#define SDL_INIT_TIMER      0x00000001
#define SDL_INIT_EVERYTHING 0x0000FFFF

int  SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char *SDL_GetError(void);

/* ── Window ──────────────────────────────────────────────────── */
typedef struct SDL_Window {
    int x, y, w, h;
    char title[128];
    Uint32 flags;
    void *_fb;    /* points to framebuffer region */
} SDL_Window;

#define SDL_WINDOWPOS_CENTERED  0x2FFF0000
#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000
#define SDL_WINDOW_SHOWN        0x00000004
#define SDL_WINDOW_FULLSCREEN   0x00000001
#define SDL_WINDOW_RESIZABLE    0x00000020

SDL_Window *SDL_CreateWindow(const char *title,
                              int x, int y, int w, int h,
                              Uint32 flags);
void SDL_DestroyWindow(SDL_Window *win);
void SDL_GetWindowSize(SDL_Window *win, int *w, int *h);
void SDL_SetWindowTitle(SDL_Window *win, const char *title);

/* ── Renderer ────────────────────────────────────────────────── */
typedef struct SDL_Renderer {
    SDL_Window *win;
    Uint8 draw_r, draw_g, draw_b, draw_a;
} SDL_Renderer;

#define SDL_RENDERER_ACCELERATED  0x00000002
#define SDL_RENDERER_PRESENTVSYNC 0x00000004

SDL_Renderer *SDL_CreateRenderer(SDL_Window *win, int idx, Uint32 flags);
void SDL_DestroyRenderer(SDL_Renderer *r);
int  SDL_RenderClear(SDL_Renderer *r);
void SDL_RenderPresent(SDL_Renderer *r);
int  SDL_SetRenderDrawColor(SDL_Renderer *r, Uint8 red, Uint8 g, Uint8 b, Uint8 a);
int  SDL_SetRenderDrawBlendMode(SDL_Renderer *r, int mode);

/* ── Rect / Point ────────────────────────────────────────────── */
typedef struct { int x, y, w, h; } SDL_Rect;
typedef struct { int x, y; }       SDL_Point;

int SDL_RenderFillRect(SDL_Renderer *r, const SDL_Rect *rect);
int SDL_RenderDrawRect(SDL_Renderer *r, const SDL_Rect *rect);
int SDL_RenderDrawLine(SDL_Renderer *r, int x1, int y1, int x2, int y2);
int SDL_RenderDrawPoint(SDL_Renderer *r, int x, int y);
int SDL_RenderDrawLines(SDL_Renderer *r, const SDL_Point *pts, int n);
int SDL_RenderFillRects(SDL_Renderer *r, const SDL_Rect *rects, int n);

/* ── Texture ─────────────────────────────────────────────────── */
typedef struct SDL_Texture {
    int w, h;
    Uint32 format;
    Uint32 *pixels;
} SDL_Texture;

#define SDL_PIXELFORMAT_ARGB8888 0x16362004
#define SDL_PIXELFORMAT_RGB24    0x17101803
#define SDL_TEXTUREACCESS_STATIC    0
#define SDL_TEXTUREACCESS_STREAMING 1

SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 format,
                                int access, int w, int h);
void SDL_DestroyTexture(SDL_Texture *t);
int  SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *rect,
                        const void *pixels, int pitch);
int  SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t,
                    const SDL_Rect *src, const SDL_Rect *dst);
int  SDL_LockTexture(SDL_Texture *t, const SDL_Rect *rect,
                      void **pixels, int *pitch);
void SDL_UnlockTexture(SDL_Texture *t);
int  SDL_SetTextureBlendMode(SDL_Texture *t, int mode);
int  SDL_SetTextureAlphaMod(SDL_Texture *t, Uint8 alpha);

/* ── Surface ─────────────────────────────────────────────────── */
typedef struct {
    int w, h, pitch;
    Uint32 format;
    void *pixels;
    int _refcount;
} SDL_Surface;

SDL_Surface *SDL_LoadBMP(const char *path);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h,
                                   int depth, Uint32 Rmask, Uint32 Gmask,
                                   Uint32 Bmask, Uint32 Amask);
void SDL_FreeSurface(SDL_Surface *s);
int  SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect,
                      SDL_Surface *dst, SDL_Rect *dstrect);
int  SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
Uint32 SDL_MapRGB(const void *fmt, Uint8 r, Uint8 g, Uint8 b);
SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *r, SDL_Surface *s);

/* ── Events ──────────────────────────────────────────────────── */
typedef enum {
    SDL_QUIT           = 0x100,
    SDL_KEYDOWN        = 0x300,
    SDL_KEYUP          = 0x301,
    SDL_MOUSEMOTION    = 0x400,
    SDL_MOUSEBUTTONDOWN= 0x401,
    SDL_MOUSEBUTTONUP  = 0x402,
    SDL_MOUSEWHEEL     = 0x403,
} SDL_EventType;

typedef struct {
    Sint32  sym;       /* SDL_Keycode */
    Uint16  mod;       /* SDL_Keymod  */
    Uint16  scancode;
} SDL_Keysym;

typedef struct {
    Uint32      type;
    Uint32      timestamp;
    SDL_Keysym  keysym;
    Uint8       repeat;
} SDL_KeyboardEvent;

typedef struct {
    Uint32 type;
    Uint32 timestamp;
    Sint32 x, y;
    Sint32 xrel, yrel;
    Uint32 state;
} SDL_MouseMotionEvent;

typedef struct {
    Uint32 type;
    Uint32 timestamp;
    Uint8  button;
    Uint8  state;
    Sint32 x, y;
} SDL_MouseButtonEvent;

typedef struct {
    Uint32 type;
    Uint32 timestamp;
} SDL_QuitEvent;

typedef union {
    Uint32 type;
    SDL_KeyboardEvent    key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_QuitEvent        quit;
    Uint8                padding[56];
} SDL_Event;

int  SDL_PollEvent(SDL_Event *event);
void SDL_PumpEvents(void);

/* Keyboard state */
#define SDL_SCANCODE_W      26
#define SDL_SCANCODE_A       4
#define SDL_SCANCODE_S      22
#define SDL_SCANCODE_D       7
#define SDL_SCANCODE_SPACE  44
#define SDL_SCANCODE_ESCAPE 41
#define SDL_SCANCODE_RETURN 40
#define SDL_SCANCODE_UP     82
#define SDL_SCANCODE_DOWN   81
#define SDL_SCANCODE_LEFT   80
#define SDL_SCANCODE_RIGHT  79
#define SDL_NUM_SCANCODES  512

#define SDLK_w     'w'
#define SDLK_a     'a'
#define SDLK_s     's'
#define SDLK_d     'd'
#define SDLK_SPACE ' '
#define SDLK_ESCAPE 27
#define SDLK_RETURN '\r'
#define SDLK_UP    0x40000052
#define SDLK_DOWN  0x40000051
#define SDLK_LEFT  0x40000050
#define SDLK_RIGHT 0x4000004F

const Uint8 *SDL_GetKeyboardState(int *numkeys);
Uint32       SDL_GetMouseState(int *x, int *y);

/* ── Time ────────────────────────────────────────────────────── */
Uint32 SDL_GetTicks(void);
Uint64 SDL_GetTicks64(void);
void   SDL_Delay(Uint32 ms);

/* ── Blend modes ─────────────────────────────────────────────── */
#define SDL_BLENDMODE_NONE  0
#define SDL_BLENDMODE_BLEND 1
#define SDL_BLENDMODE_ADD   2

#endif /* __NOTUX__ */
