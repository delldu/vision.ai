/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jun 17 11:19:14 CST 2017
***
************************************************************************************/


#ifndef _SEAM_H
#define _SEAM_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "matrix.h"
#include "image.h"

int *seam_program(MATRIX *mat, int debug);
int *seam_bestpath(IMAGE *image_a, RECT *rect_a, IMAGE *image_b, RECT *rect_b, int mode);

IMAGE *seam_bestmask(IMAGE *image_a, RECT *rect_a, IMAGE *image_b, RECT *rect_b, int mode);

int seam_test();


#if defined(__cplusplus)
}
#endif

#endif	// _SEAM_H

