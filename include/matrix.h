
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#ifndef _MATRIX_H
#define _MATRIX_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"
#include "vector.h"


typedef struct {
	DWORD magic;
	int m, n, _m; 			// _m is internal rows
	double **me,*base; 
} MATRIX;

#define matrix_rect(rect, mat) \
	do { (rect)->r = 0; (rect)->c = 0; (rect)->h = mat->m; (rect)->w = mat->n; } while(0)

#define matrix_foreach(mat, i, j) \
	for (i = 0; i < mat->m; i++) \
		for (j = 0; j < mat->n; j++)

#define check_matrix(mat) \
	do { \
		if (! matrix_valid(mat)) { \
			syslog_error("Bad matrix."); \
			return RET_ERROR; \
		} \
	} while(0)

#define CHECK_MATRIX(mat) \
	do { \
		if (! matrix_valid(mat)) { \
			syslog_error("Bad matrix."); \
			return NULL; \
		} \
	} while(0)

// Lua interface
int matrix_memsize(DWORD m, DWORD n);
void matrix_membind(MATRIX *mat, DWORD m, DWORD n);

MATRIX *matrix_create(int m, int n);
MATRIX *matrix_load(char *fname);
MATRIX *matrix_copy(MATRIX *src);
MATRIX *matrix_zoom(MATRIX *mat, int nm, int nn, int method);
MATRIX *matrix_gskernel(double sigma);
MATRIX *matrix_wkmeans(MATRIX *mat, int k, distancef_t distance);

int matrix_valid(MATRIX *M);
int matrix_pattern(MATRIX *M, char *name);

int matrix_outdoor(MATRIX *M, int i, int di, int j, int dj);
int matrix_save(MATRIX *mat, char *fname);

int matrix_setvalue(MATRIX *mat, int row, int col, double val);

int matrix_integrate(MATRIX *mat);
double matrix_difference(MATRIX *mat, int r1, int c1, int r2, int c2);
int matrix_weight(MATRIX *mat, RECT *rect);


int matrix_clear(MATRIX *mat);
int matrix_normal(MATRIX *mat);

int matrix_tempxmin(MATRIX *mat, double x, MATRIX *temp);
int matrix_tempxmax(MATRIX *mat, double x, MATRIX *temp);
int matrix_morph(char action, MATRIX *mat, MATRIX *temp);
int matrix_localmax(MATRIX *mat, int r, int c);


// Make sure outmat exists
void matrix_region(MATRIX *mat, int r, int c, int theta, MATRIX *outmat);

// sort
int matrix_sort(MATRIX *A, int cols, int descend);

int matrix_clean(MATRIX *mat);

// Pryrmid down/up
MATRIX *matrix_pyrdown(MATRIX *mat);
MATRIX *matrix_pyrup(MATRIX *mat);

void matrix_print(MATRIX *m, char *format);

MATRIX *matrix_transpose(MATRIX *matrix);

// Dot add/sub/mul/div
int matrix_add(MATRIX *A, MATRIX *B);
int matrix_sub(MATRIX *A, MATRIX *B);
int matrix_mul(MATRIX *A, MATRIX *B);
int matrix_div(MATRIX *A, MATRIX *B);


void matrix_destroy(MATRIX *m);


#if defined(__cplusplus)
}
#endif

#endif	// _MATRIX_H

