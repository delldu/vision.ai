/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Wed Jun 21 20:20:30 PDT 2017
***
************************************************************************************/


#ifndef _CAMERA_H
#define _CAMERA_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"
#include "image.h"
#include "matrix.h"

typedef struct {
    double fx, fy, cx, cy;
    double k0, k1;           /* Distortion*/
	
    double r[3 * 3];         /* Rotation*/
    double t[3];             /* Translation*/

	// Maps from image to world
	IMAGE *image;			// 2D Image, Final image
	RECT world;				// World 3D

	MATRIX *r_map, *c_map;

	double r_gain, g_gain, b_gain;
} CAMERA;

int camera_height(CAMERA *cam);
int camera_width(CAMERA *cam);
IMAGE *camera_image(CAMERA *cam);

int camera_create(CAMERA *cam, int height, int width);
int camera_loadconf(FILE *fp, CAMERA *c);
int camera_saveconf(CAMERA *c, FILE *fp);
int camera_mapworld(CAMERA *cam, RECT *world, int scale);
int camera_defisheye(CAMERA *cam, IMAGE *src, RECT *rect);
int camera_learning(CAMERA *cam);
int camera_rect_gain(CAMERA *cam, IMAGE *src, RECT *rect);
int camera_awb(CAMERA *cam);
int camera_filter(CAMERA *cam);
void camera_destroy(CAMERA *cam);


#if defined(__cplusplus)
}
#endif

#endif	// _CAMERA_H

