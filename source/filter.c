/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Thu Jul 20 00:40:34 PDT 2017
***
************************************************************************************/


#include "filter.h"
#include "histogram.h"

static int __accumul_rows(MATRIX *mat)
{
	int i, j;

	for (j = 0; j < mat->n; j++) {
		for (i = 1; i < mat->m; i++)
			mat->me[i][j] += mat->me[i - 1][j];
	}

	return RET_OK;
}

static int __accumul_cols(MATRIX *mat)
{
	int i, j;

	for (i = 0; i < mat->m; i++) {
		for (j = 1; j < mat->n; j++)
			mat->me[i][j] += mat->me[i][j - 1];
	}

	return RET_OK;
}

// Forward filter
static void __beeps_progressive(int n, double *x, double stdv, double dec)
{
	int k;
	double mu = 0.0;
	double rho = 1.0 + dec;
	double c = -0.5f / (stdv * stdv);

	x[0] /= rho;
	for (k = 1; k < n; k++) {
		mu = x[k] - rho*x[k - 1];
		mu = dec * exp(c * mu * mu);
		x[k] = x[k - 1]*mu + x[k]*(1.0 - mu)/rho;
	}
}

static void __beeps_gain(int n, double *x, double dec)
{
	int k;
    double mu = (1.0f - dec) / (1.0f + dec);

	for (k = 0; k < n; k++)
		x[k] *= mu;
}

// Backward filter
static void __beeps_regressive(int n, double *x, double stdv, double dec)
{
	int k;
	double mu = 0.0;
	double rho = 1.0 + dec;
	double c = -0.5 / (stdv * stdv);

	x[n - 1] /= rho;
	for (k = n - 2; k >= 0; k--) {
		mu = x[k] - rho * x[k + 1];
		mu = dec * exp(c * mu * mu);
		x[k] = x[k + 1]*mu + x[k]*(1.0f - mu)/rho;
	}
}

// HV Progresss
static MATRIX *__beeps_hv(MATRIX *mat, double stdv, double dec)
{
	int i, j;
	double *x;
	MATRIX *p, *g, *r, *t;

	CHECK_MATRIX(mat);

	p = matrix_copy(mat); CHECK_MATRIX(p);
	g = matrix_copy(mat); CHECK_MATRIX(g);
	r = matrix_copy(mat); CHECK_MATRIX(r);

	for (i = 0; i < r->m; i++) {
		x = p->me[i];
		__beeps_progressive(r->n, x, stdv, 1.0f - dec);
		x = g->me[i];
		__beeps_gain(r->n, x, 1.0f - dec);
		x = r->me[i];
		__beeps_regressive(r->n, x, stdv, 1.0f - dec);
	}
	
	// r += p - g
	for (i = 0; i < r->m; i++) {
		for (j = 0; j < r->n; j++)
			r->me[i][j] += p->me[i][j] - g->me[i][j];
	}

	// Transpose 
	t = matrix_transpose(r); CHECK_MATRIX(t);

	matrix_destroy(p);
	matrix_destroy(g);
	matrix_destroy(r);
	
	p = matrix_copy(t); CHECK_MATRIX(p);
	g = matrix_copy(t); CHECK_MATRIX(g);
	r = matrix_copy(t); CHECK_MATRIX(r);
	matrix_destroy(t);

	for (i = 0; i < r->m; i++) {
		x = p->me[i];
		__beeps_progressive(r->n, x, stdv, 1.0f - dec);
		x = g->me[i];
		__beeps_gain(r->n, x, 1.0f - dec);
		x = r->me[i];
		__beeps_regressive(r->n, x, stdv, 1.0f - dec);
	}
	
	// r += p - g
	for (i = 0; i < r->m; i++) {
		for (j = 0; j < r->n; j++)
			r->me[i][j] += p->me[i][j] - g->me[i][j];
	}

	t = matrix_transpose(r); CHECK_MATRIX(t);

	matrix_destroy(p);
	matrix_destroy(g);
	matrix_destroy(r);

	return t;
}

// HV Progresss
static MATRIX *__beeps_vh(MATRIX *mat, double stdv, double dec)
{
	int i, j;
	double *x;
	MATRIX *p, *g, *r, *t;

	CHECK_MATRIX(mat);

	t = matrix_transpose(mat); CHECK_MATRIX(t);
	p = matrix_copy(t); CHECK_MATRIX(p);
	g = matrix_copy(t); CHECK_MATRIX(g);
	r = matrix_copy(t); CHECK_MATRIX(r);
	matrix_destroy(t);

	for (i = 0; i < r->m; i++) {
		x = p->me[i];
		__beeps_progressive(r->n, x, stdv, 1.0f - dec);
		x = g->me[i];
		__beeps_gain(r->n, x, dec);
		x = r->me[i];
		__beeps_regressive(r->n, x, stdv, 1.0f - dec);
	}
	
	// r += p - g
	for (i = 0; i < r->m; i++) {
		for (j = 0; j < r->n; j++)
			r->me[i][j] += p->me[i][j] - g->me[i][j];
	}

	// Transpose
	t = matrix_transpose(r);
	p = matrix_copy(t); CHECK_MATRIX(p);
	g = matrix_copy(t); CHECK_MATRIX(g);
	r = matrix_copy(t); CHECK_MATRIX(r);
	matrix_destroy(t);	

	for (i = 0; i < r->m; i++) {
		x = p->me[i];
		__beeps_progressive(r->n, x, stdv, 1.0f - dec);
		x = g->me[i];
		__beeps_gain(r->n, x, 1.0f - dec);
		x = r->me[i];
		__beeps_regressive(r->n, x, stdv, 1.0f - dec);
	}

	// r += p - g
	for (i = 0; i < r->m; i++) {
		for (j = 0; j < r->n; j++)
			r->me[i][j] += p->me[i][j] - g->me[i][j];
	}

	matrix_destroy(p);
	matrix_destroy(g);
	// matrix_destroy(r);

	return r;
}


// Create dark channel
static int __create_darkchan(IMAGE *img, int radius)
{
	BYTE min;
 	int i, j, i1, i2, j1, j2, k;
	
	check_image(img);

	// 1. Save dark channel AS channel A of image
	image_foreach(img,i,j) {
		if (img->ie[i][j].r > img->ie[i][j].g) {
			// max(r, b)
			img->ie[i][j].a = MAX(img->ie[i][j].r, img->ie[i][j].b);
		}
		else {
			img->ie[i][j].a = MAX(img->ie[i][j].g, img->ie[i][j].b);
		}
	}

	// 2. Dark Channel Min Filter
	// Col Filter
	for (i = 0; i < img->height; i++) {
		for (j = 0; j < img->width; j++) {
			j1 = MAX(j - radius, 0);
			j2 = MIN(j + radius, img->width - 1);
			min = img->ie[i][j1].a;
			for (k = j1 + 1; k <= j2; k++) {
				if (img->ie[i][k].a < min)
					min = img->ie[i][k].a;
			}
			img->ie[i][j].d = min;
		}
	}
	// Row Filter
	for (j = 0; j < img->width; j++) {
		for (i = 0; i < img->height; i++) {
			i1 = MAX(i - radius, 0);
			i2 = MIN(i + radius, img->height - 1);
			min = img->ie[i1][j].d;
			for (k = i1 + 1; k <= i2; k++) {
				if (img->ie[k][j].d < min)
					min = img->ie[k][j].d;
			}				
			img->ie[i][j].a = min;
		}
	}

	return RET_OK;
}

// P --, I -- guidance
int __guided_means(MATRIX *P, MATRIX *I, int radius, double eps, MATRIX *mean_a, MATRIX *mean_b)
{
	int i, j;
	
	MATRIX *zero, *one, *a, *b;
	MATRIX *mean_p, *mean_i, *mean_ii, *mean_ip;
	MATRIX *var_i, *cov_ip;
	
	check_matrix(P);
	check_matrix(I);

	if (P->m != I->m || P->n != I->n) {
		syslog_error("Matrix and guidance size is not same.");
		return RET_ERROR;
	}

	eps = eps * 255.0f * 255.0f;

	// zero is a temp matrix
	zero = matrix_create(P->m, P->n); check_matrix(zero);
	matrix_pattern(zero, "one");
	one = matrix_box_filter(zero, radius); check_matrix(one);

	// Step 1
	mean_p = matrix_box_filter(P, radius); check_matrix(mean_p);
	matrix_div(mean_p, one);
	mean_i = matrix_box_filter(I, radius); check_matrix(mean_i);
	matrix_div(mean_i, one);
	memcpy(zero->base, I->base, zero->m * I->n * sizeof(double));
	matrix_mul(zero, I);	// zero = I .* I
	mean_ii = matrix_box_filter(zero, radius); check_matrix(mean_ii);
	matrix_div(mean_ii, one);

	memcpy(zero->base, I->base, zero->m * I->n * sizeof(double));
	matrix_mul(zero, P); // zero = I .* P
	mean_ip = matrix_box_filter(zero, radius); check_matrix(mean_ip);
	matrix_div(mean_ip, one);

	// Step 2
	memcpy(zero->base, mean_i->base, zero->m * zero->n * sizeof(double));
	matrix_mul(zero, mean_i); // Zero = mean_i .* mean_i
	var_i = matrix_copy(mean_ii); check_matrix(var_i);
	matrix_sub(var_i, zero);
	
	// Zero = mean_i .* mean_p
	memcpy(zero->base, mean_i->base, zero->m * zero->n * sizeof(double));
	matrix_mul(zero, mean_p);
	cov_ip = matrix_copy(mean_ip); check_matrix(cov_ip);
	matrix_sub(cov_ip, zero);

	// Step 3.
	a = matrix_copy(cov_ip); check_matrix(a);
	b = matrix_copy(mean_p); check_matrix(b);
	// var_i += eps
	matrix_foreach(var_i,i,j)
		var_i->me[i][j] += eps;
	matrix_div(a, var_i);
	
	memcpy(zero->base, a->base, zero->m * zero->n * sizeof(double));
	matrix_mul(zero, mean_i); // zero = a .* mean_i
	matrix_sub(b, zero);

	// Step 4
	matrix_destroy(zero);
	zero = matrix_box_filter(a, radius);
	matrix_div(zero, one);
	memcpy(mean_a->base, zero->base, zero->m * zero->n * sizeof(double));

	matrix_destroy(zero);
	zero = matrix_box_filter(b, radius);
	matrix_div(zero, one);
	memcpy(mean_b->base, zero->base, zero->m * zero->n * sizeof(double));

	matrix_destroy(a);
	matrix_destroy(b);

	// Cov 
	matrix_destroy(var_i);
	matrix_destroy(cov_ip);

	// Corr
	matrix_destroy(mean_ip);
	matrix_destroy(mean_ii);

	// Mean
	matrix_destroy(mean_i);
	matrix_destroy(mean_p);

	matrix_destroy(one);
	matrix_destroy(zero);

	return RET_OK;
}

MATRIX *matrix_box_filter(MATRIX *src, int r)
{
	int i, j;
	MATRIX *mat, *sum;

	CHECK_MATRIX(src);
	
	mat = matrix_create(src->m, src->n); CHECK_MATRIX(mat);

	sum = matrix_copy(src); CHECK_MATRIX(sum);
	// 1. Update row
	__accumul_rows(sum);
	for (i = 0; i <= r; i++) {
		for (j = 0; j < mat->n; j++)
			mat->me[i][j] = sum->me[i + r][j];  //  	-0.0f
	}
	// [i - r, i + r]
	for (i = r + 1; i < mat->m - r; i++) {
		for (j = 0; j < mat->n; j++) {
			mat->me[i][j] = sum->me[i + r][j] - sum->me[i -r - 1][j];
		}
	}
	for (i = mat->m - r; i < mat->m; i++) {
		for (j = 0; j < mat->n; j++) {
			mat->me[i][j] = sum->me[mat->m - 1][j] - sum->me[i - r - 1][j];
		}
	}
	
	// 2. Update col
	memcpy(sum->base, mat->base, mat->m * mat->n * sizeof(double));
	__accumul_cols(sum);
	for (j = 0; j <= r; j++) {
		for (i = 0; i < mat->m; i++)
			mat->me[i][j] = sum->me[i][j + r];	// - 0.0f
	}

	for (j = r + 1; j < mat->n - r; j++) {
		for (i = 0; i < mat->m; i++)
			mat->me[i][j] = sum->me[i][j + r] - sum->me[i][j - r - 1];
	}
	for (j = mat->n - r; j < mat->n; j++) {
		for (i = 0; i < mat->m; i++)
			mat->me[i][j] = sum->me[i][mat->n - 1] - sum->me[i][j - r - 1];
	}

	matrix_destroy(sum);

	return mat;
}

// BEEPS
// stdv -- Photometric Standard Deviation, dec -- Spatial Contra Decay
int matrix_beeps_filter(MATRIX *mat, double stdv, double dec)
{
	int i, j;
	MATRIX *hv, *vh;
	
	check_matrix(mat);
	
	hv = __beeps_hv(mat, stdv, dec); check_matrix(hv);
	vh = __beeps_vh(mat, stdv, dec); check_matrix(vh);

	for (i = 0; i < mat->m; i++) {
		for (j = 0; j < mat->n; j++) {
			mat->me[i][j] = (hv->me[i][j] + vh->me[i][j])/2.0f;
		}
	}
	matrix_destroy(hv);
	matrix_destroy(vh);

	return RET_OK;
}


// NO Lua interface
// hs -- space sigma,  hr -- value sigma
int matrix_bilate_filter(MATRIX *mat, double hs, double hr)
 {
	int i, j, i2, j2, k, sdim;
	double s, d, v, w;
	MATRIX *skern, *temp;
	VECTOR *rsvec;		// range standard vector
	
	check_matrix(mat);

	skern = matrix_gskernel(hs); check_matrix(skern);
	sdim = skern->m/2;

	rsvec = vector_create(256); check_vector(rsvec);
	d = -0.5f/(hr * hr);
	for (j = 0; j < rsvec->m; j++)
		rsvec->ve[j] = exp(d * j * j);

	temp = matrix_copy(mat); check_matrix(temp);
	
	matrix_foreach(temp, i, j) {
		s = 0.0f;
		w = 0.0f;
		for (i2 = -sdim; i2 <= sdim; i2++) {
			for (j2 = -sdim; j2 <= sdim; j2++) {
				if (matrix_outdoor(temp, i, i2, j, j2)) {
					v = temp->me[i][j]; d = 0;
				}
				else {
					v = temp->me[i + i2][j + j2];
					d = v - temp->me[i][j];
				}
				k = (int)ABS(d); k = MIN(k, 255);
				d = rsvec->ve[k];
				w += skern->me[sdim + i2][sdim + j2] * d;
				s += skern->me[sdim + i2][sdim + j2] * d * v;				
 			}
		}
		if (ABS(w) > MIN_DOUBLE_NUMBER)
			s /= w;
		mat->me[i][j] = s;
	}
	
	vector_destroy(rsvec);
	matrix_destroy(skern);
	matrix_destroy(temp);

	return RET_OK;
}


// P --, I -- guidance
int matrix_fast_guided_filter(MATRIX *P, MATRIX *I, int radius, double eps, int scale)
{
	int ret;

	MATRIX *small_P, *small_I;
	MATRIX *small_mean_a, *small_mean_b;

	small_P = matrix_zoom(P, P->m/scale, P->n/scale, 0); check_matrix(small_P);
	small_I = matrix_zoom(I, P->m/scale, P->n/scale, 0); check_matrix(small_I);
	
	small_mean_a = matrix_create(small_P->m, small_P->n); check_matrix(small_mean_a);
	small_mean_b = matrix_create(small_P->m, small_P->n); check_matrix(small_mean_b);

	ret = __guided_means(small_P, small_I, radius, eps, small_mean_a, small_mean_b);
	if (ret == RET_OK) {
		MATRIX *mean_a, *mean_b;

		// Step 5
		// mean_a .* I + mean_b
		mean_a = matrix_zoom(small_mean_a, P->m, P->n, 0);
		mean_b = matrix_zoom(small_mean_b, P->m, P->n, 0);

		matrix_mul(mean_a, I);
		
		matrix_add(mean_a, mean_b);
		
		memcpy(P->base, mean_a->base, mean_a->m * mean_a->n * sizeof(double));

		matrix_destroy(mean_a);
		matrix_destroy(mean_b);
	}

	matrix_destroy(small_mean_a);
	matrix_destroy(small_mean_b);

	matrix_destroy(small_P);
	matrix_destroy(small_I);

	return ret;
}


// NO Lua interface
// sigma = 0.5: convert kernel = 5x5
// sigma = 1: convert kernel = 7x7
// sigma = 2: convert kernel = 13x13
int matrix_gauss_filter(MATRIX *mat, double sigma)
{
	int i, j, m, k;
	double d;
	VECTOR *vec;
	MATRIX *temp;

	check_matrix(mat);
	vec = vector_gskernel(sigma); check_vector(vec);
	m = vec->m/2;
	temp = matrix_create(mat->m, mat->n); check_matrix(temp);

	// Col Conv
	matrix_foreach(mat, i, j) {
		d = 0;
		for (k = -m; k <= m; k++) {
			if (k + j >= 0 && k + j < mat->n)
				d += mat->me[i][k + j] * vec->ve[k + m];
			else
				d += mat->me[i][j] * vec->ve[k + m];
		}
		temp->me[i][j] = d;
	}

	// Row Conv
	matrix_foreach(mat, i, j) {
		d = 0;
		for (k = -m; k <= m; k++) {
			if (k + i >= 0 && k + i < mat->m)
				d += temp->me[k + i][j] * vec->ve[k + m];
			else
				d += temp->me[i][j] * vec->ve[k + m];
		}
		mat->me[i][j] = d;
	}

	matrix_destroy(temp);

	return RET_OK;
}

// P --, I -- guidance
int matrix_guided_filter(MATRIX *P, MATRIX *I, int radius, double eps)
{
	int ret;
	MATRIX *mean_a, *mean_b;

	mean_a = matrix_create(P->m, P->n); check_matrix(mean_a);
	mean_b = matrix_create(P->m, P->n); check_matrix(mean_b);
	
	ret = __guided_means(P, I, radius, eps, mean_a, mean_b);
	if (ret == RET_OK) {
		// Step 5
		// mean_a .* I + mean_b
		matrix_mul(mean_a, I);
		matrix_add(mean_a, mean_b);
		
		memcpy(P->base, mean_a->base, mean_a->m * mean_a->n * sizeof(double));
	}

	matrix_destroy(mean_a);
	matrix_destroy(mean_b);

	return ret;
}


int matrix_lee_filter(MATRIX *mat, int radius, double eps)
{
	int i, j;
	
	MATRIX *dst, *zero, *one;
	MATRIX *mean_mat, *mean_mat_mat, *k_mat;
	
	check_matrix(mat);

	eps = eps * 255.0f * 255.0f;

	// zero is a temp matrix
	zero = matrix_create(mat->m, mat->n); check_matrix(zero);
	matrix_pattern(zero, "one");
	one = matrix_box_filter(zero, radius); check_matrix(one);

	// Step 1
	mean_mat = matrix_box_filter(mat, radius); check_matrix(mean_mat);
	matrix_div(mean_mat, one);
	
	memcpy(zero->base, mat->base, zero->m * zero->n * sizeof(double));
	matrix_mul(zero, mat);	// zero = mat .* mat
	mean_mat_mat = matrix_box_filter(zero, radius); check_matrix(mean_mat_mat);
	matrix_div(mean_mat_mat, one);
	matrix_sub(mean_mat_mat, mean_mat);

	k_mat = matrix_copy(mean_mat_mat); check_matrix(k_mat);
	memcpy(zero->base, mean_mat_mat->base, zero->m * zero->n * sizeof(double));
	matrix_foreach(zero,i,j) {
		zero->me[i][j] += eps;
	}
	matrix_div(k_mat, zero);	// K = mean_mat_mat/(mean_mat_mat + eps)

	// zero = (1 - k) * mean_mat
	memcpy(zero->base, k_mat->base, zero->m * zero->n * sizeof(double));
	matrix_foreach(zero,i,j) {
		zero->me[i][j] = 1.0f - zero->me[i][j];
	}
	matrix_mul(zero, mean_mat);

	// 	dst = (1 - k) * mean_mat + k * mat
	dst = matrix_copy(zero); check_matrix(dst);
	memcpy(zero->base, k_mat->base, zero->m * zero->n * sizeof(double));
	matrix_mul(zero, mat);
	matrix_add(dst, zero);

	// Save dst to mat
	memcpy(mat->base, dst->base, mat->m * mat->n * sizeof(double));

	matrix_destroy(dst);
	matrix_destroy(k_mat);
	matrix_destroy(mean_mat_mat);
	matrix_destroy(mean_mat);
	matrix_destroy(one);
	matrix_destroy(zero);

	return RET_OK;
}


int matrix_minmax_filter(MATRIX *mat, int radius, int maxmode)
{
	int i, j, i1, i2, j1, j2, k;
	double d;
	MATRIX *copy;

	check_matrix(mat);
	copy = matrix_copy(mat); check_matrix(copy);

	if (maxmode) {
		// MAX Col Filter
		for (i = 0; i < copy->m; i++) {
			for (j = 0; j < copy->n; j++) {
				j1 = MAX(j - radius, 0);
				j2 = MIN(j + radius, copy->n - 1);
				d = mat->me[i][j1];
				for (k = j1 + 1; k <= j2; k++) {
					if (mat->me[i][k] > d)
						d = mat->me[i][k];
				}
				copy->me[i][j] = d;
			}
		}
		// Row Filter


		for (j = 0; j < mat->n; j++) {
			for (i = 0; i < mat->m; i++) {
				i1 = MAX(i - radius, 0);
				i2 = MIN(i + radius, mat->m - 1);
				d = copy->me[i1][j];
				for (k = i1 + 1; k <= i2; k++) {
					if (copy->me[k][j] > d)
						d = copy->me[k][j];
				}				
				mat->me[i][j] = d;
			}
		}
	}
	else {
		// MIN Col Filter
		for (i = 0; i < copy->m; i++) {
			for (j = 0; j < copy->n; j++) {
				j1 = MAX(j - radius, 0);
				j2 = MIN(j + radius, copy->n - 1);
				d = mat->me[i][j1];
 				for (k = j1 + 1; k <= j2; k++) {
					if (mat->me[i][k] < d)
						d = mat->me[i][k];
				}
				copy->me[i][j] = d;
			}
		}
		// Row Filter
		for (j = 0; j < mat->n; j++) {
			for (i = 0; i < mat->m; i++) {
				i1 = MAX(i - radius, 0);
				i2 = MIN(i + radius, mat->m - 1);
				d = copy->me[i1][j];
				for (k = i1 + 1; k <= i2; k++) {
					if (copy->me[k][j] < d)
						d = copy->me[k][j];
				}				
				mat->me[i][j] = d;
			}
		}
	}

	matrix_destroy(copy);

	return RET_OK;
}

int image_beeps_filter(IMAGE *img, double stdv, double dec, int debug)
{
	MATRIX *mat;
	
	check_image(img);

	if (debug) {
		time_reset();
	}
	// R Channel
	mat = image_getplane(img, 'R'); check_matrix(mat);
	matrix_beeps_filter(mat, stdv, dec);
	image_setplane(img, 'R', mat);
	matrix_destroy(mat);

	// G Channel
	mat = image_getplane(img, 'G'); check_matrix(mat);
	matrix_beeps_filter(mat, stdv, dec);
	image_setplane(img, 'G', mat);
	matrix_destroy(mat);

	// B Channel
	mat = image_getplane(img, 'B'); check_matrix(mat);
	matrix_beeps_filter(mat, stdv, dec);
	image_setplane(img, 'B', mat);
	matrix_destroy(mat);

	if (debug) {
		time_spend("BEEPS filter");
	}
	
	return RET_OK;
}

int image_lee_filter(IMAGE *img, int radius, double eps, int debug)
{
	MATRIX *mat;
	
	check_image(img);

	if (debug) {
		time_reset();
	}
	// R Channel
	mat = image_getplane(img, 'R'); check_matrix(mat);
	matrix_lee_filter(mat, radius, eps);
	image_setplane(img, 'R', mat);
	matrix_destroy(mat);

	// G Channel
	mat = image_getplane(img, 'G'); check_matrix(mat);
	matrix_lee_filter(mat, radius, eps);
 	image_setplane(img, 'G', mat);
	matrix_destroy(mat);

	// B Channel
	mat = image_getplane(img, 'B'); check_matrix(mat);
	matrix_lee_filter(mat, radius, eps);
 	image_setplane(img, 'B', mat);
	matrix_destroy(mat);

	if (debug) {
		time_spend("Lee filter");
	}
	
	return RET_OK;
}

int image_gauss_filter(IMAGE *image, double sigma)
{
	int i, j;
	MATRIX *mat;
	
	check_image(image);

	if (image->format == IMAGE_GRAY) {
		mat = image_getplane(image, 'R');  check_matrix(mat);
		matrix_gauss_filter(mat, sigma);
		image_setplane(image, 'R', mat);
		matrix_destroy(mat);
		image_foreach(image,  i, j)
			image->ie[i][j].g = image->ie[i][j].b = image->ie[i][j].r;
	}
	else {
		mat = image_getplane(image, 'R');  check_matrix(mat);
		matrix_gauss_filter(mat, sigma);
		image_setplane(image, 'R', mat);
		matrix_destroy(mat);

		mat = image_getplane(image, 'G');  check_matrix(mat);
		matrix_gauss_filter(mat, sigma);
		image_setplane(image, 'G', mat);
		matrix_destroy(mat);

		mat = image_getplane(image, 'B');  check_matrix(mat);
		matrix_gauss_filter(mat, sigma);
		image_setplane(image, 'B', mat);
		matrix_destroy(mat);
	}
	
	return RET_OK;
}

int image_bilate_filter(IMAGE *image, double hs, double hr)
{
	int i, j;
	MATRIX *mat;
	
	check_image(image);

	if (image->format == IMAGE_GRAY) {
		mat = image_getplane(image, 'R');  check_matrix(mat);
		matrix_bilate_filter(mat, hs, hr);
		image_setplane(image, 'R', mat);
		matrix_destroy(mat);
		image_foreach(image,  i, j)
			image->ie[i][j].g = image->ie[i][j].b = image->ie[i][j].r;
	}
	else {
		mat = image_getplane(image, 'R');  check_matrix(mat);
		matrix_bilate_filter(mat, hs, hr);
		image_setplane(image, 'R', mat);
		matrix_destroy(mat);

		mat = image_getplane(image, 'G');  check_matrix(mat);
		matrix_bilate_filter(mat, hs, hr);
		image_setplane(image, 'G', mat);
		matrix_destroy(mat);

		mat = image_getplane(image, 'B');  check_matrix(mat);
		matrix_bilate_filter(mat, hs, hr);
		image_setplane(image, 'B', mat);
		matrix_destroy(mat);
	}
	
	return RET_OK;
}

int image_guided_filter(IMAGE *img, IMAGE *guidance, int radius, double eps, int scale, int debug)
{
	MATRIX *mat_img, *mat_guidance;
	
	check_image(img);

	if (! guidance) {
		guidance = img;
	}
	
	check_image(guidance);

	if (debug)
		time_reset();

	// R Channel
	mat_img = image_getplane(img, 'R'); check_matrix(mat_img);
	mat_guidance = image_getplane(guidance, 'R'); check_matrix(mat_guidance);
	if (scale > 1) {
		matrix_fast_guided_filter(mat_img, mat_guidance, radius, eps, scale);
	}
	else {
		matrix_guided_filter(mat_img, mat_guidance, radius, eps);
	}
	image_setplane(img, 'R', mat_img);
	matrix_destroy(mat_guidance);
	matrix_destroy(mat_img);

	// G Channel
	mat_img = image_getplane(img, 'G'); check_matrix(mat_img);
	mat_guidance = image_getplane(guidance, 'G'); check_matrix(mat_guidance);
	if (scale > 1) {
		matrix_fast_guided_filter(mat_img, mat_guidance, radius, eps, scale);
	}
	else {
		matrix_guided_filter(mat_img, mat_guidance, radius, eps);
	}
	image_setplane(img, 'G', mat_img);
	matrix_destroy(mat_guidance);
	matrix_destroy(mat_img);

	// B Channel
	mat_img = image_getplane(img, 'B'); check_matrix(mat_img);
	mat_guidance = image_getplane(guidance, 'B'); check_matrix(mat_guidance);
	if (scale > 1) {
		matrix_fast_guided_filter(mat_img, mat_guidance, radius, eps, scale);
	}
	else {
		matrix_guided_filter(mat_img, mat_guidance, radius, eps);
	}
	image_setplane(img, 'B', mat_img);
	matrix_destroy(mat_guidance);
	matrix_destroy(mat_img);

	if (debug)
		time_spend("Guided filter.");

	return RET_OK;
}

int image_dehaze_filter(IMAGE *img, int radius, int debug)
{
	HISTOGRAM hist;
 	int i, j, k, al;
	double red_al, green_al, blue_al, d, d1, d2, d3;	// al -- atmoslight
 	MATRIX *tx;
	
	check_image(img);

	if (debug)
		time_reset();

	__create_darkchan(img, radius);

	// 3. Get atmos light
	histogram_reset(&hist);
	image_foreach(img,i,j)
		histogram_add(&hist, img->ie[i][j].a);
	al = histogram_top(&hist, 0.001f);	// 0.1%
	
	red_al = green_al = blue_al = 0.0f;
	k = 0;
	image_foreach(img,i,j) {
		if (img->ie[i][j].a >= al) {
			red_al += img->ie[i][j].r;
			green_al += img->ie[i][j].g;
			blue_al += img->ie[i][j].b;
			k++;
		}
	}
	if (k > 0) {
		red_al /= k;
		green_al /= k;
		blue_al /= k;
	}
	if (red_al < MIN_DOUBLE_NUMBER)
		red_al = 1.0f;
	if (green_al < MIN_DOUBLE_NUMBER)
		green_al = 1.0f;
	if (blue_al < MIN_DOUBLE_NUMBER)
		blue_al = 1.0f;
	red_al = MIN(red_al, 220);
	green_al = MIN(green_al, 220);
	blue_al = MIN(blue_al, 220);

	// 4. Create light transmission matrix
	tx = matrix_create(img->height, img->width); check_matrix(tx);
	matrix_foreach(tx,i,j) {
		d1 = 1.0f*img->ie[i][j].r/red_al;
		d2 = 1.0f*img->ie[i][j].g/green_al;
		d3 = 1.0f*img->ie[i][j].b/blue_al;

		if (d1 > d2)
 			d = MIN(d2, d3);
 		else
			d = MIN(d1, d3);
		tx->me[i][j] = d;
	}

	// tx min filter
	matrix_minmax_filter(tx, radius, 0);	// 0 -- min filter

	// tx = 1 - w*..
	double w = 0.95f;
	matrix_foreach(tx,i,j) {
		d = 1.0f - w*tx->me[i][j];
		tx->me[i][j] = MAX(d, 0.1f); // threshold;
	}

	// Update image
	// J = (I - A)/tx + A
	image_foreach(img,i,j) {
		d = (1.0f*img->ie[i][j].r - red_al)/tx->me[i][j] + red_al;
		img->ie[i][j].r = CLAMP((int)d, 0, 255);

		d = (1.0f*img->ie[i][j].g - green_al)/tx->me[i][j] + green_al;
		img->ie[i][j].g = CLAMP((int)d, 0, 255);

		d = (1.0f*img->ie[i][j].b - blue_al)/tx->me[i][j] + blue_al;
		img->ie[i][j].b = CLAMP((int)d, 0, 255);
	}
	
	matrix_destroy(tx);

	if (debug)
		time_spend("Dehazing filter");

	return RET_OK;
}

// A channel medium filter
int image_medium_filter(IMAGE *img, int radius)
{
	HISTOGRAM hist;
 	int i, j, i1, i2, i3, j1, j2, j3, k;
	
	check_image(img);

	// Channel a Median Filter
	for (i = 0; i < img->height; i++) {
		histogram_reset(&hist);
		j = 0;
		i1 = MAX(i - radius, 0);
		i2 = MIN(i + radius, img->height - 1);
		j1 = MAX(j - radius, 0);
		j2 = MIN(j + radius, img->width - 1);

		for (i3 = i1; i3 <= i2; i3++) {
			for (j3 = j1; j3 <= j2; j3++)
				histogram_add(&hist, img->ie[i3][j3].a);
		}
		img->ie[i][0].d = histogram_middle(&hist);

		for (j = 1; j < img->width; j++) {
			// Update histogram ? 
			if (j1 >= 0) {	// Delete left column
				for (k = i1; k <= i2; k++)
					histogram_del(&hist, img->ie[k][j1].a);
			}
			j1++; j2++;
			if (j2 < img->width) {	// Add right column
				for (k = i1; k <= i2; k++)
					histogram_add(&hist, img->ie[k][j2].a);
			}
			img->ie[i][j].d = histogram_middle(&hist);
		}
	}

	// Save filter result
	image_foreach(img,i,j)
		img->ie[i][j].a = img->ie[i][j].d;

	return RET_OK;
}

// [1 2 1], 1/16
int image_gauss3x3_filter(IMAGE *img, RECT *rect)
{
	int k3x3[3] = {1, 2, 1};
	return image_rect_filter(img, rect, ARRAY_SIZE(k3x3), k3x3, 16);
}

int image_gauss5x5_filter(IMAGE *img, RECT *rect)
{
	// [1 4 7 4 1]/273
	int k5x5[5] = {1, 4, 6, 4, 1};
	return image_rect_filter(img, rect, ARRAY_SIZE(k5x5), k5x5, 256);
}

int image_dot_filter(IMAGE *img, int i, int j)
{
	RECT rect;
	int kernel[3] = {1, 2, 1};

	rect.r = i - 1;
	rect.c = j - 1;
	rect.w = rect.h = 3;

	image_rectclamp(img, &rect);
	return image_rect_filter(img, &rect, ARRAY_SIZE(kernel), kernel, 16);
}


// Guass 3x3, [1 2 1], 16
// Guass 5x5, [1 4 6 4 1], 256
int image_fast_filter(IMAGE *img, int n, int *kernel, int total)
{
	RECT rect;
	image_rect(&rect, img);
	return image_rect_filter(img, &rect, n, kernel, total);
}

int image_rect_filter(IMAGE *img, RECT *rect, int n, int *kernel, int total)
{
	int i, j, k, m;

	check_image(img);

	image_rectclamp(img, rect);

	if (rect->h < n || rect->w < n) {
//		syslog_debug("NO need to filter.");
		return RET_ERROR;
	}

	n /= 2;
	// R Channel Col Conv
	for (i = 0; i < rect->h; i++) {
		for (j = n; j < rect->w - n; j++) {
			m = 0;
			for (k = -n; k <= n; k++) {
				m += kernel[k + n]*img->ie[i + rect->r][j + rect->c + k].r;
			}
			img->ie[i + rect->r][j + rect->c].d = m;
		}
	}
	// R Channel Row Conv
	for (j = n; j < rect->w - n; j++) {
		for (i = n; i < rect->h - n; i++) {
			m = 0;
			for (k = -n; k <= n; k++) {
				m += kernel[k + n]*img->ie[i + rect->r + k][j + rect->c].d;
			}				
			m /= total;
			img->ie[i + rect->r][j + rect->c].r = (BYTE)(m);
		}
	}

	// G Channel Col Conv
	for (i = 0; i < rect->h; i++) {
		for (j = n; j < rect->w - n; j++) {
			m = 0;
			for (k = -n; k <= n; k++)
				m += kernel[k + n]*img->ie[i + rect->r][j + rect->c + k].g;
			img->ie[i + rect->r][j + rect->c].d = m;
		}
	}
	// G Channel Row Conv
	for (j = n; j < rect->w - n; j++) {
		for (i = n; i < rect->h - n; i++) {
			m = 0;
			for (k = -n; k <= n; k++)
				m += kernel[k + n]*img->ie[i + rect->r + k][j + rect->c].d;
			m /= total;

			img->ie[i + rect->r][j + rect->c].g = (BYTE)(m);
		}
	}

	// B Channel Col Conv
	for (i = 0; i < rect->h; i++) {
		for (j = n; j < rect->w - n; j++) {
			m = 0;
			for (k = -n; k <= n; k++)
				m += kernel[k + n]*img->ie[i + rect->r][j + rect->c + k].b;
			img->ie[i + rect->r][j + rect->c].d = m;
		}
	}
	// B Channel Row Conv
	for (j = n; j < rect->w - n; j++) {
		for (i = n; i < rect->h - n; i++) {
			m = 0;
			for (k = -n; k <= n; k++)
				m += kernel[k + n]*img->ie[i + rect->r + k][j + rect->c].d;
			m /= total;

			img->ie[i + rect->r][j + rect->c].b = (BYTE)(m);
		}
	}

	return RET_OK;
}


