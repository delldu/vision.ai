
/************************************************************************************
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

// SIFT =  Scale Invariable Feature Transform, invented by Lowe


#include "vector.h"
#include <math.h>

#include "image.h"
#include "filter.h"

#define SIFT_OCTAVE_NUMBER 4
#define SIFT_SCALE_NUMBER 4

#define SIFT_INIT_S (0.707106781186548f)	// sqrt(2)/2
#define SIFT_INIT_K (1.4142135623731f)	// sqrt(2)
#define SIFT_DEMENSION 128				// 128 or 32
#define SIFT_DOG_THRESHOLD 0.04
#define SIFT_MATCH_THRESHOLD 0.50		// [0.20, 0.80]

// Model: (r, c, theta, scale, ... 128 ...)
#define SIFT_ROW_INDEX 0
#define SIFT_COL_INDEX 1
#define SIFT_DIR_INDEX 2
#define SIFT_SCALE_INDEX 3
#define SIFT_HEAD_SIZE 4

// ==> SIFT_INIT_S * 2^o * (SIFT_INIT_K)^s
static double __sift_sigma(int o, int s)
{
	int i;
	double d = SIFT_INIT_S;
	for (i = 0; i < o; i++)
		d *= 2.0f;
	for (i = 0; i < s; i++)
		d *= SIFT_INIT_K;
	return d;
}

// ==> 2^o
static double __scale_ratio(int o)
{
	int i;
	double d = 1.0f;
	for (i = 0; i < o; i++)
		d *= 2.0f;
	return d;
}

// Make sure bot, mid, top and r,c are valid
static int __local_3dmax(MATRIX *bot, MATRIX *mid, MATRIX *top, int r, int c)
{
	int i, j;
	double d = mid->me[r][c];
	for (i = -1; i <= 1; i++) {
		for (j = -1; j <= 1; j++) {
			if (bot->me[r + i][c + j] >= d || top->me[r + i][c + j] >= d)
				return 0;
			if ((i != 0 || j != 0) && mid->me[r + i][c + j] >= d)
				return 0;
		}
	}
	return 1;	
}

static int __local_3dmin(MATRIX *bot, MATRIX *mid, MATRIX *top, int r, int c)
{
	int i, j;
	double d = mid->me[r][c];
	for (i = -1; i <= 1; i++) {
		for (j = -1; j <= 1; j++) {
			if (bot->me[r + i][c + j] <= d ||  top->me[r + i][c + j] <= d)
				return 0;
			if ((i != 0 || j != 0) && mid->me[r + i][c + j] <= d)
				return 0;
		}
	}
	return 1;
}

static int __is_plain_rect(MATRIX *mat, int r, int c, int size)
{
	int i, j;
	double d = mat->me[r][c];
	for (i = -size; i <= size; i++) {
		for (j = -size; j <= size; j++) {
			if (matrix_outdoor(mat, r, i, c, j))
				continue;
			if (ABS(mat->me[r + i][c + j] - d) > 1)
				return 0;
		}
	}
	return 1;	
}


// Make sure mat, r, c is valid
int __hessian_valid(MATRIX *mat, int r, int c)
{
	double threshold = 12.1f;
	double dxx, dyy, dxy, tr, det, ratio;

	dxx = mat->me[r][c-1] + mat->me[r][c+1] - 2.0f * mat->me[r][c];
	dyy = mat->me[r-1][c] + mat->me[r+1][c] - 2.0f * mat->me[r][c];
	dxy = mat->me[r-1][c-1] + mat->me[r+1][c+1] -mat->me[r+1][c-1] - mat->me[r-1][c+1];
	tr = dxx + dyy;
	det = dxx * dyy - dxy * dxy;
	if (ABS(det) < MIN_DOUBLE_NUMBER)
		return 0;
	
	ratio = (1.0f * tr * tr)/det;
	
	return (ratio <= threshold) ? 1 : 0;
}

// gradient vector
int __mgrad_vector(MATRIX *mat, RECT *rect, VECTOR *vector)
{
	double dx, dy, rd, a;
	int i, j, k, arcstep;

	vector_clear(vector);
	arcstep = 360/vector->m;
	
	for (i = rect->r; i < rect->r + rect->h; i++) {
		for (j = rect->c; j < rect->c + rect->w; j++) {
			dx = (mat->me[i][j + 1] - mat->me[i][j - 1])/2.0f;
			dy = (mat->me[i + 1][j] - mat->me[i - 1][j])/2.0f;
			math_topolar((int)(1000.0f * dx), (int)(1000.0f * dy), &rd, &a);
			k = math_arcindex(a, arcstep);
			vector->ve[k] += sqrt(dx * dx + dy * dy);
		}	
	}

	return RET_OK;
}


int __sift_orientation(MATRIX *mat, int r, int c)
{
	int i, j, k, bw;
	double dx, dy, d1, d2, rd, a;
	MATRIX *gsk;
	VECTOR *v36;
	double sigma = 1.2f;	// ==> Window size 8x8 !!!

	v36 = vector_create(36); check_vector(v36); 
	gsk = matrix_gskernel(sigma); check_matrix(gsk);
	bw = gsk->m/2;

	for (i = -bw; i <= bw; i++) {
		for (j = -bw; j <= bw; j++) {
			if (matrix_outdoor(mat, r, i, c, j))
				continue;
			
			d1 = matrix_outdoor(mat, r, i, c, j - 1)? mat->me[r + i][c + j] : mat->me[r + i][c + j - 1];
			d2 = matrix_outdoor(mat, r, i, c, j + 1)? mat->me[r + i][c + j] : mat->me[r + i][c + j + 1];
			dx = (d2 - d1)/2.0f;
			d1 = matrix_outdoor(mat, r, i - 1, c, j)? mat->me[r + i][c + j] : mat->me[r + i - 1][c + j];
			d2 = matrix_outdoor(mat, r, i + 1, c, j)? mat->me[r + i][c + j] : mat->me[r + i + 1][c + j];
			dy = (d2 - d1)/2.0f;

			math_topolar((int)(1000.0f * dx), (int)(1000.0f * dy), &rd, &a);
			k = math_arcindex(a, 10);

			v36->ve[k] += sqrt(dx * dx + dy * dy) * (gsk->me[bw + i][bw + j]);
		}	
	}

	vector_smooth(v36);
	k = vector_maxindex(v36) * 10;

	matrix_destroy(gsk);
	vector_destroy(v36);
	
	return k;
}


static int __sift_descriptors(MATRIX *mat, int r, int c, int theta, MATRIX *mat8x8, VECTOR *v128, VECTOR *v8)
{
	int i, j, k;
	RECT rect;

	if (v128->m != 128 && v128->m != 32) {
		syslog_error("SIFT Dimension is not right !!!");
		return RET_ERROR;
	}

	// Sample mat
	matrix_region(mat, r, c, theta, mat8x8);

	if (v128->m == 128) {	// 128 Dimension
		rect.h = rect.w = 2;
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 4; j++) {
				rect.r = 2 * i + 1; rect.c = 2 * j + 1; 	
				__mgrad_vector(mat8x8, &rect, v8);
				for (k = 0; k < (int)v8->m; k++)
					v128->ve[8 * (4 * i + j) + k] = v8->ve[k];
			}
		}
	}
	if (v128->m == 32) {		// 32 Dimensions
		rect.h = rect.w = 4;
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				rect.r = 4 * i + 1; rect.c = 4 * j + 1; 	
				__mgrad_vector(mat8x8, &rect, v8);
				for (k = 0; k < (int)v8->m; k++)
					v128->ve[8 * (2 * i + j) + k] = v8->ve[k];
			}
		}
	}
	vector_normal2(v128);

	return RET_OK;
}

static int __sift_drawpoint(IMAGE *image, MATRIX *featmat)
{
	int i, j, k, t, s, i2, j2;
	
	check_image(image);
	check_matrix(featmat);

	for (k = 0; k < featmat->m; k++) {
		i = (int)featmat->me[k][SIFT_ROW_INDEX];
		j = (int)featmat->me[k][SIFT_COL_INDEX];
		t = (int)featmat->me[k][SIFT_DIR_INDEX];
		s = (int)featmat->me[k][SIFT_SCALE_INDEX];
		
		if (image_outdoor(image, i, -2, j, 0) || image_outdoor(image, i, +2, j, 0))
			continue;
		if (image_outdoor(image, i, 0, j, -2) || image_outdoor(image, i, 0, j, +2))
			continue;
		image_drawline(image, i, j - 2, i, j + 2, 0x00ffff);
		image_drawline(image, i - 2, j, i + 2, j, 0x00ffff);

		i2 = i + 4 * s * sin(t * MATH_PI/180);
		j2 = j + 4 * s * cos(t * MATH_PI/180);
		image_drawline(image, i, j, i2, j2, 0x00ffff);
	}
	return RET_OK;
}

int __oactave_detect(MATRIX *omat, int o, MATRIX *featmat, MATRIX *mat8x8, VECTOR *v128, VECTOR *v8)
{
	double sigma;
	MATRIX *dog[4], *log[4];
	int i, j, k, h, size, no, theta;
	
	check_matrix(omat);
	check_matrix(featmat);

	sigma = __sift_sigma(o, 0);
	for (k = 0; k < SIFT_SCALE_NUMBER; k++) {
		matrix_gauss_filter(omat, sigma);
		log[k] = matrix_copy(omat); check_matrix(log[k]);
		dog[k] = matrix_copy(omat); check_matrix(dog[k]);
		sigma *= SIFT_INIT_K;
		matrix_gauss_filter(dog[k], sigma);
		matrix_foreach(dog[k], i, j)
			dog[k]->me[i][j] -= omat->me[i][j];
	}

	no = 0;
	size = __scale_ratio(o);
	for (k = 1; k <= SIFT_SCALE_NUMBER - 2; k++) {
		// Check dog[k]
		for (i = 1; i < dog[k]->m - 1; i++) {
			for (j = 1; j < dog[k]->n - 1; j++) {
				if (ABS(dog[k]->me[i][j]) <= SIFT_DOG_THRESHOLD)  // DOG_THRESHOLD
					continue;
				
				if (! __local_3dmax(dog[k-1], dog[k], dog[k+1], i, j) && ! __local_3dmin(dog[k-1], dog[k], dog[k+1], i, j))
					continue;
				
				if (! __hessian_valid(dog[k], i, j))
					continue;

				if (__is_plain_rect(log[k - 1], i, j, 4))
					continue;

				// Found a key point
				matrix_setvalue(featmat, no, SIFT_ROW_INDEX, i * size);
				matrix_setvalue(featmat, no, SIFT_COL_INDEX, j * size);

				// Got main orientation
				theta = __sift_orientation(log[k - 1], i, j);
				matrix_setvalue(featmat, no, SIFT_DIR_INDEX, theta);

				// Scale
				matrix_setvalue(featmat, no, SIFT_SCALE_INDEX, size);

				// Got descriptiors
				__sift_descriptors(log[k - 1], i, j, theta, mat8x8, v128, v8);

				for (h = 0; h < v128->m; h++)
					featmat->me[no][h + SIFT_HEAD_SIZE] = v128->ve[h];

				no++;
			}
		}
	}

	// Clear dog matrix
	for (k = 0; k < 4; k++) {
		matrix_destroy(dog[k]);
		matrix_destroy(log[k]);
	}

	return 0;
}


int __matrix_match(MATRIX *f1, MATRIX *f2)
{
	int i, j, k, index = 0;
	double d1, d2, d;
	LINES *lines = line_set();

	lines->count = 0;
	for (i = 0; i < f1->m && lines->count < MAX_OBJECT_NUM; i++) {
		d1 = d2 = 2 * sqrt(SIFT_DEMENSION);	// Max !!!
		for (j = 0; j < f2->m; j++) {
			d = 0;
			for (k = 0; k < SIFT_DEMENSION; k++) {
				d += (f1->me[i][k + SIFT_HEAD_SIZE] - f2->me[j][k + SIFT_HEAD_SIZE]) * (f1->me[i][k + SIFT_HEAD_SIZE] - f2->me[j][k + SIFT_HEAD_SIZE]);
			}
			d = sqrt(d);
			if (d < d1) {
				d2 = d1; d1 = d; index = j;
			}
			else if (d < d2)
				d2 = d;
		}
		
		if (d2 > MIN_DOUBLE_NUMBER && d1/d2 < SIFT_MATCH_THRESHOLD)
			line_put(f1->me[i][SIFT_ROW_INDEX], f1->me[i][SIFT_COL_INDEX], f2->me[index][SIFT_ROW_INDEX], f2->me[index][SIFT_COL_INDEX]); 
	}

	printf("%d key points have been matched.\n", lines->count);
	
	return RET_OK;
}

// NO Lua Interface
MATRIX *sift_detect(IMAGE *img, int debug)
{
	int o, m, n;
	VECTOR *v8, *v128;
	MATRIX *mat, *featmat, *octave, *mat8x8;

	mat = image_getplane(img, 'A'); CHECK_MATRIX(mat);
	mat8x8 = matrix_create(10, 10); CHECK_MATRIX(mat8x8);	// 8x8 with border !!!

	v8 = vector_create(8); CHECK_VECTOR(v8); 
	v128 = vector_create(SIFT_DEMENSION); CHECK_VECTOR(v128); 

	// (r, c, d, s, 128 ...)
	featmat = matrix_create(1, SIFT_HEAD_SIZE + SIFT_DEMENSION); CHECK_MATRIX(featmat); 

	m = mat->m; n = mat->n;
	for (o = 0; o < SIFT_OCTAVE_NUMBER; o++) { 
		octave = matrix_zoom(mat, m, n, 0); CHECK_MATRIX(octave);
		__oactave_detect(octave, o, featmat, mat8x8, v128, v8);
		matrix_destroy(octave);
		m /= 2; n /= 2;
		if (m < 16 || n < 16)
			break;
	}

	matrix_destroy(mat8x8);
	vector_destroy(v128);
	vector_destroy(v8);

	printf("%d key points have been detected.\n", featmat->m);

	if (debug) {
		__sift_drawpoint(img, featmat);
	}

	matrix_destroy(mat);

	return featmat;
}


int sift_match(IMAGE *image1, IMAGE *image2)
{
	int i, j, k, color;
	IMAGE *image;
	MATRIX *f1, *f2;
	LINES *lines = line_set();
	const char *result = "sift_match.jpg";

	check_image(image1);
	check_image(image2);

#if 0	
	f1 = sift_detect(image1, 1);
	f2 = sift_detect(image2, 1);
#else
	f1 = sift_detect(image1, 0);
	f2 = sift_detect(image2, 0);
#endif
	__matrix_match(f1, f2);

	image = image_create(MAX(image1->height, image2->height), image1->width + image2->width);
	check_image(image);

	// paste image 1
	image_foreach(image1, i, j) {
		image->ie[i][j].r = image1->ie[i][j].r;
		image->ie[i][j].g = image1->ie[i][j].g;
		image->ie[i][j].b = image1->ie[i][j].b;
	}

	// paste image 2
	k = image1->width;
	image_foreach(image2, i, j) {
		image->ie[i][j + k].r = image2->ie[i][j].r;
		image->ie[i][j + k].g = image2->ie[i][j].g;
		image->ie[i][j + k].b = image2->ie[i][j].b;
	}

	matrix_destroy(f1);
	matrix_destroy(f2);

	for (k = 0; k < lines->count; k++) {
		if (k % 3 == 0)
			color = 0xff0000;
		else if (k % 3 == 1)
			color = 0x00ff00;
		else
			color = 0x0000ff;
		image_drawline(image, lines->line[k].r1, lines->line[k].c1, lines->line[k].r2, lines->line[k].c2 + image1->width, color);
		printf("Matched: (%d, %d) --> (%d, %d)\n", lines->line[k].r1, lines->line[k].c1, lines->line[k].r2, lines->line[k].c2);
	}
	
	printf("Match result saves to `%s`.\n", result);
	image_save(image, result);
	image_destroy(image);

	return RET_OK;
}
