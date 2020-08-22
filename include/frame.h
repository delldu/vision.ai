
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/


#ifndef _FRAME_H
#define _FRAME_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "simage.h"

typedef struct {
	DWORD magic;				// FRAME_MAGIC
	DWORD format;
	WORD height, width;
	BYTE *Y, *U, *V;
} FRAME;

#define check_frame(frame) \
	do { \
		if (! frame_valid(frame)) { \
			return RET_ERROR; \
		} \
	} while(0)

#define CHECK_FRAME(frame) \
	do { \
		if (! frame_valid(frame)) { \
			return NULL; \
		} \
	} while(0)

#define frame_foreach(frame, i, j) \
	for (i = 0; i < frame->height; i++) \
		for (j = 0; j < frame->width; j++)


int frame_valid(FRAME *f);
int frame_goodfmt(DWORD fmt);
int frame_goodbuf(FRAME *f);
int frame_size(DWORD fmt, int w, int h);
int frame_binding(FRAME *f, BYTE *buf);
int frame_toimage(FRAME *f, IMAGE *img);
DWORD frame_format(char *name);
FRAME *frame_create(DWORD fmt, WORD width, WORD height);
void frame_destroy(FRAME *f);


#if defined(__cplusplus)
}
#endif

#endif	// _FRAME_H
