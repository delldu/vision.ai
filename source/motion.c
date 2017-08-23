
/************************************************************************************
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include "image.h"

#define MOTION_OBJECT_DELTA 3
#define MOTION_OBJECT_THRESHOLD (512) // 2*16*16

#define OBJECT_DETECTION_LO_THRESHOLD (3.15f/10000.0f) // 4 sigma == 99.9937 --> 63/10000 --> 31.5/10000
//#define OBJECT_DETECTION_LO_THRESHOLD (1.0f/1000.0f) // 4 sigma == 99.9937 --> 63/10000 --> 31.5/10000

#define OBJECT_DETECTION_HI_THRESHOLD 0.10f // 10.0%
#define OBJECT_DETECTION_MIN_POINTS 3

static int object_min_points = OBJECT_DETECTION_MIN_POINTS;

int has_weight(int w)
{
	return (w >= object_min_points)? 1 : 0;
}

int sparse_box(MATRIX *mat, RECT *rect)
{
	return (matrix_weight(mat, rect) < (int)(0.4*rect->h * rect->w))? 1 : 0;
}

int empty_box(MATRIX *mat, RECT *rect)
{
	if (rect->h < 1 || rect->w < 1)
		return 1;
	
	return has_weight(matrix_weight(mat, rect))? 0 : 1;
}

int empty_row(MATRIX *mat, RECT *rect, int row, int *w)
{
	RECT nr;
	nr.r = row; nr.h = 1;
	nr.c = rect->c; nr.w = rect->w;
	*w = matrix_weight(mat, &nr);
	return has_weight(*w)? 0 : 1;
}

int empty_col(MATRIX *mat, RECT *rect, int col, int *w)
{
	RECT nr;
	nr.r = rect->r; nr.h = rect->h;
	nr.c = col; nr.w = 1;
	*w = matrix_weight(mat, &nr);
	return has_weight(*w)? 0 : 1;
}

int empty_top(MATRIX *mat, RECT *rect)
{
	RECT nr;
	nr.r = rect->r; nr.h = 1;
	nr.c = rect->c; nr.w = rect->w;

	return has_weight(matrix_weight(mat, &nr))? 0 : 1;
}

int empty_left(MATRIX *mat, RECT *rect)
{
	RECT nr;
	nr.r = rect->r; nr.h = rect->h;
	nr.c = rect->c; nr.w = 1;
	
	return has_weight(matrix_weight(mat, &nr))? 0 : 1;
}

int empty_bottom(MATRIX *mat, RECT *rect)
{
	RECT nr;
	nr.r = rect->r + rect->h - 1; nr.h = 1;
	nr.c = rect->c; nr.w = rect->w;

	return has_weight(matrix_weight(mat, &nr))? 0 : 1;
}

int empty_right(MATRIX *mat, RECT *rect)
{
	RECT nr;
	nr.r = rect->r; nr.h = rect->h;
	nr.c = rect->c + rect->w - 1; nr.w = 1;
	
	return has_weight(matrix_weight(mat, &nr))? 0 : 1;
}

// Suppose mat is accumulative mat and rect is valid
static int rect_compress(MATRIX *mat, RECT *rect)
{
	// Row compress
	while(rect->h > 0 && empty_top(mat, rect)) {
		rect->r++; rect->h--;
	}

	while (rect->h > 0 && empty_bottom(mat, rect)) {
		rect->h--;
	}
	if (rect->h <= 0)
		return RET_OK;		// DOT CONTINUE !!!

	// Col compress
	while(rect->w > 0 && empty_left(mat, rect)) {
		rect->c++; rect->w--;
	}
	while(rect->w > 0 && empty_right(mat, rect)) {
		rect->w--;
	}

	return RET_OK;
}

// Suppose mat is accumulative mat and rect is valid
static int rect_divide(MATRIX *mat, RECT *rect, RECT *rect1, RECT *rect2)
{
	int i, j, w, wr, wc, br, bc; // weight of row, col, best divide row, col

	*rect1 = *rect;
	*rect2 = *rect;

	wr = 0xffff; br = 0;	
	// Row divide : [r+1, r+2, r+3, .. r+h - 2] ==> h >= 3
	for (i = rect->r + 1; i <= rect->r + rect->h - 2; i++) {
		if (empty_row(mat, rect, i, &w)) {
			rect1->h = i - rect->r;
			rect2->r = i;
			rect2->h = rect->h - rect1->h;
			return 1;		// Divide OK
		}
		// best for force divide
		if (w < wr) {
			wr = w; br = i;
		}
	}

	// Col divide : [j+1, j+2, j+3, .. j+w - 2] ==> w >= 3
	wc = 0xffff; bc = 0;
	for (j = rect->c + 1; j <= rect->c + rect->w - 2; j++) {
		if (empty_col(mat, rect, j, &w)) {
			rect1->w = j - rect->c;
			rect2->c = j;
			rect2->w = rect->w - rect1->w;
			return 1;		// Divide OK
		}
		// best for force divide
		if (w < wc) {
			wc = w; bc = j;
		}
	}

	// NO Possiable divide, rect is last block ?
	if (rect->h >= (int)mat->m/2) {	// force div !
		// rect_put(rect);
		if (wr == 0xffff)
			br = rect->r + rect->h/2;
		rect1->h = br - rect->r;
		rect2->r = br;
		rect2->h = rect->h - rect1->h;
		return 1;
	}

	if (rect->w >= (int)mat->n/2) {	// force div !
		// rect_put(rect);
		if (wc == 0xffff)
			bc = rect->c + rect->w/2;
		rect1->w = bc - rect->c;
		rect2->c = bc;
		rect2->w = rect->w - rect1->w;
		return 1;
	}

	return 0;
}


static int color_delta(RGB c1, RGB c2, int threshold)
{
	int delta = (c1.r - c2.r)*(c1.r - c2.r) + (c1.g - c2.g)*(c1.g - c2.g) + 	(c1.b - c2.b)*(c1.b - c2.b);
	return (delta >= threshold)? 1 : 0;
}

static void object_finding(MATRIX *diffmat, RECT *rect)
{
	RECT r1, r2;

	if (empty_box(diffmat, rect)) 	 // ignore small objects
		return;
 	rect_compress(diffmat, rect);
	if (empty_box(diffmat, rect)) 	 // ignore small objects
		return;
	if (rect_divide(diffmat, rect, &r1, &r2) == 0) {
		if (sparse_box(diffmat, rect))
			rect_put(rect);
 		return;
	}
	object_finding(diffmat, &r1);
	object_finding(diffmat, &r2);
}

// mat -- difference matrix
static int object_search(MATRIX *mat)
{
	double r;
	RECT rect;

	rect.r = rect.c = 0;
	rect.h = mat->m; rect.w = mat->n;
	
	matrix_integrate(mat);
	r = mat->me[mat->m - 1][mat->n - 1] * OBJECT_DETECTION_LO_THRESHOLD;		// 4*sigma
	object_min_points = MAX((int)r, OBJECT_DETECTION_MIN_POINTS);
	// CheckPoint("object_min_points = %d, r = %f", object_min_points, r);

	object_finding(mat, &rect);

	return RET_OK;
}

int object_fast_detect(IMAGE *img)
{
	double r;
	MATRIX *diffmat;
	int i, j, i1, j1, i2, j2, k, threshold = MOTION_OBJECT_THRESHOLD;
	RECTS *mrs = rect_set();

	check_image(img);
	
	diffmat = matrix_create(img->height, img->width); check_matrix(diffmat);
	do {
		// Calculate
		// [0, 0], [0, 2*delta]
		// [2*delta, 0], [2*delta, 2*delta]
		// Align Left, Top
		matrix_clear(diffmat);
		for (i = 0; i < img->height - 2 * MOTION_OBJECT_DELTA; i++) {
			i1 = i + 0;
			i2 = i + MOTION_OBJECT_DELTA;
			for (j = 0; j < img->width - 2*MOTION_OBJECT_DELTA; j++) {
				j1 = j + 0;
				j2 = j + MOTION_OBJECT_DELTA;

				k = color_delta(img->ie[i1][j1], img->ie[i2][j2], threshold);
				diffmat->me[i + MOTION_OBJECT_DELTA][j + MOTION_OBJECT_DELTA] += k;
			}

		}
		// Align Right, Top
		for (i = 0; i < img->height - 2 * MOTION_OBJECT_DELTA; i++) {
			i1 = i + 0;
			i2 = i + MOTION_OBJECT_DELTA;
			for (j = 0; j < img->width - 2*MOTION_OBJECT_DELTA; j++) {
				j1 = j + 2*MOTION_OBJECT_DELTA;
				j2 = j + MOTION_OBJECT_DELTA;

				k = color_delta(img->ie[i1][j1], img->ie[i2][j2], threshold);
				diffmat->me[i + MOTION_OBJECT_DELTA][j + MOTION_OBJECT_DELTA] += k;
			}
		}
		// Align Left, Bottom
		for (i = 0; i < img->height - 2 * MOTION_OBJECT_DELTA; i++) {
			i1 = i + 2*MOTION_OBJECT_DELTA;
			i2 = i + MOTION_OBJECT_DELTA;
			for (j = 0; j < img->width - 2*MOTION_OBJECT_DELTA; j++) {
				j1 = 0;
				j2 = j + MOTION_OBJECT_DELTA;

				k = color_delta(img->ie[i1][j1], img->ie[i2][j2], threshold);
				diffmat->me[i + MOTION_OBJECT_DELTA][j + MOTION_OBJECT_DELTA] += k;
			}
		}
		// Align Right, Bottom
		for (i = 0; i < img->height - 2 * MOTION_OBJECT_DELTA; i++) {
			i1 = i + 2*MOTION_OBJECT_DELTA;
			i2 = i + MOTION_OBJECT_DELTA;
			for (j = 0; j < img->width - 2*MOTION_OBJECT_DELTA; j++) {
				j1 = j + 2*MOTION_OBJECT_DELTA;
				j2 = j + MOTION_OBJECT_DELTA;

				k = color_delta(img->ie[i1][j1], img->ie[i2][j2], threshold);
				diffmat->me[i + MOTION_OBJECT_DELTA][j + MOTION_OBJECT_DELTA] += k;
			}
		}
		r = 0.0f;
		matrix_foreach(diffmat,i,j) {
			if (diffmat->me[i][j] >= 2.0f) {
				diffmat->me[i][j] = 1.0f;
				r += 1.0;
			}
			else
				diffmat->me[i][j] = 0.0f;
		}
		r /= diffmat->m; r /= diffmat->n;

		// printf("r = %.6f\n", r);
		// for next time
		threshold += MOTION_OBJECT_THRESHOLD;
	} while (r > OBJECT_DETECTION_HI_THRESHOLD);	// > 10%

	mrs->count = 0;
	object_search(diffmat);

	matrix_destroy(diffmat);

	return RET_OK;
}

int object_detect(IMAGE *img, int levs)
{
	int i, j, k, n;
	int rows, cols, bh, bw;
	MATRIX *diffmat;
	VECTOR *voice; 
	RECTS *mrs = rect_set();

	rows = img->height/3;
	cols = img->width/3;
	voice = image_voice(img, rows, cols, levs, 1);

	bh = (img->height + rows - 1)/rows;
	bw = (img->width + cols - 1)/cols;

	mrs->count = 0;
	diffmat = matrix_create(rows, cols); check_matrix(diffmat);
	for (k = 0; k < levs; k++) {
		matrix_foreach(diffmat,i,j) {
			n = VOICE_CELL_OFFSET(i, j, k);
			diffmat->me[i][j] = voice->ve[n];
		}
		matrix_clean(diffmat);
		object_search(diffmat);
	}
	matrix_destroy(diffmat);

	vector_destroy(voice);

	for (i = 0; i < mrs->count; i++) {
		mrs->rect[i].r *= bh;
		mrs->rect[i].c *= bw;
		mrs->rect[i].h *= bh;
		mrs->rect[i].w *= bw;
	}

	return RET_OK;
}

int motion_updatebg(IMAGE *A, IMAGE *B, IMAGE *C, IMAGE *bg)
{
	int i, j, k, a, b, threshold = MOTION_OBJECT_THRESHOLD;
	image_foreach(bg, i, j) {
		k = (A->ie[i][j].r - B->ie[i][j].r) * (A->ie[i][j].r - B->ie[i][j].r) +
			(A->ie[i][j].g - B->ie[i][j].g) * (A->ie[i][j].g - B->ie[i][j].g) +
			(A->ie[i][j].b - B->ie[i][j].b) * (A->ie[i][j].b - B->ie[i][j].b);
		a = (k >= threshold)? 1 : 0;
		k = (C->ie[i][j].r - B->ie[i][j].r) * (C->ie[i][j].r - B->ie[i][j].r) +
			(C->ie[i][j].g - B->ie[i][j].g) * (C->ie[i][j].g - B->ie[i][j].g) +
			(C->ie[i][j].b - B->ie[i][j].b) * (C->ie[i][j].b - B->ie[i][j].b);
			b = (k >= threshold)? 1 : 0;
		if (a == 0 || b == 0) {         // B == A || B == C ==> B is bg
			bg->ie[i][j].r = B->ie[i][j].r;
			bg->ie[i][j].g = B->ie[i][j].g;
			bg->ie[i][j].b = B->ie[i][j].b;
		}
	}
	return RET_OK;
}

int motion_detect(IMAGE *fg, IMAGE *bg, int debug)
{
	MATRIX *diffmat;
	int i, j, k, threshold = MOTION_OBJECT_THRESHOLD;
	RECTS *mrs = rect_set();

	check_image(fg);
	diffmat = matrix_create(fg->height, fg->width); check_matrix(diffmat);

	// Calculate
	image_foreach(fg, i, j) {
		k = (fg->ie[i][j].r - bg->ie[i][j].r) * (fg->ie[i][j].r - bg->ie[i][j].r) +
			(fg->ie[i][j].g - bg->ie[i][j].g) * (fg->ie[i][j].g - bg->ie[i][j].g) +
			(fg->ie[i][j].b - bg->ie[i][j].b) * (fg->ie[i][j].b - bg->ie[i][j].b);
		if (k >= threshold)
			diffmat->me[i][j] = 1;
	}
	mrs->count = 0;
	object_search(diffmat);

	if (debug)     // Draw rects ?
		image_drawrects(fg);

	matrix_destroy(diffmat);

	return RET_OK;
}

double motion_ratio(IMAGE *img)
{
	int i;
	double sum;
	
	RECTS *mrs = rect_set();
	sum = 0.0f;
	for (i = 0; i < mrs->count; i++) {
		sum += 1.0f*mrs->rect[i].h * mrs->rect[i].w;
	}
	sum /= (img->height * img->width);

	return sum;
}

