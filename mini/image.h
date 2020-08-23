
/************************************************************************************
***
***	Copyright 2020 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, 2020年 08月 23日 星期日 01:44:37 CST
***
************************************************************************************/


#ifndef __IMAGE_H
#define __IMAGE_H

#if defined(__cplusplus)
extern "C" {
#endif

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned int
#define LONG long //int

typedef struct {
	DWORD magic;				// IMAGE_MAGIC
	int height, width;			
	BYTE *base;					// model: CxHxW
	BYTE **R, **G, **B;			// Channel RGB
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

#define syslog_error(fmt, arg...)  do { \
		fprintf(stderr, "Error: %d(%s): " fmt "\n", (int)__LINE__, __FILE__, ##arg); \
		exit(0); \
	} while (0)


char *image_version();

IMAGE *image_create(WORD h, WORD w);

int image_valid(IMAGE *img);
int image_clear(IMAGE *img);
IMAGE *image_copy(IMAGE *img);
IMAGE *image_load(char *fname);
int image_save(IMAGE *img, const char *fname);

void image_destroy(IMAGE *img);

#if defined(__cplusplus)
}
#endif

#endif	// __IMAGE_H
