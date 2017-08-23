/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Thu Jul 20 00:40:50 PDT 2017
***
************************************************************************************/


#ifndef _FILTER_H
#define _FILTER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#include "image.h"
#include "matrix.h"


// MS_BEGIN
MATRIX *matrix_box_filter(MATRIX *src, int r);
int matrix_guided_filter(MATRIX *mat, MATRIX *guidance, int radius, double eps);
int matrix_fast_guided_filter(MATRIX *P, MATRIX *I, int radius, double eps, int scale);
int matrix_beeps_filter(MATRIX *mat, double stdv, double dec);
int matrix_lee_filter(MATRIX *mat, int radius, double eps);
int matrix_minmax_filter(MATRIX *mat, int radius, int maxmode);

int matrix_gauss_filter(MATRIX *mat, double sigma);
int matrix_bilate_filter(MATRIX *mat, double hs, double hr);

int image_make_noise(IMAGE *img, char orgb, int rate);
int image_delete_noise(IMAGE *img);
int image_gauss_filter(IMAGE *image, double sigma);
int image_bilate_filter(IMAGE *image, double hs, double hr);
int image_guided_filter(IMAGE *img, IMAGE *guidance, int radius, double eps, int scale, int debug);
int image_beeps_filter(IMAGE *img, double stdv, double dec, int debug);
int image_lee_filter(IMAGE *img, int radius, double eps, int debug);
int image_dehaze_filter(IMAGE *img, int radius, int debug);
int image_medium_filter(IMAGE *img, int radius);

int image_fast_filter(IMAGE *img, int n, int *kernel, int total);
int image_gauss3x3_filter(IMAGE *img, RECT *rect);
int image_gauss5x5_filter(IMAGE *img, RECT *rect);
int image_dot_filter(IMAGE *img, int i, int j);
int image_rect_filter(IMAGE *img, RECT *rect, int n, int *kernel, int total);


// MS_END

#if defined(__cplusplus)
}
#endif

#endif	// _FILTER_H

