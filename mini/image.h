
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/


#ifndef __IMAGE_H
#define __IMAGE_H

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
#define ABS(a) ((a) > 0? (a) : (-(a)))


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

typedef struct {
	BYTE r, g, b, a;
	WORD d; // d -- depth;
} RGB;

typedef struct {
	DWORD magic;				// IMAGE_MAGIC
	int height, width;			// RGB, GRAY, BIT
	RGB **ie,*base; 
} IMAGE;


#define image_foreach(img, i, j) \
	for (i = 0; i < img->height; i++) \
		for (j = 0; j < img->width; j++)

#define check_image(img) \
	do { \
		if (! image_valid(img)) { \
			syslog_error("Bad image.\n"); \
			return RET_ERROR; \
		} \
	} while(0)

#define CHECK_IMAGE(img) \
	do { \
		if (! image_valid(img)) { \
			syslog_error("Bad image.\n"); \
			return NULL; \
		} \
	} while(0)

		
int image_memsize(WORD h, WORD w);
void image_membind(IMAGE *img, WORD h, WORD w);

IMAGE *image_create(WORD h, WORD w);
IMAGE *image_load(char *fname);
IMAGE *image_copy(IMAGE *img);

int image_valid(IMAGE *img);
int image_clear(IMAGE *img);
int image_save(IMAGE *img, const char *fname);

void image_destroy(IMAGE *img);

#if defined(__cplusplus)
}
#endif

#endif	// __IMAGE_H
