/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Tue Jul 25 02:41:12 PDT 2017
***
************************************************************************************/


#include "histogram.h"

void histogram_reset(HISTOGRAM *h)
{
	h->total =0;
	memset(h->count, 0, HISTOGRAM_MAX_COUNT*sizeof(int));
}

void histogram_add(HISTOGRAM *h, int c)
{
	h->count[c]++;
	h->total++;
}

void histogram_del(HISTOGRAM *h, int c)
{
		h->count[c]--;
		h->total--;
}

int histogram_middle(HISTOGRAM *h)
{
	int i, halfsum, sum;

	sum = 0;
	halfsum = h->total/2;
	for (i = 0; i < HISTOGRAM_MAX_COUNT; i++) {
		sum += h->count[i];
		if (sum >= halfsum)
			return i;
	}

	// next is not impossiable
	return HISTOGRAM_MAX_COUNT/2;	// (0 + 255)/2
}

int histogram_top(HISTOGRAM *h, double ratio)
{
	int i, threshold, sum;

	sum = 0;
	threshold = (int)(ratio*h->total);
	
	for (i = HISTOGRAM_MAX_COUNT - 1; i >= 0; i--) {
		sum += h->count[i];
		if (sum >= threshold) {
			return i;
		}
	}

	return 0;
}

