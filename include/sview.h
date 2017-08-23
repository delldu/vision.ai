/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Thu Jun 22 20:33:05 PDT 2017
***
************************************************************************************/


#ifndef _SVIEW_H
#define _SVIEW_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "camera.h"

int blend_height();
int car_width();
int car_height();
int front_height();
int front_width();
int left_height();
int left_width();
int right_height();
int right_width();
int rear_height();
int rear_width();

CAMERA *sview_camera(int no);
IMAGE *sview_image();
int sview_start(char *config);

void sview_frame_capture(IMAGE *src, int debug);
void sview_color_correct(int debug);
void sview_space_align(int debug);

void sview_micro_blend(int debug);
void sview_alpha_blend(int debug);
int sview_global_blend(int layers, int debug);

int sview_blend(IMAGE *src, int blend_method, int debug);
void sview_stop();



// MS_END

#if defined(__cplusplus)
}
#endif

#endif	// _SVIEW_H

