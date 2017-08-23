/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jun 17 11:18:51 CST 2017
***
************************************************************************************/


#include "seam.h"

#define SEAM_LINE_THRESHOLD 2

// a: -1, b: 0,  c: 1
static int __abc_index(double a, double b, double c)
{
	if (a < b)
		return (a < c)? -1 : 1;
	
	// a >= b
	return (b < c)? 0 : 1;
}

// a: -1, b: 0
static int __ab_index(double a, double b)
{
	return (a < b)? -1 : 0;
}

// b: 0, c: 1
static int __bc_index(double b, double c)
{
	return (b < c)? 0 : 1;
}

// Dynamic programming
int *seam_program(MATRIX *mat, int debug)
{
	int i, j, index, besti, *C;
	double a, b, c, min;
	MATRIX *t, *loc;		// location for best way
	
	CHECK_MATRIX(mat);

	t = matrix_create(mat->m, mat->n); CHECK_MATRIX(t);
	loc = matrix_create(mat->m, mat->n); CHECK_MATRIX(loc);
	C = (int *)calloc((size_t)mat->n, sizeof(int)); // CHECK_POINT(C);
	
	// Step 1. First column
	for (i = 0; i < mat->m; i++)
		t->me[i][0] = mat->me[i][0];
	

	// Step 2. t[][j], j >= 2,  Second and next columns 
	for (j = 1; j < mat->n; j++) {
		// a) top row, i == 0
		i = 0;
		b = t->me[i][j - 1] + mat->me[i][j];
		c = t->me[i + 1][j - 1] + mat->me[i][j];
		index = __bc_index(b, c);
		if (index == 0) { // b solution
			t->me[i][j] = b;
			loc->me[i][j] = i;
		}
		else {	// c solution
			t->me[i][j] = c;
			loc->me[i][j] = i + 1;
		}

		// b) midle rows
		for (i = 1; i < mat->m - 1; i++) {
			a = t->me[i - 1][j - 1] + mat->me[i][j];
			b = t->me[i][j - 1] + mat->me[i][j];
			c = t->me[i + 1][j - 1] + mat->me[i][j];
			index = __abc_index(a, b, c);
			if (index == -1) {	// a
				t->me[i][j] = a;
				loc->me[i][j] = i - 1;
			}
			else if (index == 0) { // b
				t->me[i][j] = b;
				loc->me[i][j] = i;
			}
			else {	// c
				t->me[i][j] = c;
				loc->me[i][j] = i + 1;
			}
		}


		// c) bottom row,  i == mat->m - 1
		i = mat->m - 1;
		a = t->me[i - 1][j - 1] + mat->me[i][j];
		b = t->me[i][j - 1] + mat->me[i][j];
		index = __ab_index(a, b);
		if (index == -1) {
			t->me[i][j] = a;
			loc->me[i][j] = i - 1;
		}
		else {
			t->me[i][j] = b;
			loc->me[i][j] = i;
		}
	}
	

	// Step 3. Check Last row to find best way
	besti = 0;
	min = t->me[0][t->n - 1];
	for (i = 1; i < mat->m; i++) {
		if (min > t->me[i][t->n - 1]) {
			min = t->me[i][t->n - 1];
			besti = i;
		}
	}

	if (debug) {
//		printf("Seam Raw Matrix:\n");
//		matrix_print(mat, "%lf");

//		printf("Time Cost Matrix:\n");
//		matrix_print(t, "%lf");

		printf("Last Best Path: %d, Min Cost Time: %lf\n", besti, min);
	}

	// Set best curve to C
	i = besti;
	C[mat->n - 1] = i;
	for (j = mat->n - 1; j >= 1; j--) {
		i = (int)loc->me[i][j];
		C[j - 1]  = i;
	}

	if (debug) {
		for (j = 0; j < mat->n - 1; j++)
			printf("%d ", C[j]);
		printf("\n");
	}

	matrix_destroy(loc);
	matrix_destroy(t);

	return C;
}


// Seam a, b and return best seam line
// -- make sure rect_a, rect_b size is same
// mode: 0-3
int *seam_bestpath(IMAGE *image_a, RECT *rect_a, IMAGE *image_b, RECT *rect_b, int mode)
{
	MATRIX *mat;						// seam matrix
	IMAGE *mask;
	double d, r2, x2, slope, bias;
	int i, j, a, b, c, *line;		// seam line;

	CHECK_IMAGE(image_a);
	CHECK_IMAGE(image_b);

	mat = matrix_create(rect_a->h, rect_a->w); CHECK_MATRIX(mat);
	mask = image_create(rect_a->h, rect_a->w); CHECK_IMAGE(mask);

	switch(mode) {
	case 0: 	// A, Center = (h, w)
		matrix_foreach(mat,i,j) {
			// r2 = (i - rect_a->h)*(i - rect_a->h) + (j - rect_a->w)*(j - rect_a->w);
			r2 = 1.0f;
			a = (image_a->ie[i + rect_a->r][j + rect_a->c].r - image_b->ie[i + rect_b->r][j + rect_b->c].r);
			b = (image_a->ie[i + rect_a->r][j + rect_a->c].g - image_b->ie[i + rect_b->r][j + rect_b->c].g);
			c = (image_a->ie[i + rect_a->r][j + rect_a->c].b - image_b->ie[i + rect_b->r][j + rect_b->c].b);
			x2 =  a*a + b*b + c*c;
			mat->me[i][j] = r2*x2;
		}
		break;
	case 1: 	// B, Center = (h, 0)
		matrix_foreach(mat,i,j) {
			// r2 = (i - rect_a->h)*(i - rect_a->h) + j*j;
			r2 = 1.0f;
			a = (image_a->ie[i + rect_a->r][j + rect_a->c].r - image_b->ie[i + rect_b->r][j + rect_b->c].r);
			b = (image_a->ie[i + rect_a->r][j + rect_a->c].g - image_b->ie[i + rect_b->r][j + rect_b->c].g);
			c = (image_a->ie[i + rect_a->r][j + rect_a->c].b - image_b->ie[i + rect_b->r][j + rect_b->c].b);
			x2 =  a*a + b*b + c*c;
			mat->me[i][j] = r2*x2;
		}
		break;
	case 2: 	// C, Center = (0, w)
		matrix_foreach(mat,i,j) {
			// r2 = i * i + (j - rect_a->w)*(j - rect_a->w);
			r2 = 1.0f;
			a = (image_a->ie[i + rect_a->r][j + rect_a->c].r - image_b->ie[i + rect_b->r][j + rect_b->c].r);
			b = (image_a->ie[i + rect_a->r][j + rect_a->c].g - image_b->ie[i + rect_b->r][j + rect_b->c].g);
			c = (image_a->ie[i + rect_a->r][j + rect_a->c].b - image_b->ie[i + rect_b->r][j + rect_b->c].b);
			x2 =  a*a + b*b + c*c;
			mat->me[i][j] = r2*x2;
		}
		break;
	case 3: 	// D, Center = (0, 0)
		matrix_foreach(mat,i,j) {
			// r2 = i*i + j*j;
			r2 = 1.0f;
			a = (image_a->ie[i + rect_a->r][j + rect_a->c].r - image_b->ie[i + rect_b->r][j + rect_b->c].r);
			b = (image_a->ie[i + rect_a->r][j + rect_a->c].g - image_b->ie[i + rect_b->r][j + rect_b->c].g);
			c = (image_a->ie[i + rect_a->r][j + rect_a->c].b - image_b->ie[i + rect_b->r][j + rect_b->c].b);
			x2 =  a*a + b*b + c*c;
			mat->me[i][j] = r2*x2;
		}
		break;
	default:
		break;
	}

	line = seam_program(mat, 0);
	if (line == NULL) {
		syslog_error("Find best seam line.");
		matrix_destroy(mat);
		return NULL;
	}

	// Need to line correct ?
	// ... (r1, c1) -- (r2, c2) --> k = (r2 - r1)/(c2-c1), b = r2 - k*c2;
	switch(mode) {
	case 0: 	// A, Center = (h, w)
		b = mat->m - 1 - line[mat->n - 1];
		if (b >= SEAM_LINE_THRESHOLD) {
			a = MAX((mat->n - b), 0); c = line[a];
			slope = mat->m - 1; slope -= c; slope /= b; bias = mat->m - 1 - slope*(mat->n - 1);

			for (j = a; j < mat->n - 1; j++) {
				d = slope*j + bias;
				line[j] = CLAMP((int)d, 0, (mat->m - 1));
			}
		}
		break;
	case 1: 	// B, Center = (h, 0)
		b = mat->m - 1 - line[0];
		if (b >= SEAM_LINE_THRESHOLD) {
			a = MIN((mat->n - 1), b); 	c = line[a];
			slope = c; slope -= (mat->m - 1); slope /= b;
			bias = mat->m - 1;
			for (j = 0; j < a; j++) {
				d = slope*j + bias;
				line[j] = CLAMP((int)d, 0, (mat->m - 1));
			}
		}
		break;
	case 2: 	// C, Center = (0, w)
		b = line[mat->n - 1];
		if (b >= SEAM_LINE_THRESHOLD) {
			a = MAX((mat->n - b), 0); c = line[a];
			slope = -1.0f*c/b; bias = -slope*(mat->n - 1);

			for (j = a; j < mat->n - 1; j++) {
				d = slope*j + bias;
				line[j] = CLAMP((int)d, 0, (mat->m - 1));
			}
		}
		break;
	case 3: 	// D, Center = (0, 0)
		b = line[0];
		if (b >= SEAM_LINE_THRESHOLD) {
			a = MIN((mat->n - 1), b); c = line[a];
			slope = 1.0f*c/b; bias = 0.0f;
			for (j = 0; j < a; j++) {
				d = slope*j + bias;
				line[j] = CLAMP((int)d, 0, (mat->m - 1));
			}
		}
		break;
	default:
		break;
	}

	matrix_destroy(mat);

 	return line;
}


// Seam a, b and save result to mask
// -- make sure rect_a, rect_b size is same
// mode: 0-3
IMAGE *seam_bestmask(IMAGE *image_a, RECT *rect_a, IMAGE *image_b, RECT *rect_b, int mode)
{
	IMAGE *mask;
	int i, j, *line;		// seam line;

	CHECK_IMAGE(image_a);
	CHECK_IMAGE(image_b);

	line = seam_bestpath(image_a, rect_a, image_b, rect_b, mode);
	if (! line)
		return NULL;

	mask = image_create(rect_a->h, rect_a->w); CHECK_IMAGE(mask);
	if (mode == 0 || mode == 1) {
		// Border Up: a, Down: b
		for (j = 0; j < rect_b->w; j++) {
			for (i = 0; i < rect_b->h; i++) {
				if (i <= line[j])
					mask->ie[i][j].r = mask->ie[i][j].g = mask->ie[i][j].b = 255;
				else
					mask->ie[i][j].r = mask->ie[i][j].g = mask->ie[i][j].b = 0;
  			}
		}
	}
	else {
		// Border Up: b, Down: a
		for (j = 0; j < rect_b->w; j++) {
			for (i = 0; i < rect_b->h; i++) {
				if (i >= line[j])
					mask->ie[i][j].r = mask->ie[i][j].g = mask->ie[i][j].b = 255;
				else
					mask->ie[i][j].r = mask->ie[i][j].g = mask->ie[i][j].b = 0;
 			}
		}
	}
	
	free(line);

 	return mask;
}


int seam_test()
{
	int j;
	MATRIX *mat = matrix_create(4, 5);
	check_matrix(mat);

	matrix_pattern(mat, "rand");

	int *C = seam_program(mat, 1);

	if (C) {
		printf("Best Path: ");
		for (j = 0; j < mat->n; j++)
			printf("%d ", C[j]);
		printf("\n");
		free(C);
	}

	matrix_destroy(mat);
	return RET_OK;
}


