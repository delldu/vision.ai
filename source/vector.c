
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vector.h"

#define VECTOR_MAGIC MAKE_FOURCC('V','E','C','T')


VECTOR *vector_create(int m)
{
	VECTOR *vec;

	if (m < 1) {
		syslog_error("Create vector with bad parameter.");
		return NULL;
	}

	vec = (VECTOR *)calloc((size_t)1, sizeof(VECTOR));
	if (! vec) {
		syslog_error("Allocate memeory.");
		return NULL;
	}

	vec->m = m;
	vec->ve = (double *)calloc((size_t)m, sizeof(double));
	if (! vec->ve) {
		syslog_error("Allocate memeory.");
		free(vec);
		return NULL;
	}

	vec->magic = VECTOR_MAGIC;

	return vec;
}


int vector_clear(VECTOR *vec)
{
	check_vector(vec);
	memset(vec->ve, 0, sizeof(double) * vec->m);
	return RET_OK;
}

void vector_destroy(VECTOR *v)
{
	if (! vector_valid(v))
		return;
	
	// FIX: Bug, Memorary leak !
	if (v->ve)
		free(v->ve);
	
	free(v);
}

void vector_print(VECTOR *v, char *format)
{
	int i, k;
	if (! vector_valid(v)) {
		syslog_error("Bad vector.");
		return;
	}

	for (i = 0; i < v->m; i++) {
		if (strchr(format, 'f'))	// double
			printf(format, v->ve[i]);
		else {
			k = (int)v->ve[i];
			printf(format, k);	// integer
		}
		printf("%s", (i < v->m - 1)?", " : "\n");
		// Too many data output on one line is not good idea !
		if ((i + 1) % 10 == 0)
			printf("\n");
	}
}

int vector_valid(VECTOR *v)
{
	return (v && v->m >= 1 && v->ve && v->magic == VECTOR_MAGIC);
}

int vector_cosine(VECTOR *v1, VECTOR *v2, double *res)
{
	int i;
	double s1, s2, s;

	if (! res) {
		syslog_error("Result pointer is null.");
		return RET_ERROR;
	}
	check_vector(v1); check_vector(v2);

	if (v1->m != v2->m) {
		syslog_error("Dimension of two vectors is not same.");
		return RET_ERROR;
	}
#if 0
	s1 = vector_sum(v1);
	s1 /= (double)v1->m;
	s2 = vector_sum(v2);
	s2 /= (double)v2->m;
	vector_foreach(v1,i) {
		v1->ve[i] -= s1;
		v2->ve[i] -= s2;
	}
#endif

	s1 = s2 = s = 0;
	vector_foreach(v1, i) {
		s1 += v1->ve[i] * v1->ve[i];
		s2 += v2->ve[i] * v2->ve[i];
		s += v1->ve[i] * v2->ve[i];
	}
	if (ABS(s1) < MIN_DOUBLE_NUMBER || ABS(s2) < MIN_DOUBLE_NUMBER) {
		syslog_error("Two vectors are zero.");
		*res = 1.0f;
		return RET_OK;
	}

	*res = (double)(s/(sqrt(s1) * sqrt(s2)));
	
	return RET_OK;
}

int vector_normal(VECTOR *v)
{
	int i;
	double sum;

	check_vector(v);

	sum = vector_sum(v);
	if (ABS(sum) < MIN_DOUBLE_NUMBER)  // NO Need calculation more
 		return RET_OK;
	
	vector_foreach(v, i)
		v->ve[i] /= sum;

	return RET_OK;
}

int vector_normal2(VECTOR *v)
{
	int i;
	double sum;

	check_vector(v);
	
	sum = 0;
	vector_foreach(v, i) 
		sum += (v->ve[i] * v->ve[i]);

	sum = sqrt(sum);
	if (ABS(sum) < MIN_DOUBLE_NUMBER) // NO Need calculation more
 		return RET_OK;
	
	vector_foreach(v, i)
		v->ve[i] /= sum;

	return RET_OK;
}

int vector_maxindex(VECTOR *v)
{
	int i, index;
	double d;

	check_vector(v);
	index = 0;
	d = v->ve[0];
	for (i = 1; i < v->m; i++) {
		if (v->ve[i] > d) {
			d = v->ve[i];
			index = i;
		}
	}
	return index;
}

double vector_likeness(VECTOR *v1, VECTOR *v2)
{
	double d;
	vector_cosine(v1, v2, &d);
	return d;
}

double vector_sum(VECTOR *v)
{
	int i;
	double sum;

	if (! vector_valid(v)) {
		syslog_error("Bad vector.");
		return 0.0f;
	}

	sum = 0.0f;
	vector_foreach(v, i) 
		sum += v->ve[i];

	return sum;
}

double vector_entropy(VECTOR *v)
{
	int i;
	double d, x, sum;

	if (! vector_valid(v)) {
		syslog_error("Bad vector.");
		return -1.0f;
	}

	sum = vector_sum(v);
	if(ABS(sum)< MIN_DOUBLE_NUMBER)
		return log(v->m)/log(2);

	d = 0.0f;
	for (i = 0; i < v->m; i++) {
		x = v->ve[i]/sum;
		
		if (x > MIN_DOUBLE_NUMBER)
			d += - x * log(x);
	}

	return d/log(2);
}

// Guass 1D kernel
VECTOR *vector_gskernel(double sigma)
{
	int  j, dim;
	double d, g;
	VECTOR *vec;

	dim = math_gsbw(sigma)/2;
	vec = vector_create(2 * dim + 1); CHECK_VECTOR(vec);
	d = sigma * sigma * 2.0f; 
	for (j = 0; j <= dim; j++) {
		g = exp(-(1.0f * j *j)/d);
		vec->ve[dim + j] = g;
		vec->ve[dim - j] = g;
	}
	vector_normal(vec);

	return vec;
}

// Hamming distance
int vector_hamming(VECTOR *v1, VECTOR *v2)
{
	int i, sum;
	check_vector(v1);
	check_vector(v2);

	sum = 0;
	for (i = 0; i < v1->m && i < v2->m; i++) {
		if (ABS(v1->ve[i] - v2->ve[i]) > MIN_DOUBLE_NUMBER)
			sum++;
	}
	return sum;
}


