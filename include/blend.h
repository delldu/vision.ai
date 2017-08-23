/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Wed Jun 14 18:06:15 PDT 2017
***
************************************************************************************/

#ifndef _BLEND_H
#define _BLEND_H

#if defined(__cplusplus)
extern "C" {
#endif


#include "image.h"

// Blending 
int blend_cluster(IMAGE *src, IMAGE *mask, IMAGE *dst, int top, int left, int debug);
IMAGE *blend_mutiband(IMAGE *a, IMAGE *b, IMAGE *mask, int layers, int debug);
IMAGE *blend_3x3codec(IMAGE *a, IMAGE *b, IMAGE *mask, int debug);


#if defined(__cplusplus)
}
#endif

#endif	// _BLEND_H

