
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include "frame.h"
#include "image.h"

#define FRAME_MAGIC MAKE_FOURCC('F','R','A','M')

#define FRAME_FMT_YV12    MAKE_FOURCC('Y','V','1','2')  /* 12 Y/CbCr 4:2:0 */
#define FRAME_FMT_YUV420 MAKE_FOURCC('Y','4','2','0')   /*!< 12 YUV 4:2:0 */
#define FRAME_FMT_YUV420P MAKE_FOURCC('4','2','0','P')  /*!< 12 YUV 4:2:0 */
#define FRAME_FMT_YUV422 MAKE_FOURCC('Y','4','2','2')   /*!< 16 YUV 4:2:2 */
#define FRAME_FMT_YUV422P MAKE_FOURCC('4','2','2','P')  /*!< 16 YUV 4:2:2 */
#define FRAME_FMT_YUV444  MAKE_FOURCC('Y','4','4','4')  /*!< 24 YUV 4:4:4 */
#define FRAME_FMT_YUV444P  MAKE_FOURCC('4','4','4','P') /*!< 24 YUV 4:4:4 */
#define FRAME_FMT_RGB24   MAKE_FOURCC('R','G','B','3')  /*!< 24 RGB-8-8-8 */
#define FRAME_FMT_RGBA32  MAKE_FOURCC('R','G','B','A')  /*!< 32 RGB-8-8-8-8 */

int frame_goodfmt(DWORD fmt)
{
	return (fmt == FRAME_FMT_YV12 ||
			fmt == FRAME_FMT_YUV420 || fmt == FRAME_FMT_YUV420P || 
			fmt == FRAME_FMT_YUV422 || fmt == FRAME_FMT_YUV422P ||
			fmt == FRAME_FMT_YUV444 || fmt == FRAME_FMT_YUV444P ||
			fmt == FRAME_FMT_RGB24 || fmt == FRAME_FMT_RGBA32);
}

int frame_goodbuf(FRAME *f)
{
	if (! f->Y)
		return 0;

	switch(f->format) {
	case FRAME_FMT_YV12:
	case FRAME_FMT_YUV420P:
	case FRAME_FMT_YUV422P:
	case FRAME_FMT_YUV444P:
		return (f->U && f->V);
	default:
		break;
	}

	return 0;	
}


int frame_valid(FRAME *f)
{
	if (! f || f->height < 1 || f->width < 1 || f->magic != FRAME_MAGIC)
		return 0;

	return frame_goodfmt(f->format);
}

int frame_size(DWORD fmt, int w, int h)
{
	switch(fmt) {
	case FRAME_FMT_YV12:
	case FRAME_FMT_YUV420:
	case FRAME_FMT_YUV420P:
		return 3*w*h/2;
		break;
	case FRAME_FMT_YUV422:
	case FRAME_FMT_YUV422P:
		return 2 * w * h;
		break;
	case FRAME_FMT_YUV444:
	case FRAME_FMT_YUV444P:
	case FRAME_FMT_RGB24:
		return 3 * w * h;
		break;
	case FRAME_FMT_RGBA32:
		return 4 * w * h;
		break;
	default:
		break;
	}
	
	return -1;
}

FRAME *frame_create(DWORD fmt, WORD width, WORD height)
{
	FRAME *f;

	if (! frame_goodfmt(fmt)) {
		syslog_error("Unsupport format.");
		return NULL; 
	}
	
	f = (FRAME *)calloc((size_t)1, sizeof(FRAME)); 
	if (! f) { 
		syslog_error("Allocate memeory.");
		return NULL; 
	}

	f->format = fmt;
	f->height = height;
	f->width = width;
	f->magic = FRAME_MAGIC;

	return f;
}

void frame_destroy(FRAME *f)
{
	if (! frame_valid(f))
		return;
	free(f);
}

int frame_binding(FRAME *f, BYTE *buf)
{
	check_frame(f);
	if (! buf) {
		syslog_error("Binding frame to null buffer");
		return RET_ERROR;
	}
	f->Y = buf;

	// Adjust U/V Plane
	switch(f->format) {
	case FRAME_FMT_YV12:
	case FRAME_FMT_YUV420P:
		// Y -Page + 1/4  U-Page + 1/4 V-Page
		f->U = f->Y + f->width * f->height;
		f->V = f->U + f->width * f->height/4;
		break;
	case FRAME_FMT_YUV420:
		// (YUV) ... Y ... Y ... Y
		f->U = f->V = NULL;
		break;
	case FRAME_FMT_YUV422:
		// (YUV) ... (Y) ... (YUV) .. (Y)
		f->U = f->V = NULL;
		break;
	case FRAME_FMT_YUV422P:
		f->U = f->Y + f->width * f->height;
		f->V = f->U + f->width * f->height/2;
		break;
	case FRAME_FMT_YUV444:
		// Y/U/V ...
		f->U = f->V = NULL;
		break;
	case FRAME_FMT_YUV444P:
		// Y-Pgae + U-Page + V-Page
		f->U = (f->Y + f->width * f->height);
		f->V = (f->U + f->width * f->height);
		break;
	case FRAME_FMT_RGB24:
		// R/G/B ...
		f->U = f->V = NULL;
 		break;
	case FRAME_FMT_RGBA32:
		// R/G/B/A ...
		f->U = f->V = NULL;
 		break;
	default:
		syslog_error("Invalid format");
		return RET_ERROR;
		break;
	}

	return RET_OK;	
}

DWORD frame_format(char *name)
{
	DWORD format = FRAME_FMT_YV12;

	if (strcasecmp(name, "YV12") == 0 || strcasecmp(name, "YCbCr") == 0)
		format = FRAME_FMT_YV12;
	else if (strcasecmp(name, "YUV420") == 0)
		format = FRAME_FMT_YUV420;
	else if (strcasecmp(name, "YUV420P") == 0)
		format = FRAME_FMT_YUV420P;
	else if (strcasecmp(name, "YUV422") == 0)
		format = FRAME_FMT_YUV422;
	else if (strcasecmp(name, "YUV422P") == 0)
		format = FRAME_FMT_YUV422P;
	else if (strcasecmp(name, "YUV444") == 0)
		format = FRAME_FMT_YUV444;
	else if (strcasecmp(name, "YUV444P") == 0)
		format = FRAME_FMT_YUV444P;
	else if (strcasecmp(name, "RGB24") == 0)
		format = FRAME_FMT_RGB24;
	else if (strcasecmp(name, "RGBA32") == 0) 
		format = FRAME_FMT_RGBA32;
	else
		syslog_error("Unkown video format %s.\n", name);

	return format;
}

int  frame_toimage(FRAME *f, IMAGE *img)
{
	int i, j, y, cb, cr, r, g, b;
	BYTE *us, *vs, *ys;

	check_frame(f);
	check_image(img);

	y = cb = cr = r = g = b = 0;

	if (! frame_goodbuf(f)) {
		syslog_error("Frame buffer");
		return RET_ERROR;
	}

	if (f->width != img->width || f->height != img->height) {
		syslog_error("Frame and image size is not same");
		return RET_ERROR;
	}

	ys = f->Y;
	switch(f->format) {
	case FRAME_FMT_YV12:
	case FRAME_FMT_YUV420P:
		// Y -Page + 1/4  U-Page + 1/4 V-Page
		us = f->U;
		vs = f->V;
		image_foreach(img, i, j) {
			y = *ys++;
			if (i % 2 == 0 && j % 2 == 0) {
				cb = *us++;
				cr = *vs++;
			}
			YCBCR_TO_RGB(y, cb, cr, r, g, b);
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	case FRAME_FMT_YUV420:
		// (YUV) ... Y ... Y ... Y
		image_foreach(img, i, j) {
			y = *ys++;
			if (i % 2 == 0 && j % 2 == 0) {
				cb = *ys++;
				cr = *ys++;
			}
			YCBCR_TO_RGB(y, cb, cr, r, g, b);
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	case FRAME_FMT_YUV422:
		// (YUV) ... (Y) ... (YUV) .. (Y)
		image_foreach(img, i, j) {
			y = *ys++;
			if (j % 2 == 0) {
				cb = *ys++;
				cr = *ys++;
			}
			YCBCR_TO_RGB(y, cb, cr, r, g, b);
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	case FRAME_FMT_YUV422P:
		us = f->U;
		vs = f->V;
		image_foreach(img, i, j) {
			y = *ys++;
			if (j % 2 == 0) {
				cb = *us++;
				cr = *vs++;
			}
			YCBCR_TO_RGB(y, cb, cr, r, g, b);
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	case FRAME_FMT_YUV444:
		// Y/U/V ...
		image_foreach(img, i, j) {
			y = *ys++;
			cb = *ys++;
			cr = *ys++;
			YCBCR_TO_RGB(y, cb, cr, r, g, b);
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	case FRAME_FMT_YUV444P:
		// Y-Pgae + U-Page + V-Page
		us = f->U;
		vs = f->V;
 		image_foreach(img, i, j) {
			y = *ys++;
			cb = *us++;
			cr = *vs++;
			YCBCR_TO_RGB(y, cb, cr, r, g, b);
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	case FRAME_FMT_RGB24:
		memcpy(img->base, ys, 3 * img->width * img->height);
 		break;
	case FRAME_FMT_RGBA32:
		image_foreach(img, i, j) {
			r = *ys++;
			g = *ys++;
			b = *ys++;
			ys++;
			img->ie[i][j].r = r; img->ie[i][j].g = g; img->ie[i][j].b = b;
		}
		break;
	default:
		break;
	}

	return RET_OK;
}

