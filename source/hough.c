
/************************************************************************************
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

// Hough transform:
// 	Line: r = y * cos(theta) + x * sin(theta) <==> (r, theta)
//   Circle:  x = x0 + r * cos(theta), y = y0 + r * sin(theta)
//   Oval:  x = x0 + a * cos(theta), y = y0 + b * sin(theta)

#include "vector.h"
#include "matrix.h"
#include "image.h"
#include <math.h>

static int __hough_line(IMAGE *image, int threshold, int debug)
{
	int i, j, n, r, rmax;
	double d;
	VECTOR *cosv, *sinv;
	MATRIX *hough, *begrow, *endrow, *begcol, *endcol;
	LINES *lines = line_set();

	lines->count = 0;
	
	check_image(image);
	cosv = vector_create(180); check_vector(cosv);
	sinv = vector_create(180);  check_vector(sinv);
	for (i = 0; i < 180; i++) {
		sinv->ve[i] = sin(i * MATH_PI/180);
		cosv->ve[i] = cos(i * MATH_PI/180);
	}

	rmax = (int)sqrt(image->height * image->height + image->width * image->width) + 1;
	hough = matrix_create(2 * rmax, 180); check_matrix(hough);
	begrow = matrix_create(2 * rmax, 180); check_matrix(begrow);
	endrow = matrix_create(2 * rmax, 180); check_matrix(endrow);
	begcol = matrix_create(2 * rmax, 180); check_matrix(begcol);
	endcol = matrix_create(2 * rmax, 180); check_matrix(endcol);
//	shape_midedge(image);
	shape_bestedge(image);

	image_foreach(image, i, j) {
		if (! image->ie[i][j].r)
			continue;
		for (n = 0; n < 180; n++) {
			d = i * cosv->ve[n] + j * sinv->ve[n];
			r = (int)(d + rmax);
			hough->me[r][n]++;
			if ((int)hough->me[r][n] == 1) {
				begrow->me[r][n] = i;
				begcol->me[r][n] = j;
			}
			endrow->me[r][n] = i;
			endcol->me[r][n] = j;
		}
	}

	matrix_foreach(hough, i, j) {
		if ((int)hough->me[i][j] >=  threshold) {
			line_put((int)begrow->me[i][j], (int)begcol->me[i][j], (int)endrow->me[i][j], (int)endcol->me[i][j]);
			if (debug)
				image_drawline(image, (int)begrow->me[i][j], (int)begcol->me[i][j], (int)endrow->me[i][j], (int)endcol->me[i][j], 0x00ffff);
		}
	}

	matrix_destroy(hough);
	matrix_destroy(begrow);
	matrix_destroy(endrow);
	matrix_destroy(begcol);
	matrix_destroy(endcol);

	return RET_OK;
}

int  line_detect(IMAGE *img, int debug)
{
	return __hough_line(img, MIN(img->height, img->width)/10, debug);
}

