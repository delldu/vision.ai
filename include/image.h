
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

#include "simage.h"
#include "matrix.h"

typedef struct {
	BYTE r, g, b, a;
	WORD d; // d -- depth;
} RGB;
#define RGB_R(x) ((BYTE)((x) >> 16) & 0xff)
#define RGB_G(x) ((BYTE)((x) >> 8) & 0xff)
#define RGB_B(x) ((BYTE)((x) & 0xff))
#define RGB_RGB(r, g, b) ((r) << 16 | (g) << 8 | (b))

typedef struct {
	double L, a, b;
} LAB;
#define LAB_QDIM(qL, qa, qb) (1 + ((int)(100/(qL))) * ((int)(240)/(qa)) * ((int)(240)/(qb)))

typedef struct {
	double L, u, v;
} Luv;

typedef struct {
	DWORD magic;				// IMAGE_MAGIC
	int height, width, format;	// RGB, GRAY, BIT
//	char filename[FILENAME_MAXLEN];
	RGB **ie,*base; 
} IMAGE;

#define IMAGE_RGB 0
#define IMAGE_RGBREV 1
#define IMAGE_BITMAP 2
#define IMAGE_GRAY 3
#define IMAGE_YCBCR 4
#define IMAGE_HSV 5
#define IMAGE_LAB 6
#define IMAGE_LUV 7
#define IMAGE_RLUV 8
#define IMAGE_RGB565 9

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

		
#define image_rect(rect, img) \
	do { (rect)->r = 0; (rect)->c = 0; (rect)->h = img->height; (rect)->w = img->width; } while(0)


#define check_rgb(orgb) \
	do { \
		if (! strchr("ARGB", orgb)) { \
			syslog_error("Bad color %c (ARGB).\n", orgb); \
			return RET_ERROR; \
		} \
	} while (0)

#define check_argb(orgb) \
	do { \
		if (! strchr("ARGB", orgb)) { \
			syslog_error("Bad color %c (ARGB).\n", orgb); \
			return RET_ERROR; \
		} \
	} while (0)

#define check_argbh(orgb) \
	do { \
		if (! strchr("ARGBH", orgb)) { \
			syslog_error("Bad color %c (ARGBH).", orgb); \
			return RET_ERROR; \
		} \
	} while (0)

#define CHECK_RGB(orgb) \
	do { \
		if (! strchr("RGB", orgb)) { \
			syslog_error("Bad color %c (RGB).\n", orgb); \
			return NULL; \
		} \
	} while (0)

#define CHECK_ARGB(orgb) \
	do { \
		if (! strchr("ARGB", orgb)) { \
			syslog_error("Bad color %c (ARGB).\n", orgb); \
			return NULL; \
		} \
	} while (0)

#define CHECK_ARGBH(orgb) \
	do { \
		if (! strchr("ARGBH", orgb)) { \
			syslog_error("Bad color %c (ARGBH).", orgb); \
			return NULL; \
		} \
	} while (0)

/*
From Keith Jack's excellent book "Video Demystified" (ISBN 1-878707-09-4)
Y = 0.257R + 0.504G + 0.098B + 16
U = 0.148R - 0.291G + 0.439B + 128
V = 0.439R - 0.368G - 0.071B + 128

R = 1.164(Y - 16) + 1.596(V - 128)
G = 1.164(Y - 16) - 0.391(U - 128) - 0.813(V - 128)
B = 1.164(Y - 16) + 2.018(U - 128)
*/
#define YCBCR_TO_RGB( y, cb, cr, r, g, b )       \
do {                                                                  \
     int _y  = (y)  -  16;                                            \
     int _cb = (cb) - 128;                                            \
     int _cr = (cr) - 128;                                            \
                                                                      \
     int _r = ((298 * _y             + 409 * _cr + 128) >> 8);          \
     int _g = ((298 * _y - 100 * _cb - 208 * _cr + 128) >> 8);          \
     int _b = ((298 * _y + 516 * _cb             + 128) >> 8);          \
                                                                      \
     (r) = (BYTE)CLAMP( _r, 0, 255 );                                       \
     (g) = (BYTE)CLAMP( _g, 0, 255 );                                       \
     (b) = (BYTE)CLAMP( _b, 0, 255 );                                       \
} while (0)

#define RGB_TO_YCBCR( r, g, b, y, cb, cr )       \
do {                                                                  \
	int _r = (r), _g = (g), _b = (b);                    \
	                                                          \
	(y) = (66 * _r + 129 * _g + 25 * _b  +  16*256 + 128) >> 8; \
	(cb) = (-38 * _r  - 74 * _g + 112 * _b  + 128*256 + 128) >> 8; \
	(cr) = (112 * _r - 94 * _g  -18 * _b  + 128*256 + 128) >> 8; \
} while (0)

/*
* Y = 16 + 0.2568R + 0.5051G + 0.0979B   ==> (263, 517, 100)
*Cg = 128 - 0.318R  + 0.4392G - 0.1212B  ==> (-326, 450, -124)
*Cr = 128 + 0.4392R - 0.3677G - 0.0714B  ==> (450,  -377,  -73)
*/

#define RGB_TO_YCGCR( r, g, b, y, cg, cr )        \
do {                                                                  \
     int _r = (r), _g = (g), _b = (b);                       \
                                                                      \
     (y)  = (   263 * _r + 517 * _g + 100 * _b  +  16*1024) >> 10; \
     (cg) = ( - 326 * _r + 450 * _g - 124 * _b  + 128*1024) >> 10; \
     (cr) = (   450 * _r - 377 * _g -  73 * _b  + 128*1024) >> 10; \
} while (0)

#define RGB565_R(x) ((((x) >> 11) & 0x1f) << 3)
#define RGB565_G(x) ((((x) >> 5) & 0x3f) << 2)
#define RGB565_B(x) (((x) & 0x1f) << 3)
#define RGB565_NO(r, g, b) (((r)&0xf8) << 8 | ((g) & 0xfc) << 3 | ((b) & 0xf8) >> 3) 

// Lua module functions
int image_memsize(WORD h, WORD w);
void image_membind(IMAGE *img, WORD h, WORD w);

IMAGE *image_create(WORD h, WORD w);
IMAGE *image_load(char *fname);
IMAGE *image_copy(IMAGE *img);
IMAGE *image_zoom(IMAGE *img, int nh, int nw, int method);
IMAGE *image_hmerge(IMAGE *image1, IMAGE *image2);

int image_valid(IMAGE *img);
int image_clear(IMAGE *img);
int image_outdoor(IMAGE *img, int i, int di, int j, int dj);
int image_rectclamp(IMAGE *img, RECT *rect);
int image_save(IMAGE *img, const char *fname);

BYTE image_getvalue(IMAGE *img, char oargb, int r, int c);
void image_setvalue(IMAGE *img, char oargb, int r, int c, BYTE x);
MATRIX *image_getplane(IMAGE *img, char oargb);
int image_setplane(IMAGE *img, char oargb, MATRIX *mat);


void color_rgb2lab(BYTE R, BYTE G, BYTE B, double *L, double *a, double *b);
void color_rgb2hsv(BYTE R, BYTE G, BYTE B, BYTE *h, BYTE *s, BYTE *v);
void color_rgb2ycbcr(BYTE R, BYTE G, BYTE B, BYTE *y, BYTE *cb, BYTE *cr);
void color_rgb2ycgcr(BYTE R, BYTE G, BYTE B, BYTE *y, BYTE *cg, BYTE *cr);
void color_rgb2gray(BYTE r, BYTE g, BYTE b, BYTE *gray);
void color_rgb2luv(BYTE R, BYTE G, BYTE B, double *L, double *u, double *v);
void color_luv2rgb(double L, double u, double v, BYTE *R, BYTE *G, BYTE *B);
Luv *color_rgbf2luv(BYTE R, BYTE G, BYTE B);
	

int image_drawline(IMAGE *img, int r1, int c1, int r2, int c2, int color);  
int image_drawrect(IMAGE *img, RECT *rect, int color, int fill);
int image_drawtext(IMAGE *image, int r, int c, char *texts, int color);

int image_estimate(char oargb, IMAGE *orig, IMAGE *now, VECTOR *res);
int image_paste(IMAGE *img, int r, int c, IMAGE *small, double alpha);
int image_rect_paste(IMAGE *bigimg, RECT *bigrect, IMAGE *smallimg, RECT *smallrect);


int image_statistics(IMAGE *img, char orgb, double *avg, double *stdv);
int image_rect_statistics(IMAGE *img, RECT *rect, char orgb, double *avg, double *stdv);

void image_destroy(IMAGE *img);

int image_morph(IMAGE *img, char action);


// Image Color ...
int color_midval(IMAGE *img, char orgb);
VECTOR *color_vector(IMAGE *img, RECT *rect, int ndim);
double color_entropy(IMAGE *img, RECT *rect, int ndim);
double color_likeness(IMAGE *f, IMAGE *g, RECT *rect, int ndim);

// Image Shape ...
VECTOR *shape_vector(IMAGE *img, RECT *rect, int ndim);
double shape_entropy(IMAGE *img, RECT *rect, int ndim);
double shape_likeness(IMAGE *f, IMAGE *g, RECT *rect, int ndim);

// Image Texture ...
VECTOR *texture_vector(IMAGE *img, RECT *rect, int ndim);
double texture_entropy(IMAGE *img, RECT *rect, int ndim);
double texture_likeness(IMAGE *f, IMAGE *g, RECT *rect, int ndim);



// Contour & Skeleton
int image_contour(IMAGE *img);
int image_skeleton(IMAGE *img); 

// Middle value edge &  Canny edge
int shape_midedge(IMAGE *img);
int shape_bestedge(IMAGE *img);

// Hough Transform
int line_detect(IMAGE *img, int debug);
int motion_updatebg(IMAGE *A, IMAGE *B, IMAGE *C, IMAGE *bg);
int motion_detect(IMAGE *fg, IMAGE *bg, int debug);
int object_fast_detect(IMAGE *fg);

// Matter center
int image_mcenter(IMAGE *img, char orgb, int *crow, int *ccol);
int image_rect_mcenter(IMAGE *img, RECT *rect, char orgb, int *crow, int *ccol);

MATRIX *image_classmat(IMAGE *image);
VECTOR *image_voice(IMAGE *image, int rows, int cols, int levs, int binary);
void image_voice_show(IMAGE *image, int rows, int cols, int levs, int binary);

double image_likeness(IMAGE *a, IMAGE *b, int debug);
double image_cosine(IMAGE *a, IMAGE *b, int debug);
MATRIX *image_entropy(IMAGE *image, int rows, int cols, int levs, int debug);


IMAGE *image_subimg(IMAGE *img, RECT *rect);

void image_drawrects(IMAGE *img);
MATRIX *image_gstatics(IMAGE *img, int rows, int cols);

IMAGE *image_prydown(IMAGE *img);
IMAGE *image_pryup(IMAGE *img);

IMAGE *image_defisheye1(IMAGE *src, int cr, int cc, float rr, float rc, float zoom);
IMAGE *image_defisheye(IMAGE *src, double strength, double zoom);

#if defined(__cplusplus)
}
#endif

#endif	// __IMAGE_H
