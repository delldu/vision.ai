
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/



#ifndef __VIDEO_H
#define __VIDEO_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>	
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <sys/ioctl.h>


#include "simage.h"
#include "frame.h"

#define VIDEO_MAGIC MAKE_FOURCC('V','I','D','E')


#define VIDEO_BUFFER_NUMS 3
#define VIDEO_FROM_CAMERA 1		// From camera, file, network ...

struct video_buffer_s {
	size_t offset;
	unsigned int length;
	void *startmap;				//  memory map/allocate start
};

typedef struct {
	DWORD magic;				// VIDEO_MAGIC
	DWORD format;
	WORD width, height;
	int frame_index, frame_size, frame_speed;
	FRAME *frames[VIDEO_BUFFER_NUMS];
	int video_source;

	// internal/temp
	FILE *_fp;
	int _buffer_index;
	struct video_buffer_s _frame_buffer[VIDEO_BUFFER_NUMS];
} VIDEO;

#define check_video(video) \
	do { \
		if (! video_valid(video)) { \
			syslog_error("Bad video.\n"); \
			return RET_ERROR; \
		} \
	} while(0)

#define CHECK_VIDEO(video) \
	do { \
		if (! video_valid(video)) { \
			syslog_error("Bad video.\n"); \
			return NULL; \
		} \
	} while(0)

#define video_foreach(video, i, j) \
	for (i = 0; i < video->height; i++) \
		for (j = 0; j < video->width; j++)


int video_valid(VIDEO *v);
VIDEO *video_open(char *filename, int start);
FRAME *video_read(VIDEO *v);
void video_info(VIDEO *v);
void video_close(VIDEO *v);

int video_play(char *filename, int start);

// Video MDS: Motion Detection System
int video_mds(char *filename, int start);

// Video IDS: Instrusion Detection System
int video_ids(char *filename, int start, double threshold);

// YUV420 !!!
int yuv420_play(char *filename, int width, int height);

#if defined(__cplusplus)
}
#endif

#endif	// __VIDEO_H
