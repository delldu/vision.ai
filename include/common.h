#ifndef _COMMON_H
#define _COMMON_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned int
#define LONG long //int
#define TIME int64_t

#define RET_FAIL 1
#define RET_OK 0
#define RET_ERROR (-1)

#define MIN(a, b)  ((a) > (b)? (b) : (a))
#define MAX(a, b)  ((a) > (b)? (a) : (b))
#define CLAMP(x,min,max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define ABS(a) ((a) > 0? (a) : (-(a)))

#define FILENAME_MAXLEN 256

#define MATH_PI 3.1415926f
#define MIN_DOUBLE_NUMBER 0.000001f

#define R_CHANNEL 0
#define G_CHANNEL 1
#define B_CHANNEL 2

// Zoom method
#define ZOOM_METHOD_COPY 0
#define ZOOM_METHOD_BLINE 1

// Color balance
#define COLOR_BALANCE_GRAY_WORLD 0
#define COLOR_BALANCE_FULL_REFLECT 1

#define CheckPoint(fmt, arg...) printf("# CheckPoint: %d(%s): " fmt "\n", (int)__LINE__, __FILE__, ##arg)
#define syslog_print(fmt, arg...) fprintf(stderr, fmt, ##arg)
#define syslog_debug(fmt, arg...)  do { \
		fprintf(stdout, "Debug: %d(%s): " fmt "\n", (int)__LINE__, __FILE__, ##arg); \
	} while (0)
#define syslog_error(fmt, arg...)  do { \
		fprintf(stderr, "Error: %d(%s): " fmt "\n", (int)__LINE__, __FILE__, ##arg); \
		exit(0); \
	} while (0)

#define ARRAY_SIZE(x) (int)(sizeof(x)/sizeof(x[0]))

#define MAKE_FOURCC(a,b,c,d) (((DWORD)(a) << 0) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))
#define GET_FOURCC1(a) ((BYTE)((a) & 0xff))
#define GET_FOURCC2(a) ((BYTE)(((a)>>8) & 0xff))
#define GET_FOURCC3(a) ((BYTE)(((a)>>16) & 0xff))
#define GET_FOURCC4(a) ((BYTE)(((a)>>24) & 0xff))

typedef struct { int r, c; } DOT;
typedef struct { int r1, c1, r2, c2; } LINE;
typedef struct { int x, y, r; } CIRCLE;
typedef struct { int x, y, a, b; } OVAL;
typedef struct { int r, c, h, w; } RECT;
typedef struct { int h, w; } SIZE;

#define MAX_OBJECT_NUM 2048
typedef struct { int count; DOT dot[MAX_OBJECT_NUM]; } DOTS;
typedef struct { int count; LINE line[MAX_OBJECT_NUM]; } LINES;
typedef struct { int count; CIRCLE circle[MAX_OBJECT_NUM]; } CIRCLES;
typedef struct { int count; OVAL oval[MAX_OBJECT_NUM]; } OVALS;
typedef struct { int count; RECT rect[MAX_OBJECT_NUM]; } RECTS;


#define line_dump(line) printf("line: (%d, %d, %d, %d)\n", (line)->r1, (line)->c1, (line)->r2, (line)->c2)
#define circle_dump(circle) printf("circle: (%d, %d)\n", (circle)->x, (circle)->y, (circle)->r)
#define oval_dump(oval) printf("oval: (%d, %d, %d)\n", (oval)->x, (oval)->y, (oval)->a, (oval)->b)
#define rect_dump(rect) printf("rect: (%d, %d, %d, %d)\n", (rect)->r, (rect)->c, (rect)->h, (rect)->w)

#define rect_same(rect1, rect2) \
	((rect1)->r == (rect2)->r && (rect1)->c == (rect2)->c && (rect1)->h == (rect2)->h && (rect1)->w == (rect2)->w)

#define rect_foreach(rect, i, j) \
	for (i = 0; i < (rect)->h; i++) \
		for (j = 0; j < (rect)->w; j++)

#define msleep(n) usleep((n) * 1000)		

typedef double (*distancef_t)(double *a, double *b, int n);

#define VOICE_CELL_OFFSET(r,c,d) (((r)*cols + (c))*(levs) + (d))


int buf_scanf(char *buf, const char *pattern, ...);
int get_token(char *buf, char deli, int maxcnt, char *tv[]);

int bit_count(int val);


int math_log2(int n);
int math_arcindex(double a, int arcstep);
int math_gsbw(double sigma);
void math_topolar(int x, int y, double *r, double *a);

DOTS *dot_set();
void dot_put(int r, int c);

// Hough Transform
LINES *line_set();
void line_put(int r1, int c1, int r2, int c2);

CIRCLES *circle_set();
void circle_put(int x, int y, int r);

OVALS *oval_set();
void oval_put(int x, int y, int a, int b);


// Motion Detection
RECTS *rect_set();

int rect_put(RECT *rect);
int rect_delete(int i);


TIME get_time();

// Largest common string length
int lcs_len(char *a, char *b);

void time_reset();
void time_spend(char *prompt);

#if defined(__cplusplus)
}
#endif

#endif	// _COMMON_H


