
/************************************************************************************
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/


#ifndef _VECTOR_H
#define _VECTOR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "config.h"
#include "common.h"

// double Vector
typedef struct {
	DWORD magic;
	int m;
	double *ve;
} VECTOR;


#define vector_foreach(vec, i) \
	for (i = 0; i < (int)vec->m; i++)

#define check_vector(vec) \
	do { \
		if (! vector_valid(vec)) { \
			syslog_error("Bad vector."); \
			return RET_ERROR; \
		} \
	} while(0)

#define CHECK_VECTOR(vec) \
	do { \
		if (! vector_valid(vec)) { \
			syslog_error("Bad vector."); \
			return NULL; \
		} \
	} while(0)


VECTOR *vector_create(int m);

int vector_valid(VECTOR *v);
int vector_clear(VECTOR *vec);
double vector_sum(VECTOR *v);
int vector_normal(VECTOR *v);
int vector_normal2(VECTOR *v);

int vector_maxindex(VECTOR *v);
int vector_cosine(VECTOR *v1, VECTOR *v2, double *res);

double vector_likeness(VECTOR *v1, VECTOR *v2);
double vector_entropy(VECTOR *v);

// Hamming distance
int vector_hamming(VECTOR *v1, VECTOR *v2);

VECTOR *vector_gskernel(double sigma);

void vector_print(VECTOR *v, char *format);
void vector_destroy(VECTOR *v);
int vector_smooth(VECTOR *v);



#if defined(__cplusplus)
}
#endif

#endif	// _VECTOR_H

