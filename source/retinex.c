/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Tue Jul 25 03:37:56 PDT 2017
***
************************************************************************************/

// Multiscale Retinex with Color Restoration

#include "retinex.h"
#include "matrix.h"
#include "filter.h"

// Single Scale
// R  (x,y) = log(I (x,y)) - log(I (x,y) * F(x,y)) 
static int __single_scale(MATRIX *mat, double sigma)
{
	int i, j;
	MATRIX *g;

//	CheckPoint("Sigma = %f", sigma);

	check_matrix(mat);
	g = matrix_copy(mat); check_matrix(g);
	matrix_gauss_filter(g, sigma);
	matrix_foreach(mat,i,j) {
		mat->me[i][j] = log(mat->me[i][j] + 1.0f) - log(g->me[i][j] + 1.0f);
	}
	matrix_destroy(g);

	return RET_OK;
}

// Multi Scale
static int __multi_scale(MATRIX *mat, int nscale, double *scales)
{
	int i, j, k;
	double weight;
	MATRIX *g, *out;

	check_matrix(mat);
	out = matrix_create(mat->m, mat->n); check_matrix(out);
	
	weight = 1.0f/nscale;
	
	for (k = 0; k < nscale; k++) {
		g = matrix_copy(mat); check_matrix(g);
		__single_scale(g, scales[k]);

		matrix_foreach(out,i,j)
			out->me[i][j] += weight * g->me[i][j];
		matrix_destroy(g);
	}
	memcpy(mat->base, out->base, out->m*out->n*sizeof(double));
	matrix_destroy(out);

	return RET_OK;
}

static int __color_restore(MATRIX *dst, MATRIX *src, MATRIX *gray)
{
	int i, j;
	double gain;
	
	check_matrix(dst);
	check_matrix(gray);

	matrix_foreach(dst,i,j) {
		gain = log(125.0f*(src->me[i][j] + 1.0f)) - log(3.0f*gray->me[i][j]);
		dst->me[i][j] *= gain;
	}

	return RET_OK;
}

static int __gain_offset(MATRIX *mat, double gain, double offset)
{
	int i, j;
	
	check_matrix(mat);
	matrix_foreach(mat,i,j) {
		mat->me[i][j] = gain*(mat->me[i][j] - offset);
	}

	return RET_OK;
}

static int __gimp_scale(MATRIX *mat, double sigma)
{
	double avg, stdv;
	double smin, smax;

	check_matrix(mat);
	matrix_statistics(mat, &avg, &stdv);
	smin = avg - sigma*stdv;
	smax = avg + sigma*stdv;

	matrix_scale(mat, smin, smax, 0.0f, 255.0f);

	return RET_OK;
}

int image_retinex(IMAGE *image, int nscale)
{
	int i, j;
	double scales[3] = {15, 80, 250};

	MATRIX *rmat, *gmat, *bmat, *gray, *src;

	check_image(image);

	rmat = image_getplane(image, 'R'); check_matrix(rmat);
	gmat = image_getplane(image, 'G'); check_matrix(gmat);
	bmat = image_getplane(image, 'B'); check_matrix(bmat);

	gray = matrix_create(image->height, image->width); check_matrix(gray);
	matrix_foreach(rmat,i,j) {
		gray->me[i][j] = 1.0f + (rmat->me[i][j] + gmat->me[i][j] + bmat->me[i][j])/3.0f;
	}
	__multi_scale(rmat, nscale, scales);
	__multi_scale(gmat, nscale, scales);
	__multi_scale(bmat, nscale, scales);

	src = image_getplane(image, 'R'); check_matrix(rmat);
	__color_restore(rmat, src, gray);
	matrix_destroy(src);

	src = image_getplane(image, 'G'); check_matrix(rmat);
	__color_restore(gmat, src, gray);
	matrix_destroy(src);

	src = image_getplane(image, 'B'); check_matrix(rmat);
	__color_restore(bmat, src, gray);
	matrix_destroy(src);
#if 1
	__gain_offset(rmat, 30, -6);
	__gain_offset(gmat, 30, -6);
	__gain_offset(bmat, 30, -6);
#else
	__gimp_scale(rmat, 3.0f);
	__gimp_scale(gmat, 3.0f);
	__gimp_scale(bmat, 3.0f);
#endif
	image_setplane(image, 'R', rmat);
	image_setplane(image, 'G', gmat);
	image_setplane(image, 'B', bmat);

	matrix_destroy(gray);
	matrix_destroy(bmat);
 	matrix_destroy(gmat);
	matrix_destroy(rmat);

	return RET_OK;
}

