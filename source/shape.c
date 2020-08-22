
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/
#include "common.h"
#include "image.h"
#include "filter.h"

#define MAYBE_SKELETON_COLOR 128

static int __matrix_8connect(MATRIX *mat, int r, int c)
{
        int k, sum = 0;
        int nb[8][2] = { { 0, 1 }, {-1, 1}, { -1, 0 }, {-1, -1}, { 0, -1}, {1, -1}, { 1,  0 }, {1, 1}};
        int e[9];

        for (k = 0; k < 8; k++)
                e[k] = ABS(mat->me[r + nb[k][0]][c + nb[k][1]]) > MIN_DOUBLE_NUMBER? 0 : 1;

        e[8] = e[0];
        for (k = 0; k < 8; k += 2)
                sum += e[k] - e[k] * e[k + 1] * e[k + 2];

        return sum;
}

static int __higher_cross(char orgb, IMAGE *img, int i, int j, int mid_thr)
{
    int k, a, b, delta;
    static int nb[4][2] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0}};

    delta = mid_thr/32 + 1;

	if (image_getvalue(img, orgb, i, j) < mid_thr + delta)
			return 0;
	
	a = b = 0;
	for (k = 0; k < 4; k++) {
			if (image_getvalue(img, orgb, i + nb[k][0], j + nb[k][1]) < mid_thr)
					a++;
			if (image_getvalue(img, orgb, i + nb[k][0], j + nb[k][1]) > mid_thr)
					b++;
	}
	return (a > 0 && b > 0)? 1 : 0;   // cell split nb with different world
}

// gm -- gradient m. ...
static int __canny_histsumv(IMAGE *img, double hirate)
{
	int i, j, hist[256], maxgm, hcount;
	
	for(j = 0; j < 256; j++)
		hist[j] = 0;
	
	for (i = 1; i < (int)img->height - 1; i++) {
		for (j = 1; j < (int)img->width - 1; j++) {
			if (img->ie[i][j].b == 128)
				hist[img->ie[i][j].g]++;
		}
	}
	for (i = maxgm = 0, j = 1; j < 256; j++) {
		if (hist[j])
			maxgm = j;
		i += hist[j];
	}
	hcount = (int) (hirate * i + 0.5);
	// Count max gradient (more than hcount)
	for (i = 0, j = 1; j < maxgm && i < hcount; j++) 
		i += hist[j];
	return j;
}

// 8 neighbour trace
static void __canny_8nbtrace(IMAGE *img, int i, int j, int lowthreashold)
{
	// 8- neighbour coordinations
	static int jnhs[8] = {1,1,0,-1,-1,-1,0,1};
	static int inhs[8] = {0,1,1,1,0,-1,-1,-1};

	if (i < 1 || i >= (int)img->height - 1 || j < 1 || j >= (int)img->width - 1)
		return;
	
	int ii, jj, k;
	for(k = 0; k < 8; k++) 	{
		ii = i + inhs[k];
		jj = j + jnhs[k];
		if (img->ie[ii][jj].b == 128 && img->ie[ii][jj].g >= lowthreashold) {
			img->ie[ii][jj].b = 255;
			dot_put(ii, jj);
			__canny_8nbtrace(img, ii, jj, lowthreashold);
		}
	}
}

static void __canny_edgetrace(IMAGE *img, double hr, double lr)
{
	int i, j, k;
	int hithreshold = __canny_histsumv(img, hr);  // High Threshold
	int lothreshold = (int)(hithreshold * lr + 0.5);
	DOTS *dots = dot_set();
	char *p = getenv("CANNY_EDGE_MIN_DOTS");
	int min_edge_dots = (p)? atoi(p) : 4;

	syslog_print("CANNY_EDGE_MIN_DOTS = %d\n", min_edge_dots);

	for (i = 1; i < (int)img->height - 1; i++) {
		for (j = 1; j < (int)img->width - 1; j++) {
			if (img->ie[i][j].b != 128 || img->ie[i][j].g < hithreshold)
				continue;
			dots->count = 0;
			img->ie[i][j].b = 255;
			dot_put(i, j);
			__canny_8nbtrace(img, i, j, lothreshold);
			if (dots->count < min_edge_dots) {
				for (k = 0; k < dots->count; k++)
					img->ie[dots->dot[k].r][dots->dot[k].c].b = 0;
			}
		}
	}

	// Trace end, other points is impossiable to be border
	image_foreach(img, i, j) {
		if (img->ie[i][j].b != 255)
			img->ie[i][j].b = 0;

	}
}

static int __canny_gradient(IMAGE *img)
{
	int i, j, g1, g2, g3, g4, gx, gy;
	double weight, dt1, dt2;
	
	for (i = 1; i < (int)img->height - 1; i++) {
		for (j = 1; j < (int)img->width - 1; j++) {
			gx = img->ie[i][j + 1].r - img->ie[i][j - 1].r;
			gy = img->ie[i + 1][j].r - img->ie[i - 1][j].r;
			img->ie[i][j].g = (BYTE) (sqrt((double)(gx * gx + gy * gy)/2.0f));	// <= 255
		}
	}

	for (i = 1; i < (int)img->height - 1; i++) {
		for (j = 1; j < (int)img->width - 1; j++) {
			img->ie[i][j].b = 0;
			if (img->ie[i][j].g == 0)
				continue;

			gx = img->ie[i][j + 1].r - img->ie[i][j - 1].r;
			gy = img->ie[i + 1][j].r - img->ie[i - 1][j].r;

			// gm != 0 
			if(fabs((double)gy) > fabs((double)gx)) {
				weight = fabs((double)gx)/fabs((double)gy);
				g2 = img->ie[i-1][j].g;
				g4 = img->ie[i+1][j].g;
				if(gx * gy > 0) 	{
					g1 = img->ie[i-1][j-1].g; 	//	 /
					g3 = img->ie[i+1][j+1].g;
				}
				else {
					g1 = img->ie[i-1][j+1].g;	//	
					g3 = img->ie[i+1][j-1].g;
				}
			}
			else {
				weight = fabs((double)gy)/fabs((double)gx);
				g2 = img->ie[i][j+1].g;
				g4 = img->ie[i][j-1].g;
				if(gx * gy > 0) 	{
					g1 = img->ie[i+1][j+1].g;  // 	/
					g3 = img->ie[i-1][j-1].g;
				}
				else {
					g1 = img->ie[i-1][j+1].g; //	
					g3 = img->ie[i+1][j-1].g;
				}
			}
			
			dt1 = weight*g1 + (1 - weight)*g2;
			dt2 = weight*g3 + (1 - weight)*g4;
			// if(gm->me[i][j] >= dt1 && gm->me[i][j] >= dt2)
			if(img->ie[i][j].g > dt1 && img->ie[i][j].g > dt2)
				img->ie[i][j].b = 128; // maybe is border point
			
		}
	}

	return RET_OK;
}

// shape_canny('R', image, 0.9, 0.75), Canny edge
static int __canny_edge_detect(IMAGE *img, double hr, double lr)
{
	check_image(img);

	// data storage format: R -- raw data,  G-- gradient m ..., B -- edge  !!!

	if (__canny_gradient(img) != RET_OK) {
		syslog_error("Canny gradient.");
		return RET_ERROR;
	}

	__canny_edgetrace(img, hr, lr);

	return RET_OK;
}

// Make sure mat is valid, for 3x3 template, center is (1,1), ndim is 3
static int __temp_match(MATRIX *mat, int r, int c, int ndim, int *temp)
{
	int i, j, dim, d, g;

	dim = ndim/2;	// template center is (dim, dim)
	if (r < dim || r > mat->m - 1 - dim || c < dim || c > mat->n - 1 - dim)
		return 0;

	for (i = -dim; i <= dim; i++) {
		for (j = -dim; j <= dim; j++) {
			d = temp[(i + dim) *ndim + (j + dim)];
			if (d == 0)
				continue;
			g = (int)mat->me[r + i][c + j];
			if (g == 0)
				return 0;
		}
	}

	return 1;
}

static int __skeleton_sort(MATRIX *mat)
{
	int i, j;
	int needrepeat;
	int ts0[9] = { 0, 0, 1, 0, 1, 0, 0, 0, 1};  // ==> '<'
	int ts2[9] = { 1, 0, 1, 0, 1, 0, 0, 0, 0};	// ==> 'v'
	int ts4[9] = { 1, 0, 0, 0, 1, 0, 1, 0, 0};	// ==> '>'
	int ts6[9] = { 0, 0, 0, 0, 1, 0, 1, 0, 1};	// ==> 'A'
	int to[9] = { 0, 1, 0, 1, 0, 1, 0, 1, 1};	// ==> 'O'		// ring

	// Cut isolate and end points
	do {
		needrepeat = 0;

		for(i = 1; i < mat->m - 1; i++) {
			for (j = 1; j < mat->n - 1; j++) {
				if (ABS(mat->me[i][j] - MAYBE_SKELETON_COLOR) >= MIN_DOUBLE_NUMBER)
					continue;
				if (__matrix_8connect(mat, i, j) <= 1) {
					mat->me[i][j] = 0;
					needrepeat = 1;
				}
			}
		}
	} while (needrepeat);


	for (i = 1; i < mat->m - 1; i++) {
		for (j = 1; j < mat->n - 1; j++) {
			// Break ring !!!
			if (__temp_match(mat, i, j, 3, to))  // 3x3 'O'
				mat->me[i][j+1] = 0;
	
			if (ABS(mat->me[i][j] - MAYBE_SKELETON_COLOR) < MIN_DOUBLE_NUMBER)
				continue;
			if (__temp_match(mat, i, j, 3, ts0)) { // 3x3 '<'
				mat->me[i][j] = 0;
				mat->me[i][j+1] = MAYBE_SKELETON_COLOR;
			}
			else if (__temp_match(mat, i, j, 3, ts2)) { // 3x3 'v'
				mat->me[i][j] = 0;
				mat->me[i-1][j] = MAYBE_SKELETON_COLOR;
			}
			else if (__temp_match(mat, i, j, 3, ts4)) { // 3x3 '>'
				mat->me[i][j] = 0;
				mat->me[i][j-1] = MAYBE_SKELETON_COLOR;
			}
			else if (__temp_match(mat, i, j, 3, ts6)) { // 3x3 'A'
				mat->me[i][j] = 0;
				mat->me[i+1][j] = MAYBE_SKELETON_COLOR;
			}
		}
	}
						
	return RET_OK;
}


static int __contour_border(MATRIX *mat, int r, int c)
{
	int di, dj, k;
	static int d3[4][2] = { { -1, 0}, {0, -1}, {0, 1}, {1, 0} }; // 3d.txt

	if (ABS(mat->me[r][c]) <= MIN_DOUBLE_NUMBER) 	// Make sure (r, c) is not on the border
		return 0;

	for (k = 0; k < 4; k++) {
		di = d3[k][0];
		dj = d3[k][1];
		if (ABS(mat->me[r + di][c + dj]) <= MIN_DOUBLE_NUMBER)
			return 1;
	}
	return 0;
}

static int __matrix_contour(MATRIX *mat)
{
	int i, j, count, height;
	MATRIX *copy;

	check_matrix(mat);
	copy = matrix_copy(mat); check_matrix(copy);

	matrix_clear(mat);
	height = 1;
	do { 	// repeat erode
	 	count = 0;
		for (i = height; i < copy->m - height; i++) {
			for (j = height; j < copy->n  - height; j++) {
				if (__contour_border(copy, i, j)) {
					mat->me[i][j] = height;
					count++;
				}
			}
		}
		if (count > 0) {  // ==> height line exists 
			for (i = height; i < copy->m - height; i++) {
				for (j = height; j < copy->n - height; j++) {
					if (mat->me[i][j] == height)
						copy->me[i][j] = 0;
				}
			}
			height++;
	 	}
	} while(count > 0);

	matrix_destroy(copy);
 
	return height;	
}

static void __skeleton_trace(MATRIX *mat, MATRIX *label, int r, int c)
{
	int max, k, n, e[8];
	static int nb[8][2] = { { 0, 1 }, {-1, 1}, { -1, 0 }, {-1, -1}, { 0, -1}, {1, -1}, { 1,  0 }, {1, 1}};

	if ((int)label->me[r][c] == 255)  // Traced ?
		return;

	label->me[r][c] = 255.0f; // mark (r,c) as trace point

	if (r < 1 || r >= label->m - 1 || c < 1 || c >= label->n - 1)
		return;
	
	// balance ?
	if ((int)mat->me[r][c - 1] == (int)mat->me[r][c+1] && (int)mat->me[r-1][c] == (int)mat->me[r+1][c])
		return;

	// Calculate energy, find max energy points, maybe >= 2
	for (k = 0; k < 4; k++) {
		n = (int)(mat->me[r][c] - mat->me[r + nb[2 * k][0]][c + nb[2 * k][1]]);
		e[2 * k] = (n > 0)? n : 0;
	}
	e[1] = e[0] + e[2];
	e[3] = e[2] + e[4];
	e[5] = e[4] + e[6];
	e[7] = e[6] + e[0];
	max = e[0];
	for (k = 1; k < 8; k++) {
		if (e[k] > max)
			max = e[k];
	}
	if (max <= 1)  // Power is not big enough !
		return;
	for (k = 0; k < 8; k++) {
		if (e[k] == max && ABS(mat->me[r][c] - mat->me[r + nb[k][0]][c + nb[k][1]] - 1) < MIN_DOUBLE_NUMBER)
			__skeleton_trace(mat, label, r + nb[k][0], c + nb[k][1]);
	}
}

static int __sharp_angle(MATRIX *mat, int r, int c)
{
	int k, s, e, level;
	static int nb[8][2] = { { 0, 1 }, {-1, 1}, { -1, 0 }, {-1, -1}, { 0, -1}, {1, -1}, { 1,  0 }, {1, 1}};

	level = (int)mat->me[r][c];
	if (level < 2)  // energy should be power enough
		return 0;
	
	s = 8; e = 0;
	for (k = 0; k < 8; k++) {
		if ((int)mat->me[r + nb[k][0]][c + nb[k][1]] == level) {
			s = k;
			break;
		}
	}
	for (k = s + 1; k < 8; k++) {
		if ((int)mat->me[r + nb[k][0]][c + nb[k][1]] == level) {
			e = k;
			break;
		}
	}
	return k = (e - s) > 0 && (e - s) != 4;
}

// Make sure mat's fg is 255 and bg is 0
static int __skeleton_matrix(MATRIX *mat)
{
	int i, j, height;
	MATRIX *contmat;		// contour matrix

	check_matrix(mat);
	contmat = matrix_copy(mat); check_matrix(contmat);
	height = __matrix_contour(contmat);

	matrix_clear(mat);
	if (height >= 1) {
		for(i = 1; i < mat->m - 1; i++) {
			for (j = 1; j < mat->n - 1; j++) {
				 if (__sharp_angle(contmat, i, j))
					mat->me[i][j] = MAYBE_SKELETON_COLOR;
			}
		}
		 __skeleton_sort(mat);
 		for(i = 1; i < mat->m - 1; i++) {
			for (j = 1; j < mat->n - 1; j++) {
				if (matrix_localmax(contmat, i, j)) 				//	mat->me[i][j] = 255;
					 __skeleton_trace(contmat, mat, i, j);
			}
		}
 		matrix_foreach(mat, i, j) {
			if ((int)mat->me[i][j] != 255)
				mat->me[i][j]  = 0;
		}
 	}
	matrix_destroy(contmat);

	return RET_OK;	
}


VECTOR *shape_vector(IMAGE *img, RECT *rect, int ndim)
{
	BYTE g;
	double d,  r, a;
	int i, j, k, n, crow, ccol, arcstep;
	long long sum_all, sum_arc[360];
	VECTOR *vec;
	IMAGE *sub;

	CHECK_IMAGE(img);
	if (ndim > 360) {
		syslog_error("NOT support dimension %d.", ndim);
		return NULL;
	}
	vec = vector_create(ndim); CHECK_VECTOR(vec);

	// Initialise
	arcstep = 360 / ndim;
	sum_all = 0;
	for (j = 0; j < ndim; j++)
		sum_arc[j] = 0;

	if (rect) {
		sub = image_subimg(img, rect); CHECK_IMAGE(sub);
		shape_bestedge(sub);
 		image_mcenter(sub, 'A', &crow, &ccol);
		image_foreach(sub, i, j) {
			g = image_getvalue(sub, 'A', i, j);
			if (g == 0)
				continue;
			math_topolar(j - ccol, i - crow, &r, &a);
			n = math_arcindex(a, arcstep);
			k = r * g;	
			sum_arc[n] += k;
			sum_all += k;
		}
		image_destroy(sub);
	}
	else {
		shape_bestedge(img);
		image_mcenter(img, 'A', &crow, &ccol);
		image_foreach(img, i, j) {
			g = image_getvalue(img, 'A', i, j);
			if (g == 0)
				continue;
			math_topolar(j - ccol, i - crow, &r, &a);
			n = math_arcindex(a, arcstep);
			k = r * g;	
			sum_arc[n] += k;
			sum_all += k;
		}
	}

	n = 0;
	if (sum_all) {
		for (i = 0; i < vec->m; i++) {
			if (sum_arc[i] > sum_arc[n])
				n = i;
			d = (double)sum_arc[i] /sum_all;
			vec->ve[i] = d;
		}
	}
	else {
		syslog_error("All sum == 0.");
	}

#if 0	
	d = arcstep; d *= n;  d = 360 - d;
	printf("n = %d, (%d, %d, %.4f)\n", n, crow, ccol, d);
	//arcstep = 360 - arcstep;
	d /= 180; d *= MATH_PI;
	printf("arcstep = %.4f, sin() = %.2f, cos = %.2f\n", d, sin(d), cos(d));

	image_drawtext(img, crow, ccol, "Start", 0x00ffff);
	image_drawtext(img, crow - 100*sin(d), ccol + 100*cos(d), "Stop", 0x00ffff);

	image_drawline(img, crow, ccol, crow - 100*sin(arcstep), ccol + 100*cos(arcstep), 0x00ffff);
	image_drawline(img, crow, ccol, crow + 10, ccol - 10, 0x00ffff);
#endif

	return vec;
}

double shape_entropy(IMAGE *img, RECT *rect, int ndim)
{
	VECTOR *v;
	double d = -1.0f;
	v = shape_vector(img, rect, ndim); check_vector(v);
	d = vector_entropy(v);
	vector_destroy(v);

	return d;
}

double shape_likeness(IMAGE *f, IMAGE *g, RECT *rect, int ndim)
{
	double avgd;
	VECTOR *fs, *gs;

	fs = shape_vector(f, rect, ndim); check_vector(fs);
	gs = shape_vector(g, rect, ndim); check_vector(gs);

	avgd = vector_likeness(fs, gs);
	
	vector_destroy(fs);
	vector_destroy(gs);
	
	return avgd;
}

int image_contour(IMAGE *img)
{
	int i, j, n;
	MATRIX *mat;

	check_image(img);
	mat = matrix_create(img->height, img->width); check_matrix(mat);

	// Convert image to bitmap on channel 'A'
	n = color_midval(img, 'A');
	
	image_foreach(img, i, j)
		mat->me[i][j] = (image_getvalue(img, 'A', i, j) < n)? 0.0f : 255.0f;
	n = __matrix_contour(mat);
	
	// NOW contour results are saved in matrix mat 
	matrix_foreach(mat, i, j) {
		n = (int)mat->me[i][j];
		if (n && (n % 5) == 0)
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 255;
		else
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 0;
	}
	matrix_destroy(mat);
 
	return RET_OK;
}

int image_skeleton(IMAGE *img)
{
	int i, j, n, ret;
	MATRIX *mat;

	check_image(img);
	mat = matrix_create(img->height, img->width); check_matrix(mat);

	// Convert image to bitmap on channel 'A'
	n = color_midval(img, 'A');
	
	image_foreach(img, i, j)
		mat->me[i][j] = (image_getvalue(img, 'A', i, j) < n)? 0 : 255;
	ret = __skeleton_matrix(mat);
	
	// Save skeleton 
	image_foreach(img, i, j) {
		if ((int)mat->me[i][j])
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 255;
		else
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 0;
	}
	matrix_destroy(mat);
 
	return ret;
}

// Middle value edge detection
int shape_midedge(IMAGE *img)
{
 	int i, j, mid_thr;
	MATRIX *mat;

	check_image(img);
	mat = matrix_create(img->height, img->width); 	check_matrix(mat);
 	mid_thr = color_midval(img, 'A');

	image_foreach(img, i, j) {
		mat->me[i][j] = 0;
		if (i == 0 || i == img->height - 1 || j == 0 || j == img->width - 1)  // skip border points
			continue;

		if (__higher_cross('A', img, i, j, mid_thr))
			mat->me[i][j] = 1;
	}
	image_foreach(img, i, j) {
		if ((int)mat->me[i][j])
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 255;
		else
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 0;
	}
	matrix_destroy(mat);

	return RET_OK;
}

int shape_bestedge(IMAGE *img)
{
	char *p;
	int i, j, ret;
	double hr, lr;

	check_image(img);

	image_foreach(img, i, j)
		img->ie[i][j].r = image_getvalue(img, 'A', i, j);
	
	img->format = IMAGE_GRAY;
	image_gauss_filter(img, 2);
	
	p = getenv("CANNY_EDGE_HI_THRESHOLD");
	hr = (p)? atof(p) : 0.80;
	p = getenv("CANNY_EDGE_LO_THRESHOLD");
	lr = (p)? atof(p) : 0.75;
	syslog_print("CANNY_EDGE_HI_THRESHOLD = %f, CANNY_EDGE_LO_THRESHOLD = %f\n", hr, lr);
	ret = __canny_edge_detect(img, hr, lr);
	image_foreach(img, i, j) {
		if (img->ie[i][j].b == 255)
			img->ie[i][j].r = img->ie[i][j].g = 255;
		else
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = 0;
	}
	
	return ret;
}

