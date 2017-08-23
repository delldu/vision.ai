/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Mon Aug  7 10:24:51 CST 2017
***
************************************************************************************/


#ifndef _PHASH_H
#define _PHASH_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"
#include "image.h"

#define PHASH uint64_t
typedef struct { int count; PHASH phash[MAX_OBJECT_NUM]; } PHASHS;

// PHash Set
PHASHS *phash_set();

PHASH phash_image(IMAGE *image, int debug);

int phash_hamming(PHASH f1, PHASH f2);

double phash_likeness(PHASH f1, PHASH f2);
double phash_maxlike(PHASH d, int n, PHASH *model);

int phash_objects(IMAGE *img);

#if defined(__cplusplus)
}
#endif

#endif	// _PHASH_H

