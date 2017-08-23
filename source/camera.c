/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Wed Jun 21 20:20:15 PDT 2017
***
************************************************************************************/

#include "camera.h"
#include "color.h"
#include "filter.h"

#define CONFIG_FAST_CONVERT 1

// 3D to 2D
static void __world_toimage(CAMERA *c, double xx, double yy, double zz, double *x2d, double *y2d)
{
	double inv_z, xd, yd, r, theta, theta2, theta4, theta_d;
	double x = c->r[0] * xx + c->r[1] * yy + c->r[2] * zz + c->t[0];
	double y = c->r[3] * xx + c->r[4] * yy + c->r[5] * zz + c->t[1];
	double z = c->r[6] * xx + c->r[7] * yy + c->r[8] * zz + c->t[2];

	if (ABS(z) > MIN_DOUBLE_NUMBER) {
		inv_z = 1.f / z;
		x *= inv_z;
		y *= inv_z;
		r = sqrt(x*x + y*y);
	}
	else {
		r = 0.0f;
	}

	if (ABS(r) <= MIN_DOUBLE_NUMBER) {
		xd = 0;
		yd = 0;
	}
	else {
		theta = atan(r);
		theta2 = theta * theta;
		theta4 = theta2 * theta2;
		theta_d = theta*(1 + c->k0 * theta2 + c->k1 * theta4);

		xd = x*theta_d / r;
		yd = y*theta_d / r;
	}
	
	*x2d = (xd * c->fx + c->cx);
	*y2d = (yd * c->fy + c->cy);
}

int camera_loadconf(FILE *fp, CAMERA *c)
{
	int i;
	double mat[3*3];
	/*
		fx 0  cx
		0  fy cy
		0  0   1
	*/
	for (i = 0; i < ARRAY_SIZE(mat); i++) {
		if (fscanf(fp, "%lf", &mat[i]) != 1)
			return RET_ERROR;
	}
	c->fx = mat[0];
	c->fy = mat[4];
	c->cx = mat[2];
	c->cy = mat[5];
	
	// Distort K0, K1
	if (fscanf(fp, "%lf %lf", &(c->k0), &(c->k1)) != 2)
		return RET_ERROR;

	// Rotation
	for (i = 0; i < ARRAY_SIZE(c->r); i++) {
		if (fscanf(fp, "%lf", &(c->r[i])) != 1)
			return RET_ERROR;
	}

	// Translation 
	if (fscanf(fp, "%lf %lf %lf", &(c->t[0]), &(c->t[1]), &(c->t[2])) != 3)
		return RET_ERROR;

	if (fscanf(fp, "%lf %lf %lf", &(c->r_gain), &(c->g_gain), &(c->b_gain)) != 3)
		return RET_ERROR;

	
	return RET_OK;
}

int camera_saveconf(CAMERA *c, FILE *fp)
{
	int i;

	fprintf(fp, "%lf %lf %lf\n", c->fx, 0.00f, c->cx);
	fprintf(fp, "%lf %lf %lf\n", 0.00f, c->fy, c->cy);
	fprintf(fp, "%lf %lf %lf\n", 0.00f, 0.00f, 1.00f);

	fprintf(fp, "%lf %lf\n", c->k0, c->k1);

	// External parameters
	// Rtotation
	for (i = 0; i < 3*3; i++)
		fprintf(fp, "%lf%c", c->r[i], ((i%3) == 2)? '\n':' ');

	// Translation 
	for (i = 0; i < 3; i++)
		fprintf(fp, "%lf%c", c->t[i], ((i % 3) == 2)? '\n':' ');

	fprintf(fp, "%lf %lf %lf\n", c->r_gain, c->g_gain, c->b_gain);

	return RET_OK;
}

// Setup view port
int camera_create(CAMERA *cam, int height, int width)
{
	cam->image = image_create(height, width); check_image(cam->image);

	cam->r_map = matrix_create(height, width); check_matrix(cam->r_map);
	cam->c_map = matrix_create(height, width); check_matrix(cam->c_map);

	return RET_OK;
}

int camera_mapworld(CAMERA *cam, RECT *world, int scale)
{
	int c, r;
	double c3d, r3d, c2d, r2d;	// -- world c, r
	double hstep, wstep;

	cam->world.r = world->r*scale;
	cam->world.c = world->c*scale;
	cam->world.h = world->h*scale;
	cam->world.w = world->w*scale;

	hstep = cam->world.h; hstep /= camera_height(cam);
	wstep = cam->world.w; wstep /= camera_width(cam);
	r3d = cam->world.r + cam->world.h;
	for (r = 0; r < camera_height(cam); r++) {
		c3d = cam->world.c;
 		for (c = 0; c < camera_width(cam); c++) {
			c3d += wstep;

			// (c3d, r3d, 0) --> (2d.c, 2d.r)
			__world_toimage(cam, c3d, r3d, 0.0f, &c2d, &r2d);
			// 
			cam->r_map->me[r][c] = r2d;
			cam->c_map->me[r][c] = c2d;
		}
		r3d -= hstep;
	}

	return RET_OK;
}


/**********************************************************************
d1	  d2
   (p)
d3	  d4
f(i+u,j+v) = (1-u)(1-v)f(i,j) + (1-u)vf(i,j+1) + u(1-v)f(i+1,j) + uvf(i+1,j+1)
**********************************************************************/

// Src is raw image with fisheye, and result in camera->image
// xxxx666
int camera_defisheye(CAMERA *cam, IMAGE *src, RECT *rect)
{
	int i, j, r, c;
	
	IMAGE *img = camera_image(cam);

	check_image(src);

	image_foreach(img, i, j) {
		r = (int)cam->r_map->me[i][j];
		c = (int)cam->c_map->me[i][j];

		r += rect->r;
		c += rect->c;
		r = CLAMP(r, 0, src->height - 1);	// xxxx666
		c = CLAMP(c, 0, src->width - 1);

#ifdef CONFIG_FAST_CONVERT
		img->ie[i][j].r = src->ie[r][c].r;
		img->ie[i][j].g = src->ie[r][c].g;
		img->ie[i][j].b = src->ie[r][c].b;
#else
		if (r == src->height - 1 || c == src->width - 1) {
			img->ie[i][j].r = src->ie[r][c].r;
			img->ie[i][j].g = src->ie[r][c].g;
			img->ie[i][j].b = src->ie[r][c].b;
		}
		else {
			double d1, d2, d3, d4, u, v, d;

			u = cam->r_map->me[i][j] - (int)cam->r_map->me[i][j];
			v = cam->c_map->me[i][j] - (int)cam->c_map->me[i][j];

			// R
			d1 = src->ie[r][c].r;
			d2 = src->ie[r][c + 1].r;
			d3 = src->ie[r + 1][c].r;
			d4 = src->ie[r + 1][c + 1].r;
			d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
			img->ie[i][j].r = CLAMP((int)d, 0, 255);

			// G
			d1 = src->ie[r][c].g;
			d2 = src->ie[r][c + 1].g;
			d3 = src->ie[r + 1][c].g;
			d4 = src->ie[r + 1][c + 1].g;
			d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
			img->ie[i][j].g = CLAMP((int)d, 0, 255);

			// B
			d1 = src->ie[r][c].b;
			d2 = src->ie[r][c + 1].b;
			d3 = src->ie[r + 1][c].b;
			d4 = src->ie[r + 1][c + 1].b;
			d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
			img->ie[i][j].b = CLAMP((int)d, 0, 255);
		}
#endif
	}

	// Debug
#if 0	
	image_foreach(img, i, j) {
		r = (int)cam->r_map->me[i][j];
		c = (int)cam->c_map->me[i][j];
		
		r += rect->r;
		c += rect->c;
		
		r = CLAMP(r, 0, src->height - 1);
		c = CLAMP(c, 0, src->width - 1);
		
		// Draw border ...
		if (i == 0 || j == 0 || i == img->height - 1 || j == img->width - 1) {
			src->ie[r][c].r = 0x7f;
			src->ie[r][c].g = 0xff;
			src->ie[r][c].b = 0x00;
		}
		if (i == 250 || j == 200) {
			src->ie[r][c].r = 0x00;
			src->ie[r][c].g = 0x00;
			src->ie[r][c].b = 0xff;
		}
	}
#endif

	return RET_OK;
}

// Learning gains ...
int camera_learning(CAMERA *cam)
{
	RECT rect;
	image_rect(&rect, cam->image);
	return camera_rect_gain(cam, cam->image, &rect);
}

int camera_rect_gain(CAMERA *cam, IMAGE *src, RECT *rect)
{
	double r_gain, g_gain, b_gain, sum;

	color_rect_prmgain(src, rect, &r_gain, &g_gain, &b_gain);

//	color_rect_gwmgain(src, rect, &r_gain, &g_gain, &b_gain);

	sum = cam->r_gain + cam->g_gain + cam->b_gain;
	if (sum < MIN_DOUBLE_NUMBER || sum > 100.0f) {
		cam->r_gain = r_gain;
		cam->g_gain = g_gain;
		cam->b_gain = b_gain;
	}
	else {	// Filter for video frame
		cam->r_gain = (0.4f*cam->r_gain + 0.6f*r_gain);
		cam->g_gain = (0.4f*cam->g_gain + 0.6f*g_gain);
		cam->b_gain = (0.4f*cam->g_gain + 0.6f*b_gain);
	}
	return RET_OK;
}


// auto white balance
int camera_awb(CAMERA *cam)
{
	return color_correct(cam->image, cam->r_gain, cam->g_gain, cam->b_gain);
}

int camera_height(CAMERA *cam)
{
	return cam->image->height;
}

int camera_width(CAMERA *cam)
{
	return cam->image->width;
}

IMAGE *camera_image(CAMERA *cam)
{
	return cam->image;
}

void camera_destroy(CAMERA *cam)
{
	if (cam) {
		image_destroy(cam->image);
		matrix_destroy(cam->c_map);
		matrix_destroy(cam->r_map);
	}
}

int camera_filter(CAMERA *cam)
{
	return image_guided_filter(camera_image(cam), NULL, 20, 0.01f, 2, 0);
}

