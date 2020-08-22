
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/


#include <math.h>
#include "image.h"
#include "color.h"

// CIE 1931:
// C:   98.07, 100.00, 118.22
// D50: 96.42, 100.00,  82.51
// D65: 95.04, 100.00, 108.88
// CIE 1964
// C:   97.29, 100.00, 116.14
// D50: 96.72, 100.00,  81.43
// D65: 94.81, 100.00, 107.32


// Follow CIE 1931, D65

#define  LAB_X_R  0.433953f /* = xyzXr / 0.950456 */
#define  LAB_X_G  0.376219f /* = xyzXg / 0.950456 */
#define  LAB_X_B  0.189828f /* = xyzXb / 0.950456 */

#define  LAB_Y_R  0.212671f /* = xyzYr */
#define  LAB_Y_G  0.715160f /* = xyzYg */ 
#define  LAB_Y_B  0.072169f /* = xyzYb */ 

#define  LAB_Z_R  0.017758f /* = xyzZr / 1.088754 */
#define  LAB_Z_G  0.109477f /* = xyzZg / 1.088754 */
#define  LAB_Z_B  0.872766f /* = xyzZb / 1.088754 */

extern int color_rgbcmp(RGB *c1, RGB *c2);

static int __color_rgbcmp(const void *p1, const void *p2)
{
	RGB *c1 = *(RGB * const *)p1;
	RGB *c2 = *(RGB * const *)p2;
	return color_rgbcmp(c1, c2);
}


// Color is skin ? Maybe !!!
int color_beskin(BYTE r, BYTE g, BYTE b)
{
	BYTE y, cb, cr;

	color_rgb2ycbcr(r, g, b, &y, &cb, &cr);
	if (y < 64 || y > 235)
		return 0;

	// Cb in [77, 127], Cr in [133, 173]
	if (cb >= 77 && cb <= 127 && cr >= 133 && cr <= 173)
		return 1;
	
	// cb in [80, 135], cr in [136, 177]
	if (cb >= 80 && cb <= 135 && cr >= 136 && cr <= 177)
		return 1;

	return 0;
}


// RGB color space
int color_rgbcmp(RGB *c1, RGB *c2)
{
	if (c1->g != c2->g)
		return (c1->g - c2->g);
	if (c1->r != c2->r)
		return (c1->r - c2->r);
	return (c1->b - c2->b);
}



// L¡G0¡X100¡@a¡G-128¡X127¡@b¡G-128¡X127¡@  ?
void color_rgb2lab(BYTE R, BYTE G, BYTE B, double *L, double *a, double *b)
{
	double x, y, z;
	
	x = LAB_X_R * R + LAB_X_G * G + LAB_X_B * B;
	y = LAB_Y_R * R + LAB_Y_G * G + LAB_Y_B * B;
	z = LAB_Z_R * R + LAB_Z_G * G + LAB_Z_B * B;
	x /= 255.f;
	y /= 255.f;
	z /= 255.f;

//	printf("R = %d, G = %d, B = %d, x = %f, y = %f, z = %f\n", R, G, B, x, y, z);
	
	if( x > 0.008856f )
		x = pow(x, 0.333333f);
	else
		x = 7.787f * x + 0.137931f;	 // 0.137913 = 16/116
	
	if( z > 0.008856f )
		z = pow(z, 0.333333f);
	else
		z = 7.787f * z + 0.137931f;
	
	if( y > 0.008856f ) {
		y = pow(y, 0.333333f);
		*L = 116.f * y - 16.f;
	}
	else {
		*L = 903.3f * y;
		y = 7.787 * y + 0.137931f;
	}
	
	*a = 500.f * (x - y);
	*b = 200.f * (y - z);

//	printf(" ==> L = %f, a = %f, b = %f\n", *L, *a, *b);
}

void color_rgb2luv(BYTE R, BYTE G, BYTE B, double *L, double *u, double *v)
{
	static int rgb2luv_init = 0;
	static double X0, Z0, Y0, u20, v20;

	double x, y, X, Y, Z, d, u2, v2, r, g, b;

	if (rgb2luv_init == 0) {
		X0 = (0.607 + 0.174 + 0.201);
		Y0 = (0.299 + 0.587 + 0.114);
		Z0 = (0.066 + 1.117);

		u20 = 4 * X0 / (X0 + 15 * Y0 + 3 * Z0);
		v20 = 9 * Y0 / (X0 + 15 * Y0 + 3 * Z0);
		rgb2luv_init = 1;
	}

	r = (R <= 20)? (double)(8.715e-4 * R) : (double) pow((R + 25.245) / 280.245, 2.22);
	g = (G <= 20)? (double)(8.715e-4 * G) : (double) pow((G + 25.245) / 280.245, 2.22);
	b = (B <= 20)? (double)(8.715e-4 * B) : (double) pow((B + 25.245) / 280.245, 2.22);

	X = 0.412453 * r + 0.357580 * g + 0.180423 * b;
	Y = 0.212671 * r + 0.715160 * g + 0.072169 * b;
	Z = 0.019334 * r + 0.119193 * g + 0.950227 * b;

	if (X == 0.0 && Y == 0.0 && Z == 0.0) {
		x = 1.0 / 3.0;
		y = 1.0 / 3.0;
	} else {
		d = X + Y + Z;
		x = X / d;
		y = Y / d;
	}

	d = -2 * x + 12 * y + 3;
	u2 = 4 * x / d;
	v2 = 9 * y / d;

	if (Y > 0.008856)
		*L = 116.0 * pow(Y, 1.0 / 3.0) - 16;
	else
		*L = 903.3 * Y;

	*u = 13.0 * (*L) * (u2 - u20);
	*v = 13.0 * (*L) * (v2 - v20);
}


void color_luv2rgb(double L, double u, double v, BYTE *R, BYTE *G, BYTE *B)
{
	static int luv2rgb_init = 0;
	static double X0, Y0, Z0, u20, v20;
	int k;
	double x, y, X, Y, Z, d, u2, v2, vec[3];

	if (luv2rgb_init == 0) {
		X0 = (0.607 + 0.174 + 0.201);
		Y0 = (0.299 + 0.587 + 0.114);
		Z0 = (0.066 + 1.117);

		u20 = 4 * X0 / (X0 + 15 * Y0 + 3 * Z0);
		v20 = 9 * Y0 / (X0 + 15 * Y0 + 3 * Z0);
		luv2rgb_init = 1;
	}

	if (L > 0) {
		Y = (L < 8.0)? L/903.3 : pow((L + 16.0)/116.0, 3.0);
		u2 = u/13.0/L + u20;
		v2 = v/13.0/L + v20;

		d = 6 + 3 * u2 - 8 * v2;
		x = 4.5 * u2 / d;
		y = 2.0 * v2 / d;

		X = x / y * Y;
		Z = (1 - x - y) / y * Y;
	} else {
		X = Y = Z = 0.0;
	}

	vec[0] = (3.240479 * X - 1.537150 * Y - 0.498536 * Z);
	vec[1] = (-0.969256 * X + 1.875992 * Y + 0.041556 * Z);
	vec[2] = (0.055648 * X - 0.204043 * Y + 1.057311 * Z);
	for (k = 0; k < 3; k++) {
		if (vec[k] <= 0.018)
			vec[k] = 255 * 4.5 * vec[k];
		else
			vec[k] =  255 * (1.099 * pow(vec[k], 0.45) - 0.099);
		
		if (vec[k] > 255)
			vec[k] = 255;
		else if (vec[k] < 0)
			vec[k] = 0;
		
		if (k == 0)
			*R = (BYTE) round(vec[k]);
		else if (k == 1)
			*G = (BYTE) round(vec[k]);
		else
			*B = (BYTE) round(vec[k]);
	}
}


void color_rgb2hsv(BYTE R, BYTE G, BYTE B, BYTE *h, BYTE *s, BYTE *v)
{
	int x;
	double dh, ds, dv;
	
	x = MAX(R, G); dv = MAX(x, B);
	if (ABS(dv) > MIN_DOUBLE_NUMBER) {
		x = MIN(R, G); x = MIN(x, B);
		ds = (dv - x)/dv;
	}
	else
		ds = 0;

	if (ABS(dv - R) <= MIN_DOUBLE_NUMBER)  //  dv == R
		dh = 60.0f*(G - B)/ds;
	else if (ABS(dv - G) <= MIN_DOUBLE_NUMBER)  //  dv == G
		dh = 120.0f + 60.0f*(B - R)/ds;
	else 		// dv == B
		dh = 240.0f + 60.0f*(R - G)/ds;

	if (dh < 0)
		dh += 360.0f;

	*h = (BYTE)(255.0f * dh/360.f);
	*s = (BYTE)(255.0f * ds);
	*v = (BYTE)(255.0f * dv);
}

void color_rgb2ycbcr(BYTE R, BYTE G, BYTE B, BYTE *y, BYTE *cb, BYTE *cr)
{
	int y2, cb2, cr2;
	RGB_TO_YCBCR(R, G, B, y2, cb2, cr2);
	*y = (BYTE)y2;
	*cb = (BYTE)cb2;
	*cr = (BYTE)cr2;
}

void color_rgb2ycgcr(BYTE R, BYTE G, BYTE B, BYTE *y, BYTE *cg, BYTE *cr)
{
	int y2, cg2, cr2;
	RGB_TO_YCGCR(R, G, B, y2, cg2, cr2);
	*y = (BYTE)y2;
	*cg = (BYTE)cg2;
	*cr = (BYTE)cr2;
}

// Y, U, V
// Y = 0.299R + 0.587G + 0.114B
// U = -0.147R - 0.289G + 0.436B
// V = 0.615R - 0.515G - 0.100B
void color_rgb2gray(BYTE r, BYTE g, BYTE b, BYTE *gray)
{
	//  double d = 0.299f * r + 0.587 * g + 0.114 * b;
	// *gray = (BYTE)(d + 0.5f);
	int d = (306 * r + 601 * g + 117 * b) >> 10;
	*gray = (BYTE)d;
}

void color_rgbsort(int n, RGB *cv[])
{
	qsort(cv, n, sizeof(RGB *), __color_rgbcmp);
}

int color_midval(IMAGE *img, char orgb)
{
	double midv;
	return (image_statistics(img, orgb, &midv, NULL) == RET_OK)? (int)midv : - 1;
}

VECTOR *color_vector(IMAGE *img, RECT *rect, int ndim)
{
	IMAGE *sub = NULL;
	int i, *count = NULL;
	VECTOR *vec = vector_create(ndim); CHECK_VECTOR(vec);

	if (rect) {
		sub = image_subimg(img, rect); CHECK_IMAGE(sub);
		count = color_count(sub, 1, 1, ndim);
		image_destroy(sub);
	}
	else {	// whole image
		count = color_count(img, 1, 1, ndim);
	}

	if (count) {
		for (i = 0; i < ndim; i++)
			vec->ve[i] = count[i];
		free(count);
	}
	vector_normal(vec);

	return vec;
}

double color_entropy(IMAGE *img, RECT *rect, int ndim)
{
	double d;
	VECTOR *vec = color_vector(img, rect, ndim);	
	if (! vector_valid(vec)) {
		syslog_error("Create vector.");
		return -1.0f;
	}
	d = vector_entropy(vec);
	vector_destroy(vec);

	return d;
}

double color_likeness(IMAGE *f, IMAGE *g, RECT *rect, int ndim)
{
	double avgd;
	VECTOR *fs,*gs;

	fs = color_vector(f, rect, ndim); check_vector(fs);
	gs = color_vector(g, rect, ndim); check_vector(gs);

	avgd = vector_likeness(fs, gs);
	
	vector_destroy(fs);
	vector_destroy(gs);
	
	return avgd;
}


int skin_detect(IMAGE *img)
{
	int i, j;
	check_image(img);
	image_foreach(img, i, j) {
		if (! color_beskin(img->ie[i][j].r, img->ie[i][j].g, img->ie[i][j].b)) {
			img->ie[i][j].r = 0;
			img->ie[i][j].g = 0;
			img->ie[i][j].b = 0;
		}
	}
	return RET_OK;
}

int color_cluster(IMAGE *image, int num, int update)
{
	int i, j, k, c;
	MATRIX *mat, *ccmat;
	int classno[0xffff + 1];

	check_image(image);

	mat = image_classmat(image); check_matrix(mat);

	// Weight k-means
	ccmat = matrix_wkmeans(mat, num, NULL); check_matrix(ccmat);

	// Back project !!!
	memset(classno, 0, ARRAY_SIZE(classno) * sizeof(int));
	for (i = 0; i < mat->m; i++) {
		k = RGB565_NO((int)mat->me[i][0], (int)mat->me[i][1], (int)mat->me[i][2]);
		classno[k] = (int)mat->me[i][4]; // weight -- class
	}
	image_foreach(image,  i, j) {
		k = RGB565_NO(image->ie[i][j].r, image->ie[i][j].g, image->ie[i][j].b);
		c = classno[k];
		
		if (update) {
			image->ie[i][j].r = (BYTE)ccmat->me[c][0];
			image->ie[i][j].g = (BYTE)ccmat->me[c][1];
			image->ie[i][j].b = (BYTE)ccmat->me[c][2];
		}
		image->ie[i][j].d = (BYTE)ccmat->me[c][5];	// Depths, Layers
	}

	matrix_destroy(ccmat);
	matrix_destroy(mat);

	return RET_OK;
}

int *color_count(IMAGE *image, int rows, int cols, int levs)
{
	int *count;
	int i, j, i2, j2, k2, bh, bw; // bh -- block height, bw -- block width

	if (levs < 1 || rows < 1 || cols < 1) {
		syslog_error("rows(%d), cols(%d), levs(%d) must be greater 1", rows, cols, levs);
		return NULL;
	}

	count = (int *)calloc((size_t)rows*cols*levs, sizeof(int));
	if (! count) { 
		syslog_error("Allocate memeory.");
		return NULL; 
	}

	color_cluster(image, levs, 0);

	// Statistics !!!
	bh = (image->height + rows - 1)/rows;
	bw = (image->width + cols - 1)/cols;
	for (i = 0; i < image->height; i++) {
		i2 = (i/bh);
		for (j = 0; j < image->width; j++) {
			j2 = (j/bw);
			k2 = image->ie[i][j].d;
			count[VOICE_CELL_OFFSET(i2, j2, k2)]++;
		}
	}

	return count;
}


int color_picker()
{
	static int color_set[7] = {0xff0000, 0x00ff00, 0x0000ff, 0x00ffff, 0xff00ff, 0xffff00, 0xffffff};
	return color_set[random() % ARRAY_SIZE(color_set)];
}


int skin_statics(IMAGE *img, RECT *rect)
{
	int i, j, sum;
	check_image(img);

	sum = 0;
	for (i = rect->r; i < rect->r + rect->h; i++) {
		for (j = rect->c; j < rect->c + rect->w; j++) {
			if (color_beskin(img->ie[i][j].r, img->ie[i][j].g, img->ie[i][j].b))
				sum++;
		}
	}
	return sum;
}

// fast to luv
Luv *color_rgbf2luv(BYTE R, BYTE G, BYTE B)
{
	static int f2init = 0;
	static Luv Luvtable[0xffff + 1];

	int k;
	BYTE r, g, b;
	double L, u, v;

	if (f2init == 0) {
		for (k = 0; k < 0xffff + 1; k++) {
			r = RGB565_R(k);
			g = RGB565_G(k);
			b = RGB565_B(k);
			color_rgb2luv(r, g, b, &L, &u, &v);
			Luvtable[k].L = L;
			Luvtable[k].u = u;
			Luvtable[k].v = v;
		}
		f2init = 1;
	}
	
	k = RGB565_NO(R, G, B);
	return &Luvtable[k];
}

// GWM -- Gray World Method
int color_rect_gwmgain(IMAGE *img, RECT *rect, double *r_gain, double *g_gain, double *b_gain)
{
	int i, j;
	double r_avg, g_avg, b_avg, rgb_avg, d;

	image_rectclamp(img, rect);

	if (rect->h < 1 || rect->w)
		return RET_ERROR;

	r_avg = g_avg = b_avg = 0.0f;
	rect_foreach(rect,i,j) {
		r_avg += img->ie[i + rect->r][j + rect->c].r;
		g_avg += img->ie[i + rect->r][j + rect->c].g;
		b_avg += img->ie[i + rect->r][j + rect->c].b;
	}

	d = 1.0f/rect->h/rect->w;
	r_avg *= d; 	g_avg *= d; 	b_avg *= d;
	
	rgb_avg = 127.0f; // (r_avg + g_avg + b_avg)/3.0f;	// 127.0f
	
	*r_gain = *g_gain = *b_gain = 1.0f;
	if (r_avg > MIN_DOUBLE_NUMBER)
		*r_gain = rgb_avg/r_avg;
	if (g_avg > MIN_DOUBLE_NUMBER)
		*g_gain = rgb_avg/g_avg;
	if (b_avg > MIN_DOUBLE_NUMBER)
		*b_gain = rgb_avg/b_avg;

	return RET_OK;
}

int color_gwmgain(IMAGE *img, double *r_gain, double *g_gain, double *b_gain)
{
	RECT rect;
	
	image_rect(&rect,img);
	return color_rect_gwmgain(img, &rect, r_gain, g_gain, b_gain);
}

int color_balance(IMAGE *img, int method, int debug)
{
	double r_gain, g_gain, b_gain;
	
	if (method == COLOR_BALANCE_GRAY_WORLD) {
		color_gwmgain(img, &r_gain, &g_gain, &b_gain);
		if (debug)
			printf("GWM Gain: r = %lf, g = %lf, b = %lf\n", r_gain, g_gain, b_gain);
	}
	else {
		color_prmgain(img, &r_gain, &g_gain, &b_gain);
		if (debug)
			printf("PRM Gain: r = %lf, g = %lf, b = %lf\n", r_gain, g_gain, b_gain);
	}

	color_correct(img, r_gain, g_gain, b_gain);
	
	return RET_OK;
}

// PRM -- Perfect Reflector Method
int color_rect_prmgain(IMAGE *img, RECT *rect, double *r_gain, double *g_gain, double *b_gain)
{
	int i, j, k, threshold;
	double ratio, sum, r_max, g_max, b_max;
	int count[255 * 3 + 1];

	check_image(img);
	image_rectclamp(img, rect);
	
	memset(count, 0, ARRAY_SIZE(count)*sizeof(int));
	rect_foreach(rect,i,j) {
		count[img->ie[i + rect->r][j + rect->c].r + img->ie[i + rect->r][j + rect->c].g + img->ie[i + rect->r][j + rect->c].b]++;
	}

	sum = 0.0f;
	ratio = 0.05f * img->height * img->width;
	threshold = 0;
	for (k = ARRAY_SIZE(count) - 1; k >= 0; k--) {
		sum += count[k];
		if (sum >= ratio) {
			threshold = k;
			break;
		}
	}

	// Count R+G+B >= threshold as white points
	r_max = g_max = b_max = sum = 0.0f;
	rect_foreach(rect,i,j) {
		if (img->ie[i + rect->r][j + rect->c].r + img->ie[i + rect->r][j + rect->c].g + img->ie[i + rect->r][j + rect->c].b >= threshold) {
			sum += 1.0f;
			r_max += img->ie[i + rect->r][j + rect->c].r;
			g_max += img->ie[i + rect->r][j + rect->c].g;
			b_max += img->ie[i + rect->r][j + rect->c].b;
		}
	}
		
	if (sum > 0.0f) {
		r_max = r_max/sum;
		g_max = g_max/sum;
		b_max = b_max/sum;
	}

	*r_gain = (r_max < 1)?1.0f : 255.0f/r_max;
	*g_gain = (g_max < 1)?1.0f : 255.0f/g_max;
	*b_gain = (b_max < 1)?1.0f : 255.0f/b_max;	

	return RET_OK;
}

int color_prmgain(IMAGE *img, double *r_gain, double *g_gain, double *b_gain)
{
	RECT rect;
	
	image_rect(&rect,img);
	return color_rect_prmgain(img, &rect, r_gain, g_gain, b_gain);
}

// Von Kries diagonal model
int color_correct(IMAGE *img, double gain_r, double gain_g, double gain_b)
{
	int i, j;
	double d;

	check_image(img);
	
	image_foreach(img,i,j) {
		d = gain_r * img->ie[i][j].r;	// xxxx666 --> Fast ...
		img->ie[i][j].r = (BYTE)MIN(d, 255);

		d = gain_g * img->ie[i][j].g;
		img->ie[i][j].g = (BYTE)MIN(d, 255);

		d = gain_b * img->ie[i][j].b;
		img->ie[i][j].b = (BYTE)MIN(d, 255);
	}

	return RET_OK;	
}

int color_togray(IMAGE *img)
{
	BYTE n;
	int i, j;

	check_image(img);
	image_foreach(img,i,j) {
		color_rgb2gray(img->ie[i][j].r, img->ie[i][j].g, img->ie[i][j].b, &n);
		img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = n;
	}
	return RET_OK;
}

int color_torgb565(IMAGE *img)
{
	int i, j;

	check_image(img);
	
	image_foreach(img,i,j) {
		img->ie[i][j].r = (img->ie[i][j].r >> 3) << 3;	// 5
		img->ie[i][j].g = (img->ie[i][j].g >> 2) << 2;	// 6
		img->ie[i][j].b = (img->ie[i][j].b >> 3) << 3;	// 5
	}

	return RET_OK;
}

// Lab distance
double color_distance(RGB *c1, RGB *c2)
{
	static int first = 1;
	static double L[0xffff + 1], a[0xffff + 1], b[0xffff + 1];

	int i, j, k;
	int n1, n2;
	double dL, da, db;

	if (first) {
		for (i = 0; i <= 0xff; i++) {
			for (j = 0; j <= 0xff; j++) {
				for (k = 0; k <= 0xff; k++) {
					color_rgb2lab(i, j, k, &dL, &da, &db);
					n1 = RGB565_NO(i, j, k);
					L[n1] = dL; a[n1] = da; b[n1] = db;
				}
				
			}
		}
		first = 0;
	}

	n1 = RGB565_NO(c1->r, c1->g, c1->b);
	n2 = RGB565_NO(c2->r, c2->g, c2->b);
	dL = L[n1] - L[n2]; dL *= dL;
	da = a[n1] - a[n2]; da *= da;
	db = b[n1] - b[n2]; db *= db;

	return sqrt(dL + da + db);
}
