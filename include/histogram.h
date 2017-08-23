/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Tue Jul 25 02:41:54 PDT 2017
***
************************************************************************************/


#ifndef _HISTOGRAM_H
#define _HISTOGRAM_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"

#define HISTOGRAM_MAX_COUNT 256

typedef struct {
 	int total;
	int count[HISTOGRAM_MAX_COUNT];
 } HISTOGRAM;

void histogram_reset(HISTOGRAM *h);
void histogram_add(HISTOGRAM *h, int c);
void histogram_del(HISTOGRAM *h, int c);
int histogram_middle(HISTOGRAM *h);
int histogram_top(HISTOGRAM *h, double ratio);

#if defined(__cplusplus)
}
#endif

#endif	// _HISTOGRAM_H

