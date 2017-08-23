/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Thu Jun 22 20:29:46 PDT 2017
***
************************************************************************************/
// sview -- SURROUND-VIEW


#include "sview.h"
#include "blend.h"
#include "seam.h"
#include "filter.h"
#include "color.h"

#define FRONT_CAMERA 0
#define LEFT_CAMERA 1
#define RIGHT_CAMERA 2
#define REAR_CAMERA 3
#define CAMERA_NUMBERS 4

#define BLEND_LAYERS 5

#define FRONT_A_RECT(rect) do { 	(rect)->r = front_height() - blend_height(); (rect)->c = 0; (rect)->h = blend_height(); (rect)->w = left_width(); } while(0)
#define FRONT_B_RECT(rect) do { 	(rect)->r = front_height() - blend_height(); (rect)->c = left_width() + car_width(); (rect)->h = blend_height(); (rect)->w = right_width(); } while(0)
#define FRONT_M_RECT(rect) do { 	(rect)->r = front_height() - blend_height(); (rect)->c = left_width(); (rect)->h = blend_height(); (rect)->w = car_width(); } while(0)

#define LEFT_A_RECT(rect) do { 	(rect)->r = 0; (rect)->c = 0; (rect)->h = blend_height(); (rect)->w = left_width(); } while(0)
#define LEFT_B_RECT(rect) do { 	(rect)->r = left_height() + blend_height(); (rect)->c = 0; (rect)->h = blend_height(); (rect)->w = left_width(); } while(0)
// Left middle
#define LEFT_M_RECT(rect) do { 	(rect)->r = blend_height(); (rect)->c = 0; (rect)->h = left_height(); (rect)->w = left_width(); } while(0)


#define RIGHT_A_RECT(rect) do { 	(rect)->r = 0; (rect)->c = 0; (rect)->h = blend_height(); (rect)->w = right_width(); } while(0)
#define RIGHT_B_RECT(rect) do { 	(rect)->r = right_height() + blend_height(); (rect)->c = 0; (rect)->h = blend_height(); (rect)->w = right_width(); } while(0)
// Right middle
#define RIGHT_M_RECT(rect) do { 	(rect)->r = blend_height(); (rect)->c = 0; (rect)->h = right_height(); (rect)->w = right_width(); } while(0)

#define REAR_A_RECT(rect) do { 	(rect)->r = 0; (rect)->c = 0; (rect)->h = blend_height(); (rect)->w = left_width(); } while(0)
#define REAR_B_RECT(rect) do { 	(rect)->r = 0; (rect)->c = left_width() + car_width(); (rect)->h = blend_height(); (rect)->w = right_width(); } while(0)
#define REAR_M_RECT(rect) do { 	(rect)->r = 0; (rect)->c = left_width(); (rect)->h = blend_height(); (rect)->w = car_width(); } while(0)

//
#define WHOLE_FRONT_RECT(rect) do { 	(rect)->r = 0; (rect)->c = 0; (rect)->h = front_height(); (rect)->w = front_width(); } while(0)
#define WHOLE_LEFT_RECT(rect) do { 	(rect)->r = front_height(); (rect)->c = 0; (rect)->h = left_height(); (rect)->w = left_width(); } while(0)
#define WHOLE_RIGHT_RECT(rect) do { 	(rect)->r = front_height(); (rect)->c = left_width() + car_width(); (rect)->h = right_height(); (rect)->w = right_width(); } while(0)
#define WHOLE_REAR_RECT(rect) do { 	(rect)->r = front_height() + left_height(); (rect)->c = 0; (rect)->h = rear_height(); (rect)->w = rear_width(); } while(0)

#define FINAL_FILTER_BAND_WIDTH 3	// >= 2

typedef struct {
	int front_height;

	int left_width;
	
	int car_width;
	int car_height;

	int rear_height;

	int blend_height;	// default = 250 ?

 	CAMERA camera[CAMERA_NUMBERS];
	IMAGE *image;

	MATRIX *mask_a, *mask_b, *mask_c, *mask_d;
} SVIEW;

// Global varaibles
static SVIEW __global_view;

// A/B
static int __create_maska()
{
	int i, j;
	MATRIX *mask;

	mask = matrix_create(front_height(), left_width());
	check_matrix(mask);

	matrix_foreach(mask, i, j) {
		// j = w/h*(h - i);	
		if (mask->m * j < mask->n * (mask->m - i)) {
				if (i == 0 && j == 0)
				mask->me[i][j] = 1.0f;
				else
				mask->me[i][j] = 1.0f*mask->n*j/(mask->n*j + mask->m*i);
			}
			else {	// j >= w/h*(h - i);
			if (i == mask->m && j == mask->n)
				mask->me[i][j] = 1.0f;
				else
				mask->me[i][j] = 1.0f*mask->n*(mask->m - i)/(mask->m*(mask->n -j) + mask->n*(mask->m - i));
		}
	}
	__global_view.mask_a = mask;
	return RET_OK;
}

static int __create_maskb()
{
	int i, j;
	MATRIX *mask;

	mask = matrix_create(front_height(), right_width());
	check_matrix(mask);

	matrix_foreach(mask, i, j) {
			// j = w/h*i;	Line
		if (mask->m * j >= mask->n * i) { // j > w/h*i
			if (i == 0 && j == mask->n)
				mask->me[i][j] = 1.0f;
				else
				mask->me[i][j] = 1.0f*mask->m*(mask->n -j)/(mask->n*i + mask->m*(mask->n -j));
			}
			else {	// j < w/h*i;
			if (j == 0 && i == mask->m)
				mask->me[i][j] = 1.0f;
				else
				mask->me[i][j] = 1.0f*mask->n*(mask->m - i)/(mask->m*j + mask->n*(mask->m - i));
		}
	}
	__global_view.mask_b = mask;
	return RET_OK;
}

static int __create_maskc()
{
	int i, j;
	MATRIX *mask;

	mask = matrix_create(rear_height(), left_width());
	check_matrix(mask);

	matrix_foreach(mask, i, j) {
			// j = w/h*i;	Line
		if (mask->m * j > mask->n * i) {	// j > w/h*i
			if (i == 0 && j == mask->n)
				mask->me[i][j] = 1.0f;
				else
				mask->me[i][j] = 1.0f*mask->n*i/(mask->n*i + mask->m*(mask->n -j));
			}
			else {	// j <= w/h*i;
			if (j == 0 && i == mask->m)
				mask->me[i][j] = 0.0f;
				else
				mask->me[i][j] = 1.0f*mask->m*j/(mask->m*j + mask->n*(mask->m - i));
		}
	}
	__global_view.mask_c = mask;
	return RET_OK;
}

static int __create_maskd()
{
	int i, j;
	MATRIX *mask;

	mask = matrix_create(rear_height(), right_width());
	check_matrix(mask);

	matrix_foreach(mask, i, j) {
			// j = w/h*(h - i);	Line
		if (mask->m * j <= mask->n * (mask->m - i)) {	// Line above
				if (i == 0 && j == 0)
				mask->me[i][j] = 1.0f;
				else
				mask->me[i][j] = 1.0f*mask->n*i/(mask->n*i + mask->m*j);
			}
			else {	// j < w/h*i;
			if (j == mask->n && i == mask->m)
				mask->me[i][j] = 0.0f;
				else
				mask->me[i][j] = 1.0f*mask->m*(mask->n - j)/(mask->m*(mask->n - j) + mask->n*(mask->m - i));
		}			
	}
	__global_view.mask_d = mask;
	return RET_OK;
}

static void __create_masks()
{
	__create_maska();
	__create_maskb();
	__create_maskc();
	__create_maskd();
}


static void __update_block(IMAGE *image_a, RECT *rect_a, IMAGE *image_b, RECT *rect_b, MATRIX *mask)
{
	int i, j;
	double alpha, d;

	matrix_foreach(mask,i,j) {
		alpha = mask->me[i][j];
		
		d = alpha*image_a->ie[rect_a->r + i][rect_a->c + j].r + (1.0f - alpha)*image_b->ie[rect_b->r + i][rect_b->c + j].r;
		image_a->ie[rect_a->r + i][rect_a->c + j].r = CLAMP((int)d, 0, 255);
		d = alpha*image_a->ie[rect_a->r + i][rect_a->c + j].g + (1.0f - alpha)*image_b->ie[rect_b->r + i][rect_b->c + j].g;
		image_a->ie[rect_a->r + i][rect_a->c + j].g = CLAMP((int)d, 0, 255);
		d = alpha*image_a->ie[rect_a->r + i][rect_a->c + j].b + (1.0f - alpha)*image_b->ie[rect_b->r + i][rect_b->c + j].b;
		image_a->ie[rect_a->r + i][rect_a->c + j].b = CLAMP((int)d, 0, 255);
			}			
}



// A/B
static void __update_blocka()
{
	RECT rect_a, rect_b;
	IMAGE *image_a, *image_b;

	// Top-left -- A
	image_a = camera_image(sview_camera(FRONT_CAMERA));
	image_b = camera_image(sview_camera(LEFT_CAMERA));
	FRONT_A_RECT(&rect_a);
	LEFT_A_RECT(&rect_b);

	__update_block(image_a, &rect_a, image_b, &rect_b, __global_view.mask_a);
}

static void __update_blockb()
{
	RECT rect_a, rect_b;
	IMAGE *image_a, *image_b;

	// Top-Right  -- B
	image_a = camera_image(sview_camera(FRONT_CAMERA));
	image_b = camera_image(sview_camera(RIGHT_CAMERA));
	FRONT_B_RECT(&rect_a);
	RIGHT_A_RECT(&rect_b);
		
	__update_block(image_a, &rect_a, image_b, &rect_b, __global_view.mask_b);
		}

static void __update_blockc()
{
	RECT rect_a, rect_b;
	IMAGE *image_a, *image_b;

	// Bottom-left  -- C
	image_a = camera_image(sview_camera(REAR_CAMERA));
	image_b = camera_image(sview_camera(LEFT_CAMERA));
	REAR_A_RECT(&rect_a);
	LEFT_B_RECT(&rect_b);

	__update_block(image_a, &rect_a, image_b, &rect_b, __global_view.mask_c);
	}

static void __update_blockd()
{
	RECT rect_a, rect_b;
	IMAGE *image_a, *image_b;

	// Bottom-right -- D
	image_a = camera_image(sview_camera(REAR_CAMERA));
	image_b = camera_image(sview_camera(RIGHT_CAMERA));
	REAR_B_RECT(&rect_a);
	RIGHT_B_RECT(&rect_b);

	__update_block(image_a, &rect_a, image_b, &rect_b, __global_view.mask_d);
}

static void __update_blocks()
{
	__update_blocka();
	__update_blockb();
	__update_blockc();
	__update_blockd();
}

static int __save_config()
{
	FILE *fp;
	char *newconfig = "camera.newmodel";

	fp = fopen(newconfig, "w");

	if (fp == NULL) {
		syslog_error("Create config %s.", newconfig);
		return RET_ERROR;
	}

	camera_saveconf(sview_camera(FRONT_CAMERA), fp);
	camera_saveconf(sview_camera(LEFT_CAMERA), fp);
	camera_saveconf(sview_camera(RIGHT_CAMERA), fp);
	camera_saveconf(sview_camera(REAR_CAMERA), fp);

	fclose(fp);
	
	return RET_OK;
}


static int __rect_filter(IMAGE *image, RECT *rect)
{
	int ret;
	int k3x3[3] = {1, 2, 1};
	int k5x5[5] = {1, 4, 6, 4, 1};
	double avg, stdv;

	ret = image_rect_statistics(image, rect, 'A', &avg, &stdv);
	avg = stdv/(ABS(avg) + MIN_DOUBLE_NUMBER);
	if (avg >= 0.1f)
	image_rect_filter(image, rect, ARRAY_SIZE(k3x3), k3x3, 16);

	// Coefficient of Variation
	ret = image_rect_statistics(image, rect, 'A', &avg, &stdv);
	avg = stdv/(ABS(avg) + MIN_DOUBLE_NUMBER);
	if (avg >= 0.1f) {
		ret = image_rect_filter(image, rect, ARRAY_SIZE(k5x5), k5x5, 256);
	}
	return ret;
}

static void __final_tunning()
{
	RECT rect;
	
	// A1
	rect.r = front_height() - FINAL_FILTER_BAND_WIDTH;
	rect.c = 0;
	rect.h = 2*FINAL_FILTER_BAND_WIDTH;
	rect.w = left_width();
	__rect_filter(__global_view.image, &rect);

	// A2
	rect.r = 0;
	rect.c = left_width() - FINAL_FILTER_BAND_WIDTH;
	rect.h = front_height();
	rect.w = 2*FINAL_FILTER_BAND_WIDTH;
	__rect_filter(__global_view.image, &rect);

	// A3
	rect.r = 0;
	rect.c = left_width() + car_width() - FINAL_FILTER_BAND_WIDTH;
	rect.h = front_height();
	rect.w = 2*FINAL_FILTER_BAND_WIDTH;
	__rect_filter(__global_view.image, &rect);

	// A4
	rect.r = front_height() - FINAL_FILTER_BAND_WIDTH;
	rect.c = left_width() + car_width();
	rect.h = 2*FINAL_FILTER_BAND_WIDTH;
	rect.w = right_width();
	__rect_filter(__global_view.image, &rect);

	// B1
	rect.r = front_height() + left_height() - FINAL_FILTER_BAND_WIDTH;
	rect.c = 0;
	rect.h = 2*FINAL_FILTER_BAND_WIDTH;
	rect.w = left_width();
	__rect_filter(__global_view.image, &rect);

	// B2
	rect.r = front_height() + left_height();
	rect.c = left_width() - FINAL_FILTER_BAND_WIDTH;
	rect.h = rear_height();
	rect.w = 2*FINAL_FILTER_BAND_WIDTH;
	__rect_filter(__global_view.image, &rect);
		
	// B3
	rect.r = front_height() + right_height();
	rect.c = left_width() + car_width() - FINAL_FILTER_BAND_WIDTH;
	rect.h = rear_height();
	rect.w = 2*FINAL_FILTER_BAND_WIDTH;
	__rect_filter(__global_view.image, &rect);

	// B4
	rect.r = front_height() + right_height() - FINAL_FILTER_BAND_WIDTH;
	rect.c = left_width() + car_width();
	rect.h = 2*FINAL_FILTER_BAND_WIDTH;
	rect.w = right_width();
	__rect_filter(__global_view.image, &rect);
	// End filter
}

static void __laplace_quantize(MATRIX *mat)
{
	int i, j, q;

	matrix_foreach(mat,i,j) {
		q = mat->me[i][j]/2.0f;
		mat->me[i][j] = 2.0f*q;
	}
}

int blend_height()
{
	return __global_view.blend_height;
}

int car_width()
{
	return __global_view.car_width;
}

int car_height()
{
	return __global_view.car_height;
}

int front_height()
{
	return __global_view.front_height;
}

int front_width()
{
	return __global_view.left_width + __global_view.car_width + __global_view.left_width;
}

int left_height()
{
	return __global_view.car_height;
}

int left_width()
{
	return __global_view.left_width;
}

int right_height()
{
	return __global_view.car_height;
}

int right_width()
{
	return __global_view.left_width;
}

int rear_height()
{
	return __global_view.rear_height;
}

int rear_width()
{
	return __global_view.left_width + __global_view.car_width + __global_view.left_width;
}

// Global multi-band blend
int sview_global_blend(int layers, int debug)
{
	int i, j, k, hh, hw, i1, j1, i2;
	IMAGE *a, *b, *c, *d;
	MATRIX *g_a[3][BLEND_LAYERS];
	MATRIX *g_b[3][BLEND_LAYERS];
	MATRIX *g_c[3][BLEND_LAYERS];
	MATRIX *g_d[3][BLEND_LAYERS];
	MATRIX *g_all[3][BLEND_LAYERS];
	MATRIX *e;

	// Check blend layers
	if (layers >= BLEND_LAYERS) {
		syslog_error("Too many layers for multi-band blending.");
		return RET_ERROR;
	}

	if (debug) {
		time_reset();
	}

	__update_blocks();
	
	// Create all Gauss Prymids, 0ms
	hh = front_height() + left_height() + rear_height();
	hw = left_width() + car_width() + right_width();
	g_all[R_CHANNEL][0] = matrix_create(hh, hw); check_matrix(g_all[R_CHANNEL][0]);
	g_all[G_CHANNEL][0] = matrix_create(hh, hw); check_matrix(g_all[G_CHANNEL][0]);
	g_all[B_CHANNEL][0] = matrix_create(hh, hw); check_matrix(g_all[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		hh = (hh + 1)/2; hw = (hw + 1)/2;
		g_all[R_CHANNEL][k] = matrix_create(hh, hw); check_matrix(g_all[R_CHANNEL][k]);
		g_all[G_CHANNEL][k] = matrix_create(hh, hw); check_matrix(g_all[G_CHANNEL][k]);
		g_all[B_CHANNEL][k] = matrix_create(hh, hw); check_matrix(g_all[B_CHANNEL][k]);
	}
	if (debug) {
		time_spend("Create all Guass Prymids");
	}

	// Create a-Gauss Prymids, G[1], G[2], ... 40ms
	a = camera_image(sview_camera(FRONT_CAMERA));
	g_a[R_CHANNEL][0] = image_getplane(a, 'R'); check_matrix(g_a[R_CHANNEL][0]);
	g_a[G_CHANNEL][0] = image_getplane(a, 'G'); check_matrix(g_a[G_CHANNEL][0]);
	g_a[B_CHANNEL][0] = image_getplane(a, 'B'); check_matrix(g_a[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		g_a[R_CHANNEL][k] = matrix_pyrdown(g_a[R_CHANNEL][k - 1]); check_matrix(g_a[R_CHANNEL][k]);
		g_a[G_CHANNEL][k] = matrix_pyrdown(g_a[G_CHANNEL][k - 1]); check_matrix(g_a[G_CHANNEL][k]);
		g_a[B_CHANNEL][k] = matrix_pyrdown(g_a[B_CHANNEL][k - 1]); check_matrix(g_a[B_CHANNEL][k]);
	}

	if (debug) {
		time_spend("Create A Guass Prymids");
	}

	// Calclaute a-Laplace prymids, 40ms
	// L[k] = G[k] = G[k] - pryUp(G[k+1])	, k = 0 .. layers - 1
	for (k = 0; k < layers; k++) {
		// R
		e = matrix_pyrup(g_a[R_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_a[R_CHANNEL][k], i, j)
			g_a[R_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_a[R_CHANNEL][k]);
		
		// G
		e = matrix_pyrup(g_a[G_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_a[G_CHANNEL][k], i, j)
			g_a[G_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_a[G_CHANNEL][k]);

		// B
		e = matrix_pyrup(g_a[B_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_a[B_CHANNEL][k], i, j)
			g_a[B_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_a[B_CHANNEL][k]);
	}

	if (debug) {
		time_spend("Create A Laplace Prymids");
	}

	// Create b Gauss Prymids, 75 ms
	b = camera_image(sview_camera(LEFT_CAMERA));
	g_b[R_CHANNEL][0] = image_getplane(b, 'R'); check_matrix(g_b[R_CHANNEL][0]);
	g_b[G_CHANNEL][0] = image_getplane(b, 'G'); check_matrix(g_b[G_CHANNEL][0]);
	g_b[B_CHANNEL][0] = image_getplane(b, 'B'); check_matrix(g_b[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		g_b[R_CHANNEL][k] = matrix_pyrdown(g_b[R_CHANNEL][k - 1]); check_matrix(g_b[R_CHANNEL][k]);
		g_b[G_CHANNEL][k] = matrix_pyrdown(g_b[G_CHANNEL][k - 1]); check_matrix(g_b[G_CHANNEL][k]);
		g_b[B_CHANNEL][k] = matrix_pyrdown(g_b[B_CHANNEL][k - 1]); check_matrix(g_b[B_CHANNEL][k]);
	}

	if (debug) {
		time_spend("Create B Guass Prymids");
	}

	// Calclaute b-Laplace prymids, 119ms
	for (k = 0; k < layers; k++) {
		// R
		e = matrix_pyrup(g_b[R_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_b[R_CHANNEL][k], i, j)
			g_b[R_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_b[R_CHANNEL][k]);

		// G
		e = matrix_pyrup(g_b[G_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_b[G_CHANNEL][k], i, j)
			g_b[G_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_b[G_CHANNEL][k]);

		// B
		e = matrix_pyrup(g_b[B_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_b[B_CHANNEL][k], i, j)
			g_b[B_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_b[B_CHANNEL][k]);
	}

	if (debug) {
		time_spend("Create B Laplace Prymids");
	}
	
	// Create c Gauss Prymids, 62ms
	c = camera_image(sview_camera(RIGHT_CAMERA));
	g_c[R_CHANNEL][0] = image_getplane(c, 'R'); check_matrix(g_c[R_CHANNEL][0]);
	g_c[G_CHANNEL][0] = image_getplane(c, 'G'); check_matrix(g_c[G_CHANNEL][0]);
	g_c[B_CHANNEL][0] = image_getplane(c, 'B'); check_matrix(g_c[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		g_c[R_CHANNEL][k] = matrix_pyrdown(g_c[R_CHANNEL][k - 1]); check_matrix(g_c[R_CHANNEL][k]);
		g_c[G_CHANNEL][k] = matrix_pyrdown(g_c[G_CHANNEL][k - 1]); check_matrix(g_c[G_CHANNEL][k]);
		g_c[B_CHANNEL][k] = matrix_pyrdown(g_c[B_CHANNEL][k - 1]); check_matrix(g_c[B_CHANNEL][k]);
	}

	if (debug) {
		time_spend("Create C Gauss Prymids");
	}

	// Calclaute c-Laplace prymids, 64ms
	for (k = 0; k < layers; k++) {
		// R
		e = matrix_pyrup(g_c[R_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_c[R_CHANNEL][k], i, j)
			g_c[R_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_c[R_CHANNEL][k]);

		// G
		e = matrix_pyrup(g_c[G_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_c[G_CHANNEL][k], i, j)
			g_c[G_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_c[G_CHANNEL][k]);

		// B
		e = matrix_pyrup(g_c[B_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_c[B_CHANNEL][k], i, j)
			g_c[B_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_c[B_CHANNEL][k]);
	}
	if (debug) {
		time_spend("Create C Laplace Pruymids");
	}

	// Create d Gauss Prymids, 40ms
	d = camera_image(sview_camera(REAR_CAMERA));
	g_d[R_CHANNEL][0] = image_getplane(d, 'R'); check_matrix(g_d[R_CHANNEL][0]);
	g_d[G_CHANNEL][0] = image_getplane(d, 'G'); check_matrix(g_d[G_CHANNEL][0]);
	g_d[B_CHANNEL][0] = image_getplane(d, 'B'); check_matrix(g_d[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		g_d[R_CHANNEL][k] = matrix_pyrdown(g_d[R_CHANNEL][k - 1]); check_matrix(g_d[R_CHANNEL][k]);
		g_d[G_CHANNEL][k] = matrix_pyrdown(g_d[G_CHANNEL][k - 1]); check_matrix(g_d[G_CHANNEL][k]);
		g_d[B_CHANNEL][k] = matrix_pyrdown(g_d[B_CHANNEL][k - 1]); check_matrix(g_d[B_CHANNEL][k]);
	}
	if (debug) {
		time_spend("Create D Gauss Prymids");
	}

	// Calclaute d-Laplace prymids, 45ms
	for (k = 0; k < layers; k++) {
		// R
		e = matrix_pyrup(g_d[R_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_d[R_CHANNEL][k], i, j)
			g_d[R_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_d[R_CHANNEL][k]);

		// G
		e = matrix_pyrup(g_d[G_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_d[G_CHANNEL][k], i, j)
			g_d[G_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_d[G_CHANNEL][k]);

		// B
		e = matrix_pyrup(g_d[B_CHANNEL][k + 1]); check_matrix(e);
		matrix_foreach(g_d[B_CHANNEL][k], i, j)
			g_d[B_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
		__laplace_quantize(g_d[B_CHANNEL][k]);
	}
	if (debug) {
		time_spend("Create D Laplace Prymids");
	}

	// Create Mixed Laplacian Prymid
	for (k = 0; k <= layers; k++) {
		// Paste a, 5ms
		matrix_foreach(g_a[R_CHANNEL][k], i, j) {
			g_all[R_CHANNEL][k]->me[i][j] = g_a[R_CHANNEL][k]->me[i][j];
			g_all[G_CHANNEL][k]->me[i][j] = g_a[G_CHANNEL][k]->me[i][j];
			g_all[B_CHANNEL][k]->me[i][j] = g_a[B_CHANNEL][k]->me[i][j];
		}

		// Paste b, 5ms
		i1 = (front_height() >> k);
		i2 = (blend_height() >> k);
		for (i = 0; i < (left_height() >> k); i++) {
			for (j = 0; j < g_b[R_CHANNEL][k]->n; j++) {
				g_all[R_CHANNEL][k]->me[i + i1][j] = g_b[R_CHANNEL][k]->me[i + i2][j];
				g_all[G_CHANNEL][k]->me[i + i1][j] = g_b[G_CHANNEL][k]->me[i + i2][j];
				g_all[B_CHANNEL][k]->me[i + i1][j] = g_b[B_CHANNEL][k]->me[i + i2][j];
			}
		}

		// Paste c, 2ms
		i1 = (front_height() >> k);
		i2 = (blend_height() >> k);
		for (i = 0; i < (right_height() >> k); i++) {
			j1 = ((left_width() + car_width()) >> k);
			
			for (j = 0; j < g_c[R_CHANNEL][k]->n; j++) {
				g_all[R_CHANNEL][k]->me[i + i1][j + j1] = g_c[R_CHANNEL][k]->me[i + i2][j];
				g_all[G_CHANNEL][k]->me[i + i1][j + j1] = g_c[G_CHANNEL][k]->me[i + i2][j];
				g_all[B_CHANNEL][k]->me[i + i1][j + j1] = g_c[B_CHANNEL][k]->me[i + i2][j];
			}
		}
		
		// Paste d, 3ms
		i1 = (front_height() + left_height()) >> k;
		i2 = (left_height() + blend_height()) >> k;
		matrix_foreach(g_d[R_CHANNEL][k], i, j) {
			g_all[R_CHANNEL][k]->me[i + i1][j] = g_d[R_CHANNEL][k]->me[i][j];
			g_all[G_CHANNEL][k]->me[i + i1][j] = g_d[G_CHANNEL][k]->me[i][j];
			g_all[B_CHANNEL][k]->me[i + i1][j] = g_d[B_CHANNEL][k]->me[i][j];
		}
	}
	if (debug) {
		time_spend("Create Mixed Laplace Prymids");
	}

	// Re-constuct, 199 - 300 ms
	for (k = layers; k >= 1; k--) {
		// R, 46ms + 12ms
		e = matrix_pyrup(g_all[R_CHANNEL][k]); check_matrix(e);
		matrix_foreach(g_all[R_CHANNEL][k - 1], i, j)
			g_all[R_CHANNEL][k - 1]->me[i][j] += e->me[i][j];
		matrix_destroy(e);

		// G, 47ms + 12ms
		e = matrix_pyrup(g_all[G_CHANNEL][k]); check_matrix(e);
		matrix_foreach(g_all[G_CHANNEL][k - 1], i, j)
			g_all[G_CHANNEL][k - 1]->me[i][j] += e->me[i][j];
		matrix_destroy(e);
		
		// B, 46ms + 12ms
		e = matrix_pyrup(g_all[B_CHANNEL][k]); check_matrix(e);
		matrix_foreach(g_all[B_CHANNEL][k - 1], i, j)
			g_all[B_CHANNEL][k - 1]->me[i][j] += e->me[i][j];
		matrix_destroy(e);
	}
	if (debug) {
		time_spend("Restruct Laplace");
	}

	// Copy Result, 7ms
	image_setplane(__global_view.image, 'R', g_all[R_CHANNEL][0]);
	image_setplane(__global_view.image, 'G', g_all[G_CHANNEL][0]);
	image_setplane(__global_view.image, 'B', g_all[B_CHANNEL][0]);
	// image_gauss_filter(__global_view.image, 2);
	if (debug) {
		time_spend("Copy Result");
	}

	// Delete g_all, g_d - g_a, 2ms
	for (k = 0; k <= layers; k++) {
		matrix_destroy(g_all[R_CHANNEL][k]);
		matrix_destroy(g_all[G_CHANNEL][k]);
		matrix_destroy(g_all[B_CHANNEL][k]);

		matrix_destroy(g_d[R_CHANNEL][k]);
		matrix_destroy(g_d[G_CHANNEL][k]);
		matrix_destroy(g_d[B_CHANNEL][k]);

		matrix_destroy(g_c[R_CHANNEL][k]);
		matrix_destroy(g_c[G_CHANNEL][k]);
		matrix_destroy(g_c[B_CHANNEL][k]);

		matrix_destroy(g_b[R_CHANNEL][k]);
		matrix_destroy(g_b[G_CHANNEL][k]);
		matrix_destroy(g_b[B_CHANNEL][k]);

		matrix_destroy(g_a[R_CHANNEL][k]);
		matrix_destroy(g_a[G_CHANNEL][k]);
		matrix_destroy(g_a[B_CHANNEL][k]);
	}
	if (debug) {
		time_spend("Destroy All Matrixs");
	}

	return RET_OK;
}

void sview_alpha_blend(int debug)
{
	if (debug) {
		time_reset();
	}

	__update_blocks();

	sview_micro_blend(0);

	if (debug) {
		time_spend("Alpha blending");
	}
}

CAMERA *sview_camera(int no)
{
	return &__global_view.camera[no];
}

IMAGE *sview_image()
{
	RECT rect;

	rect.r = front_height();
	rect.c = left_width();
	rect.h = car_height() -1;
	rect.w = car_width() - 1;
	image_drawrect(__global_view.image, &rect, 0xffffff, 0);

	return __global_view.image;
}

// sview_start("camera.model")
int sview_start(char *config)
{
	int scale;
	RECT world;
	CAMERA *camera;
	
	FILE *fp = fopen(config, "r");
	if (fp == NULL) {
		syslog_error("Open camera config %s.", config);
		return RET_ERROR;
	}

	// Setup sview layout parameters
	scale = 2;		

	__global_view.car_width = 200/scale;
	__global_view.car_height = 520/scale;
	__global_view.front_height = 250/scale;
	__global_view.left_width = 200/scale;
	__global_view.rear_height = 250/scale;

	__global_view.blend_height = 250/scale;

	// Front
	world.r = car_height()/2;
	world.c = -(left_width() + car_width()/2);
	world.h = front_height();
	world.w = front_width();
	camera = sview_camera(FRONT_CAMERA);
	camera_create(camera, front_height(), front_width());
 	camera_loadconf(fp, camera);
	camera_mapworld(camera, &world, scale);
	// camera_saveconf(camera, stdout);

	// Left
	world.r = -car_height()/2 - blend_height();
	world.c = -(left_width() + car_width()/2);
	world.h = car_height() + 2*blend_height();
	world.w = left_width();
	camera = sview_camera(LEFT_CAMERA);
	camera_create(camera, left_height() + 2*blend_height(), left_width());
 	camera_loadconf(fp, camera);
	camera_mapworld(camera, &world, scale);
	// camera_saveconf(camera, stdout);

	// Right
	world.r = -car_height()/2 - blend_height();
	world.c = car_width()/2;
	world.h = car_height() + 2*blend_height();
	world.w = right_width();
	camera = sview_camera(RIGHT_CAMERA);
	camera_create(camera, right_height() + 2*blend_height(), right_width());
  	camera_loadconf(fp, camera);
	camera_mapworld(camera, &world, scale);
	// camera_saveconf(camera, stdout);

	// Back
	world.r = -(car_height()/2 + rear_height());
	world.c = -(left_width() + car_width()/2);
	world.h = rear_height();
	world.w = rear_width();
	camera = sview_camera(REAR_CAMERA);
	camera_create(camera, rear_height(), rear_width());
 	camera_loadconf(fp, camera);
	camera_mapworld(camera, &world, scale);
	// camera_saveconf(camera, stdout);

	fclose(fp);

	__global_view.image = image_create(front_height() + car_height() + rear_height(), front_width()); check_image(__global_view.image);

	__create_masks();

	return RET_OK;
}

void sview_frame_capture(IMAGE *src, int debug)
{
	RECT rect;
	CAMERA *cam;
	
	/*	Source image layout
		Front	Rear
		Left	Right
	*/

	if (debug) {
		time_reset();
	}

	rect.h = src->height/2;
	rect.w = src->width/2;
	cam = sview_camera(FRONT_CAMERA);
	rect.r = rect.c = 0;	
	camera_defisheye(cam, src, &rect);		// 3 ms
//	camera_rect_gain(cam, src, &rect);

	cam = sview_camera(LEFT_CAMERA);
	rect.r = rect.h; rect.c = 0;	
	camera_defisheye(cam, src, &rect);	// 8 ms
//	camera_rect_gain(cam, src, &rect);

	cam = sview_camera(RIGHT_CAMERA);
	rect.r = rect.h; rect.c = rect.w;	
	camera_defisheye(cam, src, &rect);	// 4ms
//	camera_rect_gain(cam, src, &rect);

	cam = sview_camera(REAR_CAMERA);
	rect.r = 0;
	rect.c = rect.w;
	camera_defisheye(cam, src, &rect);	// 3ms
//	camera_rect_gain(cam, src, &rect);

	if (debug) {
		time_spend("Frame Capture");
	}
}

void sview_color_correct(int debug)
{
	int i;
	RECT rect;
	CAMERA *cam;
	double avg[CAMERA_NUMBERS], davg, dstdv, gain;

	if (debug)
		time_reset();

	for (i = 0; i < CAMERA_NUMBERS; i++) {
		cam = sview_camera(i);
		camera_learning(cam);
		camera_awb(cam);	// 5 ms
	}

#if 0	
	for (i = 0; i < CAMERA_NUMBERS; i++) {
		cam = sview_camera(i);
		image_statistics(camera_image(cam), 'A', &avg[i], &dstdv);
	}
#else
	FRONT_M_RECT(&rect);
	image_rect_statistics(camera_image(sview_camera(FRONT_CAMERA)), &rect, 'A', &avg[FRONT_CAMERA], &dstdv);
	LEFT_M_RECT(&rect);
	image_rect_statistics(camera_image(sview_camera(LEFT_CAMERA)), &rect, 'A', &avg[LEFT_CAMERA], &dstdv);
	RIGHT_M_RECT(&rect);
	image_rect_statistics(camera_image(sview_camera(RIGHT_CAMERA)), &rect, 'A', &avg[RIGHT_CAMERA], &dstdv);
	REAR_M_RECT(&rect);
	image_rect_statistics(camera_image(sview_camera(REAR_CAMERA)), &rect, 'A', &avg[REAR_CAMERA], &dstdv);
#endif

	// Fine tunning color of front and rear
	davg = (avg[LEFT_CAMERA] + avg[RIGHT_CAMERA])/2.0f;
	gain = davg/(avg[FRONT_CAMERA] + MIN_DOUBLE_NUMBER);
	if (gain >= 0.5f && gain <= 2.0f)
		color_correct(camera_image(sview_camera(FRONT_CAMERA)), gain, gain, gain);

	davg = (avg[LEFT_CAMERA] + avg[RIGHT_CAMERA])/2.0f;
	gain = davg/(avg[REAR_CAMERA] + MIN_DOUBLE_NUMBER);
	if (gain >= 0.5f && gain <= 2.0f)
		color_correct(camera_image(sview_camera(REAR_CAMERA)), gain, gain, gain);

	for (i = 0; i < CAMERA_NUMBERS; i++) {
		cam = sview_camera(i);
		camera_filter(cam);
	}
	
	if (debug) {
		time_spend("Camera Color Balance");
	}
}

void sview_space_align(int debug)
{
	RECT rect;
	int crow, ccol;

	if (debug == 0)
		return;

	FRONT_A_RECT(&rect);
	image_rect_mcenter(camera_image(sview_camera(FRONT_CAMERA)), &rect, 'A', &crow, &ccol);
	crow -= rect.r + rect.h/2;
	ccol -= rect.c + rect.w/2;
	CheckPoint("Front A master center offset: row = %d, col = %d", crow, ccol);

	LEFT_A_RECT(&rect);
	image_rect_mcenter(camera_image(sview_camera(LEFT_CAMERA)), &rect, 'A', &crow, &ccol);
	crow -= rect.r + rect.h/2;
	ccol -= rect.c + rect.w/2;
	CheckPoint("Left A master center offset: row = %d, col = %d", crow, ccol);


	FRONT_B_RECT(&rect);
	image_rect_mcenter(camera_image(sview_camera(FRONT_CAMERA)), &rect, 'A', &crow, &ccol);
	crow -= rect.r + rect.h/2;
	ccol -= rect.c + rect.w/2;
	CheckPoint("Front B master center offset: row = %d, col = %d", crow, ccol);

	RIGHT_A_RECT(&rect);
	image_rect_mcenter(camera_image(sview_camera(RIGHT_CAMERA)), &rect, 'A', &crow, &ccol);
	crow -= rect.r + rect.h/2;
	ccol -= rect.c + rect.w/2;
	CheckPoint("Right A master center offset: row = %d, col = %d", crow, ccol);

	if (debug) {
		syslog_debug("Space Alignment.");
	}
}

// Simple Paste, Microsoft blend -- micro_blend !
void sview_micro_blend(int debug)
{
	CAMERA *cam;
	RECT bigrect, smallrect;
	
	if (debug) {
		time_reset();
	}

	// Front image
	cam = sview_camera(FRONT_CAMERA);
	WHOLE_FRONT_RECT(&bigrect);
	image_rect(&smallrect, camera_image(cam));
	image_rect_paste(__global_view.image, &bigrect, camera_image(cam), &smallrect);
	
	// Left image
	cam = sview_camera(LEFT_CAMERA);
	WHOLE_LEFT_RECT(&bigrect);
	LEFT_M_RECT(&smallrect);
	image_rect_paste(__global_view.image, &bigrect, camera_image(cam), &smallrect);

	// Right image
	cam = sview_camera(RIGHT_CAMERA);
	WHOLE_RIGHT_RECT(&bigrect);
	RIGHT_M_RECT(&smallrect);
	image_rect_paste(__global_view.image, &bigrect, camera_image(cam), &smallrect);

	// Back image
	cam = sview_camera(REAR_CAMERA);
	WHOLE_REAR_RECT(&bigrect);
	image_rect(&smallrect, camera_image(cam));
	image_rect_paste(__global_view.image, &bigrect, camera_image(cam), &smallrect);

	if (debug) {
		time_spend("Micro blending");
	}
}

// Method:
// 0 -- none, simple blend
// 1 -- Alpha blend
// 2 -- Local multiband blend
// 3 -- Global multiband blend
int sview_blend(IMAGE *src, int blend_method, int debug)
{
	//color_torgb565(src);

	sview_frame_capture(src, debug);
	sview_color_correct(debug);
 	sview_space_align(0);

	switch(blend_method) {
	case 0:
		sview_micro_blend(debug);
		__final_tunning();
		break;
	case 1:
		sview_alpha_blend(debug);
		__final_tunning();
		break;
	case 2:
		sview_global_blend(2, debug);
		__final_tunning();
		break;
	default:
		break;
	}

	return RET_OK;
}

void sview_stop()
{
	int k;

	__save_config();
	
	for (k = 0; k <  CAMERA_NUMBERS; k++)
		camera_destroy(sview_camera(k));

	image_destroy(__global_view.image);

	matrix_destroy(__global_view.mask_d);
	matrix_destroy(__global_view.mask_c);
	matrix_destroy(__global_view.mask_b);
	matrix_destroy(__global_view.mask_a);
}

