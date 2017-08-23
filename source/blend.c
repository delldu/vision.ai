/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Wed Jun 14 18:05:59 PDT 2017
***
************************************************************************************/


#include "blend.h"
#include "filter.h"
#include "color.h"

#define MASK_THRESHOLD 128
#define COLOR_CLUSTERS 32
#define BOARDER_THRESHOLD 5
#define MASK_ALPHA 0.0f

#define MAX_MULTI_BAND_LAYERS 16


#define mask_foreach(img, i, j) \
	for (i = 1; i < img->height - 1; i++) \
		for (j = 1; j < img->width - 1; j++)


static void __mask_binary(IMAGE *mask, int debug)
{
	int i, j;

	image_foreach(mask, i, j) {
		// set 0 to box border
		if (i == 0 || j == 0 || i == mask->height - 1 || j == mask->width - 1)
			mask->ie[i][j].g = 0;
		else	
			mask->ie[i][j].g = (mask->ie[i][j].g < MASK_THRESHOLD)? 0 : 255;
	}

	if (debug) {
		image_foreach(mask, i, j) {
			mask->ie[i][j].r = mask->ie[i][j].g;
			mask->ie[i][j].b = mask->ie[i][j].g;
		}
	}
}

static inline int __mask_border(IMAGE *mask, int i, int j)
{
	int k;
	static int nb[4][2] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0}};

	if (mask->ie[i][j].g != 255)
		return 0;

	// current is 1
	for (k = 0; k < 4; k++) {
		if (mask->ie[i + nb[k][0]][j + nb[k][1]].g == 0)
			return 1;
	}

	return 0;
}

// neighbours
static inline int __mask_4conn(IMAGE *mask, int i, int j)
{
	int k, sum;
	static int nb[4][2] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0}};

	sum = 0;
	for (k = 0; k < 4; k++) {
		if (mask->ie[i + nb[k][0]][j + nb[k][1]].g != 0)
			sum++;
	}

	return sum;
}


static inline int __mask_cloner(IMAGE *mask, int i, int j)
{
  	return (mask->ie[i][j].g == 255);
}


static inline void __mask_update(IMAGE *mask, int i, int j)
{
	mask->ie[i][j].g = 255;

	mask->ie[i][j].r = 255;
	mask->ie[i][j].b = 255;
}

static void __mask_remove(IMAGE *mask, int i, int j)
{
	mask->ie[i][j].g = 0;

	mask->ie[i][j].r = 0;
	mask->ie[i][j].b = 0;
}

static void __mask_finetune(IMAGE *mask, IMAGE *src, int debug)
{
	int i, j, k, count[COLOR_CLUSTERS];

	__mask_binary(mask, debug);

	color_cluster(src, COLOR_CLUSTERS, 0);	// not update

	// Calculate border colors
	memset(count, 0, COLOR_CLUSTERS * sizeof(int));
	mask_foreach(mask, i, j) {
		if (! __mask_border(mask, i, j))
			continue;
		count[src->ie[i][j].d]++;
	}

	// Fast delete border's blocks
	for (k = 0; k < COLOR_CLUSTERS; k++) {
		if (count[k] < BOARDER_THRESHOLD)
			continue;
		mask_foreach(src, i, j) {
			if (src->ie[i][j].d == k) {
				__mask_remove(mask, i, j);
			}
		}
	}

	// Delete isolate points
	mask_foreach(mask,i,j) {
		if (! __mask_cloner(mask, i, j))
			continue;

		// current == 255
		k = __mask_4conn(mask, i, j);
		if (k <= 1)
			__mask_remove(mask, i, j);
	}

	// Fill isolate holes
	mask_foreach(mask,i,j) {
		if (__mask_cloner(mask, i, j))
			continue;

		// ==> current == 0
		k = __mask_4conn(mask, i, j);
		if (k >= 3)
			__mask_update(mask, i, j);
	}
}


int blend_cluster(IMAGE *src, IMAGE *mask, IMAGE *dst, int top, int left, int debug)
{
	int i, j;
	double d;

	check_image(src);
	check_image(dst);

	if (debug) {
		time_reset();
	}

	color_cluster(src, COLOR_CLUSTERS, 1);
	if (mask == NULL) {
		image_foreach(mask,i,j) {
			if (i == 0 || j == 0 || i == mask->height - 1 || j == mask->width - 1)
				mask->ie[i][j].r = mask->ie[i][j].g = mask->ie[i][j].b = 0;
			else
				mask->ie[i][j].r = mask->ie[i][j].g = mask->ie[i][j].b = 255;
		}
	}

	__mask_finetune(mask, src, debug);

	if (debug) {
		image_foreach(mask, i, j) {
			if (__mask_cloner(mask, i, j))
				continue;
			src->ie[i][j].r = 0;
			src->ie[i][j].g = 0;
			src->ie[i][j].b = 0;
		}
	}

	image_foreach(mask, i, j) {
		if (! __mask_cloner(mask, i, j))
			continue;

		if (image_outdoor(dst, i, top, j, left)) {
			// syslog_error("Out of target image");
			continue;
		}

		if (__mask_border(mask, i, j)) {
			d = MASK_ALPHA * src->ie[i][j].r + (1 - MASK_ALPHA)*dst->ie[i + top][j + left].r;
 			dst->ie[i + top][j + left].r = (BYTE)d;

			d = MASK_ALPHA * src->ie[i][j].g + (1 - MASK_ALPHA)*dst->ie[i + top][j + left].g;
 			dst->ie[i + top][j + left].g = (BYTE)d;

			d = MASK_ALPHA * src->ie[i][j].b + (1 - MASK_ALPHA)*dst->ie[i + top][j + left].b;
 			dst->ie[i + top][j + left].b = (BYTE)d;
		}
		else {
			dst->ie[i + top][j + left].r = src->ie[i][j].r;
			dst->ie[i + top][j + left].g = src->ie[i][j].g;
			dst->ie[i + top][j + left].b = src->ie[i][j].b;
		}
	}

	if (debug) {
		time_spend("Color cluster blending");
	}

	return RET_OK;
}


#define MAX3X3_SIZE 1280 // 4Kx2K  picture: 3840x2160

void __encode3x3(int a, int b, int c, int *ra, int *rb, int *rc)
{
    *ra = (2*a - b - c)/6 + 126;  // ==> (2 * a - b - c)/12 + 127;
    *rb = (a + b + c)/3;
    *rc = (2*c - b - a)/6 + 126; // ==> (2 * c - a - b)/12 + 127;
}

void __decode3x3(int a, int b, int c, int *ra, int *rb, int *rc)
{
    *ra = b + (a - 126) * 2;
    *rb = b - (a + c - 252) * 2;
    *rc = b + (c - 126) * 2;
}

int __3x3_encode(char orgb, IMAGE *img, RECT *rect)
{
	int a, b, c;
	int as[MAX3X3_SIZE], bs[MAX3X3_SIZE], cs[MAX3X3_SIZE];
	int i, j, m, n;
	RECT oldrect;

	check_rgb(orgb);
	check_image(img);
	if (! rect) {
		return RET_ERROR;
	}

	oldrect = *rect;
	image_rectclamp(img, rect);
	
	if (rect->h > 3 * MAX3X3_SIZE || rect->w > 3 * MAX3X3_SIZE) {
		*rect = oldrect;
		return RET_ERROR;
	}


	m = (rect->h + 2)/3; n = (rect->w + 2)/3;

	// Transform by rows
	for (i = rect->r; i < rect->r + rect->h; i++) {
		// Transform color Plane
		for (j = 0; j < n; j++) {
			a = image_getvalue(img, orgb,  i, rect->c + 3*j);
			b = (rect->c + 3*j + 1 < img->width)? \
				image_getvalue(img, orgb,  i, rect->c + 3*j + 1) : a;
			c = (rect->c + 3*j + 2 < img->width)? \
				image_getvalue(img, orgb,  i, rect->c + 3*j + 2) : b;
			__encode3x3(a, b, c, &as[j], &bs[j], &cs[j]);
		}
		
		// Copy data to Plane
		for (j = 0; j <  n; j++) {
			image_setvalue(img, orgb, i, rect->c + j, (BYTE)as[j]);
			if (rect->c + n + j < img->width)
				image_setvalue(img, orgb, i, rect->c + n + j, (BYTE)bs[j]);
			if (rect->c + 2*n + j < img->width)
				image_setvalue(img, orgb, i, rect->c + 2*n + j, (BYTE)cs[j]);
		}
	}
	
	// Transform by columns
	for (j = rect->c; j < rect->c + rect->w; j++) {
		// Transform color Plane
		for (i = 0; i < m; i++) {
			a = image_getvalue(img, orgb,  rect->r + 3 * i, j);
			b = (rect->r + 3 * i + 1 < img->height) ? \
				image_getvalue(img, orgb,  rect->r + 3 * i + 1, j) : a;
			c = (rect->r + 3 * i + 2 < img->height) ? \
				image_getvalue(img, orgb,  rect->r + 3 * i + 2, j) : b;
			__encode3x3(a, b, c, &as[i], &bs[i], &cs[i]);
		}
		// Copy data to Plane
		for (i = 0; i < m; i++) {
			image_setvalue(img, orgb, rect->r + i, j, (BYTE)as[i]);
			if (rect->r + m + i < img->height)
				image_setvalue(img, orgb, rect->r + m + i, j, (BYTE)bs[i]);
			if (rect->r + 2*m + i < img->height)
				image_setvalue(img, orgb, rect->r + 2*m + i, j, (BYTE)cs[i]);
		}
	}

	*rect = oldrect;

	return RET_OK;
}

int __3x3_decode(char orgb, IMAGE *img, RECT *rect)
{
	int a, b, c;
	int as[MAX3X3_SIZE], bs[MAX3X3_SIZE], cs[MAX3X3_SIZE];
	int i, j, m, n;
	RECT oldrect;

	check_rgb(orgb);
	check_image(img);
	if (! rect) {
		return RET_ERROR;
	}

	oldrect = *rect;

	image_rectclamp(img, rect);
	
	if (rect->h > 3 * MAX3X3_SIZE || rect->w > 3 * MAX3X3_SIZE) {
		*rect = oldrect;
		return RET_ERROR;
	}

	m = (rect->h + 2)/3; n = (rect->w + 2)/3;
	/*
		a = a' + b'
		b = b' - a' - c'
		c = b' + c'
	*/

	// Transform by columns
	for (j = rect->c; j < rect->c + rect->w; j++) {
		// Pick up color Plane 
		for (i = 0; i < m; i++) {
			as[i] = image_getvalue(img, orgb,  rect->r + i, j);
			bs[i] = (rect->r + m + i < img->height)? \
				image_getvalue(img, orgb,  rect->r + m + i, j) : as[i];
			cs[i] = (rect->r + 2*m + i < img->height)? \
				image_getvalue(img, orgb,  rect->r + 2*m + i, j) : bs[i]; 
		}
		// Copy data to R Plane
		for (i = 0; i < m; i++) {
 			__decode3x3(as[i], bs[i], cs[i], &a, &b, &c);

			image_setvalue(img, orgb, rect->r + 3*i, j, a);
			if (rect->r + 3*i + 1 < img->height)
				image_setvalue(img, orgb, rect->r + 3*i + 1, j, b);
			if (rect->r + 3*i + 2 < img->height)
				image_setvalue(img, orgb, rect->r + 3*i + 2, j, c);
		}
	}
	
	// Transform by rows
	for (i = rect->r; i < rect->r + rect->h; i++) {
		// Pick up color Plane
		for (j = 0; j < n; j++) {
			as[j] = image_getvalue(img, orgb,  i, rect->c + j);
 			bs[j] = (rect->c + n + j < img->width)? \
				image_getvalue(img, orgb,  i, rect->c + n + j) : as[j];
 			cs[j] = (rect->c + 2*n + j < img->width)? \
				image_getvalue(img, orgb,  i, rect->c + 2*n + j) : bs[j];
 		}
		// Copy data to R Plane
		for (j = 0; j <  n; j++) {
 			__decode3x3(as[j], bs[j], cs[j], &a, &b, &c);

			image_setvalue(img, orgb, i, rect->c + 3*j, a);
			if (rect->c + 3*j + 1 < img->width)
				image_setvalue(img, orgb, i, rect->c + 3*j + 1, b);
			if (rect->c + 3*j + 2 < img->width)
				image_setvalue(img, orgb, i, rect->c + 3*j + 2, c);
		}
	}

	*rect = oldrect;
	
	return RET_OK;
}

int image_3x3enc(IMAGE *img)
{
	RECT rect;

	rect.r = 0;
	rect.c = 0;
	rect.h = img->height;
	rect.w = img->width;
	
	if (__3x3_encode('R', img, &rect) != RET_OK) {
		CheckPoint("Error");
		return RET_ERROR;
	}

	if (__3x3_encode('G', img, &rect) != RET_OK) {
		CheckPoint("Error");
		return RET_ERROR;
	}

	return __3x3_encode('B', img, &rect);
}

int image_3x3dec(IMAGE *img)
{
	RECT rect;

	rect.r = 0;
	rect.c = 0;
	rect.h = img->height;
	rect.w = img->width;

	if (__3x3_decode('R', img, &rect) != RET_OK)
		return RET_ERROR;

	if (__3x3_decode('G', img, &rect) != RET_OK)
		return RET_ERROR;
	
	return __3x3_decode('B', img, &rect);
}

IMAGE *blend_3x3codec(IMAGE *a, IMAGE *b, IMAGE *mask, int debug)
{
	int i, j;
	RECT rect;
	IMAGE *c;


	if (debug) {
		time_reset();
	}

	// make sure size of a, b, mask is same.
	if (a->height != b->height || a->height != mask->height) {
		syslog_error("Height of A, B, Mask is not same.");
		return NULL;
	}
	if (a->width != b->width || a->width != mask->width) {
		syslog_error("Width of A, B, Mask is not same.");
		return NULL;
	}

	c = image_create(mask->height, mask->width); CHECK_IMAGE(c);

	rect.r = 0;
	rect.c = 0;
	rect.h = mask->height;
	rect.w = mask->width;

	__3x3_encode('R', a, &rect);
	__3x3_encode('G', a, &rect);
	__3x3_encode('B', a, &rect);
	image_gauss_filter(c, 1.0f);

	__3x3_encode('R', b, &rect);
	__3x3_encode('G', b, &rect);
	__3x3_encode('B', b, &rect);
	image_gauss_filter(c, 1.0f);

	image_foreach(c, i, j) {
		if (mask->ie[i][j].g > 0) {
			c->ie[i][j].r = a->ie[i][j].r;
			c->ie[i][j].g = a->ie[i][j].g;
			c->ie[i][j].b = a->ie[i][j].b;
		}
		else {
			c->ie[i][j].r = b->ie[i][j].r;
			c->ie[i][j].g = b->ie[i][j].g;
			c->ie[i][j].b = b->ie[i][j].b;
		}
	}
	image_gauss_filter(c, 1.0f);

	__3x3_decode('R', c, &rect);
	__3x3_decode('G', c, &rect);
	__3x3_decode('B', c, &rect);
	image_gauss_filter(c, 1.0f);

	if (debug) {
		time_spend("3x3 Codec blending");
	}
	
	return c;
}

IMAGE *blend_mutiband(IMAGE *a, IMAGE *b, IMAGE *mask, int layers, int debug)
{
	int i, j, k, hh, hw;
	IMAGE *g_mask[MAX_MULTI_BAND_LAYERS];
	IMAGE *c = NULL;

	MATRIX *g_a[3][MAX_MULTI_BAND_LAYERS];
	MATRIX *g_b[3][MAX_MULTI_BAND_LAYERS];
	MATRIX *e;

	CHECK_IMAGE(a);
	CHECK_IMAGE(b);
	CHECK_IMAGE(mask);

	// Check blend layers
	if (layers >= MAX_MULTI_BAND_LAYERS) {
		syslog_error("Too many layer for multi-band blending.");
		return NULL;
	}

	// make sure size of a, b, mask is same.
	if (a->height != b->height || a->height != mask->height) {
		syslog_error("Height of A, B, Mask is not same.");
		return NULL;
	}
	if (a->width != b->width || a->width != mask->width) {
		syslog_error("Width of A, B, Mask is not same.");
		return NULL;
	}

	if (debug) {
		time_reset();
	}

	// Create result image
	c = image_create(a->height, a->width); CHECK_IMAGE(c);

	// Create mask Gauss Prymids
	g_mask[0] = mask;
	hh = mask->height; hw = mask->width;
	for (k = 1; k <= layers; k++) {
		hh = (hh + 1)/2; hw = (hw + 1)/2;
		g_mask[k] = image_zoom(g_mask[k - 1], hh, hw, ZOOM_METHOD_COPY);
		CHECK_IMAGE(g_mask[k]);
	}

	// Create a-Gauss Prymids, G[1], G[2], ...
	g_a[R_CHANNEL][0] = image_getplane(a, 'R'); CHECK_MATRIX(g_a[R_CHANNEL][0]);
	g_a[G_CHANNEL][0] = image_getplane(a, 'G'); CHECK_MATRIX(g_a[G_CHANNEL][0]);
	g_a[B_CHANNEL][0] = image_getplane(a, 'B'); CHECK_MATRIX(g_a[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		g_a[R_CHANNEL][k] = matrix_pyrdown(g_a[R_CHANNEL][k - 1]); CHECK_MATRIX(g_a[R_CHANNEL][k]);
		g_a[G_CHANNEL][k] = matrix_pyrdown(g_a[G_CHANNEL][k - 1]); CHECK_MATRIX(g_a[G_CHANNEL][k]);
		g_a[B_CHANNEL][k] = matrix_pyrdown(g_a[B_CHANNEL][k - 1]); CHECK_MATRIX(g_a[B_CHANNEL][k]);
	}

	// Calclaute a-Laplace prymids
	// L[k] = G[k] = G[k] - pryUp(G[k+1])	, k = 0 .. layers - 1
	for (k = 0; k < layers; k++) {
		// R
		e = matrix_pyrup(g_a[R_CHANNEL][k + 1]); CHECK_MATRIX(e);
		matrix_foreach(g_a[R_CHANNEL][k], i, j)
			g_a[R_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);

		// G
		e = matrix_pyrup(g_a[G_CHANNEL][k + 1]); CHECK_MATRIX(e);
		matrix_foreach(g_a[G_CHANNEL][k], i, j)
			g_a[G_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);

		// B
		e = matrix_pyrup(g_a[B_CHANNEL][k + 1]); CHECK_MATRIX(e);
		matrix_foreach(g_a[B_CHANNEL][k], i, j)
			g_a[B_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
	}
	
	// Create b Gauss Prymids
	g_b[R_CHANNEL][0] = image_getplane(b, 'R'); CHECK_MATRIX(g_b[R_CHANNEL][0]);
	g_b[G_CHANNEL][0] = image_getplane(b, 'G'); CHECK_MATRIX(g_b[G_CHANNEL][0]);
	g_b[B_CHANNEL][0] = image_getplane(b, 'B'); CHECK_MATRIX(g_b[B_CHANNEL][0]);
	for (k = 1; k <= layers; k++) {
		g_b[R_CHANNEL][k] = matrix_pyrdown(g_b[R_CHANNEL][k - 1]); CHECK_MATRIX(g_b[R_CHANNEL][k]);
		g_b[G_CHANNEL][k] = matrix_pyrdown(g_b[G_CHANNEL][k - 1]); CHECK_MATRIX(g_b[G_CHANNEL][k]);
		g_b[B_CHANNEL][k] = matrix_pyrdown(g_b[B_CHANNEL][k - 1]); CHECK_MATRIX(g_b[B_CHANNEL][k]);
	}

	// Calclaute b-Laplace prymids
	for (k = 0; k < layers; k++) {
		// R
		e = matrix_pyrup(g_b[R_CHANNEL][k + 1]); CHECK_MATRIX(e);
		matrix_foreach(g_b[R_CHANNEL][k], i, j)
			g_b[R_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);

		// G
		e = matrix_pyrup(g_b[G_CHANNEL][k + 1]); CHECK_MATRIX(e);
		matrix_foreach(g_b[G_CHANNEL][k], i, j)
			g_b[G_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);

		// B
		e = matrix_pyrup(g_b[B_CHANNEL][k + 1]); CHECK_MATRIX(e);
		matrix_foreach(g_b[B_CHANNEL][k], i, j)
			g_b[B_CHANNEL][k]->me[i][j] -= e->me[i][j];
		matrix_destroy(e);
	}
	
	// Create Laplacian Prymids (a - b)
	// Laplace A = mask * A + (1 - mask) * B
	// Include two parts: 
	//		1) L[1] ... L[layers - 1] -- High frequency
	// 		2) Guass Filter(a) + Gauss Filter(b) -- Low frequency
	for (k = 0; k <= layers; k++) {
		matrix_foreach(g_a[R_CHANNEL][k], i, j) {
			if (g_mask[k]->ie[i][j].g == 0) {
				g_a[R_CHANNEL][k]->me[i][j] = g_b[R_CHANNEL][k]->me[i][j];
				g_a[G_CHANNEL][k]->me[i][j] = g_b[G_CHANNEL][k]->me[i][j];
				g_a[B_CHANNEL][k]->me[i][j] = g_b[B_CHANNEL][k]->me[i][j];
			}
		}
	}

	// Re-constuct
	for (k = layers; k >= 1; k--) {
		// R
		e = matrix_pyrup(g_a[R_CHANNEL][k]); CHECK_MATRIX(e);
		matrix_foreach(g_a[R_CHANNEL][k - 1], i, j)
			g_a[R_CHANNEL][k - 1]->me[i][j] += e->me[i][j];
		matrix_destroy(e);

		// G
		e = matrix_pyrup(g_a[G_CHANNEL][k]); CHECK_MATRIX(e);
		matrix_foreach(g_a[G_CHANNEL][k - 1], i, j)
			g_a[G_CHANNEL][k - 1]->me[i][j] += e->me[i][j];
		matrix_destroy(e);
		
		// B
		e = matrix_pyrup(g_a[B_CHANNEL][k]); CHECK_MATRIX(e);
		matrix_foreach(g_a[B_CHANNEL][k - 1], i, j)
			g_a[B_CHANNEL][k - 1]->me[i][j] += e->me[i][j];
		matrix_destroy(e);
	}

	// Copy Result
	image_setplane(c, 'R', g_a[R_CHANNEL][0]);
	image_setplane(c, 'G', g_a[G_CHANNEL][0]);
	image_setplane(c, 'B', g_a[B_CHANNEL][0]);
	image_gauss_filter(c, 1);

	// Delete g_b
	for (k = 1; k <= layers; k++) {
		matrix_destroy(g_b[R_CHANNEL][k]);
		matrix_destroy(g_b[G_CHANNEL][k]);
		matrix_destroy(g_b[B_CHANNEL][k]);
	}

	// Delete g_a
	for (k = 1; k <= layers; k++) {
		matrix_destroy(g_a[R_CHANNEL][k]);
		matrix_destroy(g_a[G_CHANNEL][k]);
		matrix_destroy(g_a[B_CHANNEL][k]);
	}

	// Delete g_mask
	for (k = 1; k <= layers; k++) {
		image_destroy(g_mask[k]);
	}

	if (debug) {
		time_spend("Multi-band blending");
	}

	return c;
}
