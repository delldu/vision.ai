
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/


#include <math.h>
#include "image.h"

// Threshold Local Binary Pattern
static int texture_tlbp8(char orgb, IMAGE *img, int r, int c, int k)
{
	BYTE gn, gc;
	int j, lbp;
	static int nb[8][2] = {{-1, -1}, {-1, 0}, { -1, 1}, { 0, -1}, {0, 1}, {1, -1}, { 1,  0 }, {1, 1}};

	if (r < 1 || r >= img->height -1 || c < 1 || c >= img->width - 1)
		return 0;

	// (x - c) > (c) * t <==> x > (1.00 + t) * c
	lbp = 0;
	gc = image_getvalue(img, orgb, r, c);
	for (j = 0; j < 8; j++) {
		gn = image_getvalue(img, orgb, r + nb[j][0], c + nb[j][1]);
		if (ABS(gn - gc) >= k)
			lbp += (0x1 << j);
	}
	
	return lbp;
}

VECTOR *texture_vector(IMAGE *img, RECT *rect, int ndim)
{
	VECTOR *vec;
	int i, j, qlevel, count[256];
	IMAGE *sub;
	
	CHECK_IMAGE(img);

	if (ndim > 256) {
		syslog_error("NOT support dimension %d.", ndim);
		return NULL;
	}

	vec = vector_create(ndim); CHECK_VECTOR(vec);
	if (rect) {
		sub = image_subimg(img, rect); CHECK_IMAGE(sub);
		image_foreach(sub, i, j)
			count[texture_tlbp8('A', sub, i, j, 1)]++;
		image_destroy(sub);
	}
	else {
		image_foreach(img, i, j)
			count[texture_tlbp8('A', img, i, j, 1)]++;
	}

	qlevel = 256/ndim;
	vector_foreach(vec, i)
		vec->ve[i] += count[i/qlevel];

	return vec;
}

double texture_entropy(IMAGE *img, RECT *rect, int ndim)
{
	VECTOR *v;
	double d = -1.0f;
	v = texture_vector(img, rect, ndim);
	check_vector(v);
	d = vector_entropy(v);
	vector_destroy(v);

	return d;
}

double texture_likeness(IMAGE *f, IMAGE *g, RECT *rect, int ndim)
{
	double avgd;
	VECTOR *fs,*gs;

	fs = texture_vector(f, rect, ndim); check_vector(fs);
	gs = texture_vector(g, rect, ndim); check_vector(gs);

	avgd = vector_likeness(fs, gs);
	
	vector_destroy(fs);
	vector_destroy(gs);
	
	return avgd;
}

