/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Thu Jul 20 01:38:46 PDT 2017
***
************************************************************************************/


#ifndef _COLOR_H
#define _COLOR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>


// MS_BEGIN
// Color applications ...
int skin_detect(IMAGE *img);
int skin_statics(IMAGE *img, RECT *rect);

int color_cluster(IMAGE *image, int num, int update);
int color_beskin(BYTE r, BYTE g, BYTE b);
int *color_count(IMAGE *image, int rows, int cols, int levs);
int color_picker();
int color_balance(IMAGE *img, int method, int debug);

int color_gwmgain(IMAGE *img, double *r_gain, double *g_gain, double *b_gain);
int color_rect_gwmgain(IMAGE *img, RECT *rect, double *r_gain, double *g_gain, double *b_gain);

int color_prmgain(IMAGE *img, double *r_gain, double *g_gain, double *b_gain);
int color_rect_prmgain(IMAGE *img, RECT *rect, double *r_gain, double *g_gain, double *b_gain);

int color_correct(IMAGE *img, double gain_r, double gain_g, double gain_b);
int color_togray(IMAGE *img);
int color_torgb565(IMAGE *img);
double color_distance(RGB *c1, RGB *c2);


// MS_END

#if defined(__cplusplus)
}
#endif

#endif	// _COLOR_H

