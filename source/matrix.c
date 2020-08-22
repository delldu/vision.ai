
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "matrix.h"
#include "image.h"
#include "filter.h"

#define MATRIX_MAGIC MAKE_FOURCC('M','A','T','R') 


#define MATRIX_HEAD_PATTERN "matrix[ \t]*:"
#define EMPTY_LINE_PATTERN "^[ \t\r\n]*$"

#define MATRIX_MORE_ROWS 16

static int __matrix_qsort_column = 0;

static int __mhead_size()
{
	return (sizeof(MATRIX) - sizeof(double **) - sizeof(double *));
}

static MATRIX *__load_fromtxt(char *fname)
{
	int i, j, n, mat_m, mat_n, mat_found = 0;
	char *tv[256], buf[4096];
	MATRIX *mat = NULL;

	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		syslog_error("open file %s.", fname);
		return NULL;
	}
	
	// Search matrix head
	while(fgets(buf, sizeof(buf), fp)) {
		n = buf_scanf(buf, MATRIX_HEAD_PATTERN);
		if (n > 0) {
			n = get_token(buf + n, 'x', 256, tv);
			if (n == 2 && (mat_m = atoi(tv[0])) > 0 && (mat_n = atoi(tv[1])) > 0) {
				mat_found = 1;
				break;
			}
			else
				syslog_error("Bad matrix head item in file.");
		}
	}
	if (! mat_found) {
		fclose(fp);
		return NULL;
	}
	
	mat = matrix_create(mat_m, mat_n);
	if (! matrix_valid(mat)) {
		syslog_error("Create matrix.");
		fclose(fp);
		return NULL;
	}

	// data process
	i = 0;
	while(i < mat_m && fgets(buf, sizeof(buf), fp)) {
		if (buf_scanf(buf, EMPTY_LINE_PATTERN) > 0) // skip empty line
			continue;
		
		n = get_token(buf, ',', 256, tv);
		for (j = 0; i < mat_m && j < n && j < mat_n; j++)
			mat->me[i][j] = atof(tv[j]);

		// next row
		++i;
	}
	fclose(fp);
	
	return mat;
}

static MATRIX *__load_fromraw(char *fname)
{
	int i, j, n, mat_m, mat_n;
	char *tv[256], buf[4096];
	MATRIX *mat = NULL;

	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		syslog_error("Open file %s.", fname);
		return NULL;
	}

	// get dimensions mat_m x mat_n
	mat_m = mat_n = 0;
	while(fgets(buf, sizeof(buf), fp)) {
		if (buf_scanf(buf, EMPTY_LINE_PATTERN) > 0) // skip empty line
			continue;

		mat_m++;
		n = get_token(buf, ',', 256, tv);
		if (n > mat_n)
			mat_n = n;
	}

	if (mat_m < 1 || mat_n < 1) {
		syslog_error("Empty file %s.", fname);
		fclose(fp);
		return NULL;
	}

	mat = matrix_create(mat_m, mat_n);
	if (! matrix_valid(mat)) {
		syslog_error("Create matrix.");
		fclose(fp);
		return NULL;
	}

	fseek(fp, 0L, SEEK_SET);		// rewind

	// data process
	i = 0;
	while(i < mat_m && fgets(buf, sizeof(buf), fp)) {
		if (buf_scanf(buf, EMPTY_LINE_PATTERN) > 0) // skip empty line
			continue;
		
		n = get_token(buf, ',', 256, tv);
		for (j = 0; i < mat_m && j < n; j++) {
			mat->me[i][j] = atof(tv[j]);
		}
		// next row
		++i;
	}
	fclose(fp);
	
	return mat;
}

static MATRIX *__load_frombin(char *fname)
{
	int ret;
	MATRIX head;
	MATRIX *mat = NULL;
	
	FILE *fp = fopen(fname, "rb");
	if (fp == NULL) {
		syslog_error("Open file %s.", fname);
		return NULL;
	}

	// Is it matrix file ?
	ret = fread(&(head), __mhead_size(), 1, fp);
	if (ret <=0 || head.m < 1 || head.n < 1 || head.magic != MATRIX_MAGIC) {
		syslog_error("%s is NOT matrix file\n", fname);
		goto quit;
	}
	
	mat = matrix_create(head.m, head.n);
	if (! matrix_valid(mat)) {
		syslog_error("Create matrix.");
		goto quit;
	}
	ret = fread(mat->base, head.m * head.n * sizeof(double), 1, fp);
	fclose(fp);	

	// assert(ret >= 0);

	return mat;
	
quit:
	fclose(fp);
	return NULL;
}

static int __save_totxt(MATRIX *M, char *fname)
{
	int i, j;
	FILE *fp;

	check_matrix(M);

	if ((fp = fopen(fname, "w")) == NULL) {
		syslog_error("Create file %s.", fname);
		return RET_ERROR;
	}

	// write down matrix head
	fprintf(fp, "matrix: %dx%d\n", M->m, M->n);

	// write down data
	matrix_foreach(M, i, j) {
		fprintf(fp, "%f", M->me[i][j]);
		fprintf(fp, "%s", (j < (int)M->n - 1)?"," : "\n");
	}
	fclose(fp);

	return RET_OK;
}

static int __save_tobin(MATRIX *M, char *fname)
{
	FILE *fp;

	check_matrix(M);

	if ((fp = fopen(fname, "w")) == NULL) {
		syslog_error("Create file %s.", fname);
		return RET_ERROR;
	}

	// write down matrix head
	fwrite(M, __mhead_size(), 1, fp);	// Save head
	// write down data
	fwrite(M->base, M->m * M->n * sizeof(double), 1, fp);	// Save head

	fclose(fp);	

	return RET_OK;
}

// Euclidean Space square !!!
static double __euc_distance2(double *a, double *b, int n)
{
	double d = 0.0f;
	(void)n;
#if 0
	int i;
	for (i = 0; i < n; i++)
		d += (a[i] - b[i]) * (a[i] - b[i]);
#else	// Fast version
	double d0, d1, d2;
	d0 = (a[0] - b[0]);  d0 *= d0;
	d1 = (a[1] - b[1]);  d1 *= d1;
	d2 = (a[2] - b[2]);  d2 *= d2;
	d = d0 + d1 + d2;
#endif



	return d; // sqrt(d);	
}

static double __temp_xmax(MATRIX *mat, int r, int c, double x, MATRIX *temp)
{
	int i, j;

	double d, max = mat->me[r][c] + x * temp->me[temp->m/2][temp->n/2];
	
	matrix_foreach(temp, i, j) { // left corner (r - temp->m/2, c - temp->n/2)
		if (matrix_outdoor(mat, r - temp->m/2 + i, 0, c - temp->n/2 + j, 0)) 
			continue;
		if (ABS(temp->me[i][j]) < MIN_DOUBLE_NUMBER)
			continue;

		d = mat->me[r - temp->m/2 + i][c - temp->n/2 + j] + x * temp->me[i][j];
		if (d > max)
			max = d;
	}
	max = MIN(max, 255);
	
	return max;
}

static double __temp_xmin(MATRIX *mat, int r, int c, double x, MATRIX *temp)
{
	int i, j;
	double d, min = mat->me[r][c] + x * temp->me[temp->m/2][temp->n/2];
	
	matrix_foreach(temp, i, j) {  // left corner (r - temp->m/2, c - temp->n/2)
		if (matrix_outdoor(mat, r - temp->m/2 + i, 0, c - temp->n/2 + j, 0)) 
			continue;
		if (ABS(temp->me[i][j]) < MIN_DOUBLE_NUMBER)
			continue;

		d = mat->me[r - temp->m/2 + i][c - temp->n/2 + j] + x * temp->me[i][j];
		if (d < min)
			min = d;
	}
	min = MAX(0, min);
	
	return min;
}

static int __matrix_resize(MATRIX *matrix, int newrows)
{
	int i, j, oldrow;
	
	check_matrix(matrix);
	
	oldrow = matrix->m;
	if (matrix->_m >= newrows) {
			matrix->m = newrows;
			return RET_OK;
	}
	
	newrows += MATRIX_MORE_ROWS;	// Allocate more
	matrix->base = (double *)realloc(matrix->base, (newrows * matrix->n) * sizeof(double));
	if (! matrix->base) {
			syslog_error("Re-Allocate memeory.");
			return RET_ERROR;
	}
	matrix->me = (double **)realloc(matrix->me, newrows * sizeof(double *));
	if (! matrix->me) {
			syslog_error("Re-Allocate memeory.");
			return RET_ERROR;
	}
	for (i = 0; i < newrows; i++)
			matrix->me[i] = &(matrix->base[i * matrix->n]);
	
	// Init new cells
	for (i = oldrow; i < newrows; i++)
			for (j = 0; j < matrix->n; j++)
					matrix->me[i][j] = 0.0f;
	
	matrix->_m = newrows;
	matrix->m = (newrows - MATRIX_MORE_ROWS);		// Refer previously allocate more
	
	return RET_OK;
}

static int __cmp_1col(const void *p1, const void *p2)
{
        double *d1 = (double *)p1;
        double *d2 = (double *)p2;

        d1 += __matrix_qsort_column;
        d2 += __matrix_qsort_column;

        if (ABS(*d1 - *d2) < MIN_DOUBLE_NUMBER)
                return 0;

        return (*d1 < *d2)? -1 : 1;
}

static int __dcmp_1col(const void *p1, const void *p2)
{
        return -(__cmp_1col(p1, p2));
}


static int __matrix_8conn(MATRIX *mat, int r, int c)
{
	int k, sum = 0;
	int nb[8][2] = { { 0, 1 }, {-1, 1}, { -1, 0 }, {-1, -1}, { 0, -1}, {1, -1}, { 1,  0 }, {1, 1}};

	for (k = 0; k < 8; k++)
		sum += ABS(mat->me[r + nb[k][0]][c + nb[k][1]]) > MIN_DOUBLE_NUMBER? 0 : 1;


	return sum;
}

MATRIX *matrix_load(char *fname)
{
	char *extname = strrchr(fname, '.');
	if (extname) {
		if (strcasecmp(extname, ".txt") == 0)
			return __load_fromtxt(fname);
		else if (strcasecmp(extname, ".mat") == 0)
			return __load_frombin(fname);
		else if (strcasecmp(extname, ".raw") == 0)
			return __load_fromraw(fname);
	}

	syslog_error("Matrix load only support *.txt/*.mat/*.raw file.");
	return NULL;	
}

// Lua interface
int matrix_memsize(DWORD m, DWORD n)
{
	int size;
	
	size = sizeof(MATRIX); 
	size += m * n * sizeof(double);	// Data
	size += m * sizeof(double *);	// me
	return size;
}

// Lua interface
void matrix_membind(MATRIX *mat, DWORD m, DWORD n)
{
	DWORD i;
	char *base = (char *)mat;
	
	mat->magic = MATRIX_MAGIC;
	mat->m = m; mat->n = n; mat->_m = m;
	
	mat->base = (double *)(base + sizeof(MATRIX));	// Data
	mat->me = (double **)(base + sizeof(MATRIX) + (m * n) * sizeof(double));	// Skip head and data
	for (i = 0; i < m; i++) 
		mat->me[i] = &(mat->base[i * n]); 
}

MATRIX *matrix_create(int m, int n)
{
	int i;
	MATRIX *matrix;
	
	if (m < 1 || n < 1) {
			syslog_error("Create matrix.");
			return NULL;
	}
	matrix = (MATRIX *)calloc((size_t)1, sizeof(MATRIX));
	if (! matrix) {
			syslog_error("Allocate memeory.");
			return NULL;
	}
	matrix->magic = MATRIX_MAGIC;
	matrix->m = m; matrix->n = n; matrix->_m = m;

	matrix->base = (double *)calloc((size_t)(m * n), sizeof(double));
	if (! matrix->base) {
			syslog_error("Allocate memeory.");
			free(matrix); return NULL;
	}
	matrix->me = (double **)calloc(m, sizeof(double *));
	if (! matrix->me) {
			syslog_error("Allocate memeory.");
			free(matrix->base);
			free(matrix);
			return NULL;
	}
	for (i = 0; i < m; i++)
			matrix->me[i] = &(matrix->base[i * n]);

	return matrix;
}

int matrix_clear(MATRIX *mat)
{
	check_matrix(mat);
	memset((BYTE *)mat->base, 0, mat->m * mat->n * sizeof(double));
	return RET_OK;
}

int matrix_save(MATRIX *mat, char *fname)
{
	char *extname = strrchr(fname, '.');

	if (extname) {
		if (strcasecmp(extname, ".txt") == 0)
			return __save_totxt(mat, fname);
		if (strcasecmp(extname, ".mat") == 0)
			return __save_tobin(mat, fname);
 	}

	syslog_error("Matrix save only support *.txt/*.mat file.");
	return RET_ERROR;
}

void matrix_destroy(MATRIX *m)
{
	if (! matrix_valid(m)) {
			return;
	}
	free(m->me);
	free(m->base);
	free(m);
}

void matrix_print(MATRIX *m, char *format)
{
	int i, j, k;
	if (! matrix_valid(m)) {
		syslog_error("Bad matrix.");
		return;
	}

	// write data
	matrix_foreach(m, i, j) {
		if (strchr(format, 'f'))	// double
			printf(format, m->me[i][j]);
		else {
			k = m->me[i][j];
			printf(format, k);	// integer
		}
		printf("%s", (j < m->n - 1)?", " : "\n");
	}
}

int matrix_valid(MATRIX *M)
{
	return (M && M->m >= 0 && M->n >= 1 && M->me && M->magic == MATRIX_MAGIC);
}

MATRIX *matrix_copy(MATRIX *src)
{
	MATRIX *copy;

	CHECK_MATRIX(src);
	copy = matrix_create(src->m, src->n); CHECK_MATRIX(copy);
	memcpy((void *)copy->base, (void *)(src->base), src->m * src->n * sizeof(double));
	
	return copy;
}

// NO Lua interface
MATRIX *matrix_zoom(MATRIX *mat, int nm, int nn, int method)
{
	int i, j, i2, j2;
	double di, dj, d1, d2, d3, d4, u, v, d;
	MATRIX *copy;

	CHECK_MATRIX(mat);
	if (mat->m == nm && mat->n == nn)
		return matrix_copy(mat);

	// size changed
	copy = matrix_create(nm, nn); CHECK_MATRIX(copy);
	di = 1.0 * mat->m/copy->m;
	dj = 1.0 * mat->n/copy->n;

	if (method == ZOOM_METHOD_BLINE) {
		/**********************************************************************
		d1    d2
		    (p)
		d3    d4
		f(i+u,j+v) = (1-u)(1-v)f(i,j) + (1-u)vf(i,j+1) + u(1-v)f(i+1,j) + uvf(i+1,j+1)
		**********************************************************************/
		matrix_foreach(copy, i, j) {
			i2 = (int)(di * i);
			u = di * i - i2;
			j2 = (int)(dj * j);
			v = dj *j - j2;
			if (i2 == (int)mat->m - 1 || j2 == mat->n - 1) {
				copy->me[i][j] = mat->me[i2][j2];
			}
			else {
				d1 = mat->me[i2][j2];
				d2 = mat->me[i2][j2 + 1];
				d3 = mat->me[i2 + 1][j2];
				d4 = mat->me[i2 + 1][j2 + 1];
				d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
				copy->me[i][j] = (int)d;
			}
		}
	}
	else {
		matrix_foreach(copy, i, j) {
			i2 = (int)(di * i);
			j2 = (int)(dj * j);
			copy->me[i][j] = mat->me[i2][j2];
		}
	}
	
	return copy;
}

int matrix_pattern(MATRIX *M, char *name)
{
	int i, j;
	double d;
	
	check_matrix(M);

	if (strcmp(name, "random") == 0 || strcmp(name, "rand") == 0) {
		srandom((unsigned int)time(NULL));
		matrix_foreach(M, i, j) {
			d = random()%10000;
			d /= 10000.0f;
			M->me[i][j] = d;
	 	}
	}
	else if (strcmp(name, "dialog") == 0) {
		matrix_foreach(M, i, j) {
			if (i == j)
				M->me[i][j] = (i + 1.0f);
		}
	}
	else if (strcmp(name, "eye") == 0) {
		matrix_foreach(M, i, j) {
			if (i == j)
				M->me[i][j] = 1.0f;
		}
	}
	else if (strcmp(name, "one") == 0) {
		matrix_foreach(M, i, j)
			M->me[i][j] = 1.0f;
	}
	else if (strcmp(name, "zero") == 0) {
		memset((BYTE *)M->base, 0, M->m * M->n * sizeof(double));
	}
	else if (strcmp(name, "3x3disc") == 0) {
		matrix_foreach(M, i, j)
			M->me[i][j] = 1.0f;
		M->me[0][0] = M->me[0][2] = M->me[2][0] = M->me[2][2] = 0.0;
 	}

	return RET_OK;
}

int matrix_setvalue(MATRIX *mat, int row, int col, double val)
{
	check_matrix(mat);
	
	if (col >= mat->n) {
		syslog_error("Bad row/col.");
		return RET_ERROR;
	}

	if (row >= mat->m && __matrix_resize(mat, row + 1) != RET_OK) {
		syslog_error("Bad row.");
		return RET_ERROR;
	}
	mat->me[row][col] = val;
	return RET_OK;
}


// (i + di, j + dj) will move to outside ?
int matrix_outdoor(MATRIX *M, int i, int di, int j, int dj)
{
	return (i + di < 0 || i + di >= M->m || j + dj < 0 || j + dj >= M->n);
}


MATRIX *matrix_transpose(MATRIX *matrix)
{
        int i, j;
        MATRIX *transmat = NULL;

        if (! matrix_valid(matrix)) {
                syslog_error("Bad matrix.");
                return NULL;
        }

        transmat = matrix_create(matrix->n, matrix->m);
        if (matrix_valid(transmat)) {
                matrix_foreach(transmat, i, j)
                        transmat->me[i][j] = matrix->me[j][i];
        }

        return transmat;
}

int matrix_integrate(MATRIX *mat)
{
	int i, j;
	
	check_matrix(mat);

	for (i = 0; i < mat->m; i++) {
		for (j = 1; j < mat->n; j++)
			mat->me[i][j] += mat->me[i][j - 1];
	}

	for (j = 0; j < mat->n; j++) {
		for (i = 1; i < mat->m; i++)
			mat->me[i][j] += mat->me[i - 1][j];
	}

	return RET_OK;
}


//     1   2 
//     3   4
//  delta = (1+4) - (2+3)
//
// return [r1, c1, r2, c2]
double matrix_difference(MATRIX *mat, int r1, int c1, int r2, int c2)
{
	double d1, d2, d3, d4;
	r2 = MIN(r2, mat->m - 1);
	c2 = MIN(c2, mat->n - 1);

	d1 = (r1 <= 0 || c1 <= 0)? 0.0f : mat->me[r1 - 1][c1 - 1];
	d2 = (r1 <= 0)? 0.0f : mat->me[r1 - 1][c2];
	d3 = (c1 <= 0)? 0.0f : mat->me[r2][c1 - 1];
	d4 = mat->me[r2][c2];

 	return d1 + d4 - (d2 + d3);
}

int matrix_weight(MATRIX *mat, RECT *rect)
{
	return (int)matrix_difference(mat, rect->r, rect->c, rect->r + rect->h, rect->c + rect->w);
}


// NO Lua interface
int matrix_normal(MATRIX *mat)
{
	int i, j;
	double sum;
	check_matrix(mat);

	sum = 0.0f;
	matrix_foreach(mat, i, j)
		sum += mat->me[i][j];

	if (ABS(sum) > MIN_DOUBLE_NUMBER) {
		matrix_foreach(mat, i, j)
			mat->me[i][j] /= sum;
	}

	return RET_OK;;
}

// ????
MATRIX *matrix_gskernel(double sigma)
{
	int i, j, dim;
	double d, g;
	MATRIX *mat;

	dim = math_gsbw(sigma)/2;
	mat = matrix_create(2 * dim + 1, 2 * dim + 1); CHECK_MATRIX(mat);
	d = sigma * sigma * 2.0f; 
	for (i = 0; i <= dim; i++) {
		for (j = 0; j <= dim; j++) {
			g = exp(-(1.0 * i * i + 1.0 * j *j)/d);
			mat->me[dim + i][dim + j] = g;
			mat->me[dim - i][dim - j] = g;
			mat->me[dim + i][dim + j] = g;
			mat->me[dim - i][dim - j] = g;
		}
	}
	matrix_normal(mat);
	
	return mat;
}

// NO Lua interface
int matrix_tempxmin(MATRIX *mat, double x, MATRIX *temp)
{
	int i, j;
	MATRIX *copy;

	check_matrix(mat);
	check_matrix(temp);
	copy = matrix_copy(mat); check_matrix(copy);

	matrix_foreach(mat, i, j)
		mat->me[i][j] = __temp_xmin(copy, i, j, x, temp);
	
	matrix_destroy(copy);

	return RET_OK;
}

// NO Lua interface
int matrix_tempxmax(MATRIX *mat, double x, MATRIX *temp)
{
	int i, j;
	MATRIX *copy;

	check_matrix(mat);
	check_matrix(temp);
	copy = matrix_copy(mat); check_matrix(copy);

	matrix_foreach(mat, i, j)
		mat->me[i][j] = __temp_xmax(copy, i, j, x, temp);
	
	matrix_destroy(copy);

	return RET_OK;
}

// NO Lua interface
int matrix_morph(char action, MATRIX *mat, MATRIX *temp)
{
	int i, j;
	MATRIX *copy = NULL;
	
	if (! strchr("EDOCBb", action)) {
		syslog_error("Bad action %c.", action);
		return RET_ERROR;
	}

	check_matrix(mat);
	check_matrix(temp);

	if (action == 'B' || action == 'b') {
		copy = matrix_copy(mat);check_matrix(copy);
	}

	switch(action) {
	case 'E': // erode
		matrix_tempxmin(mat, -1, temp); 
		break;
	case 'D': // dilate
		matrix_tempxmax(mat, +1, temp);  
		break;
	case 'O': // open
		matrix_tempxmin(mat, -1, temp);
		matrix_tempxmax(mat, +1, temp);
		break;
	case 'C': // close
		matrix_tempxmax(mat, +1, temp);
		matrix_tempxmin(mat, -1, temp);
		break;
	case 'B': // out border
		matrix_tempxmax(mat, +1, temp);
		matrix_foreach(mat, i, j)
			mat->me[i][j] -= copy->me[i][j];

		break;
	case 'b': // inner border
		matrix_tempxmin(mat, -1, temp);
		matrix_foreach(mat, i, j)
			mat->me[i][j] = copy->me[i][j] - mat->me[i][j];
		break;
	default:
		break;
	}
	
	if (action == 'B' || action == 'b')
		matrix_destroy(copy);
	
	return RET_OK;
}


// NO Lua interface
int matrix_localmax(MATRIX *mat, int r, int c)
{
	int radius = 1;

	int i, j, k;
	RECT rect;

	k = (int)mat->me[r][c];
	if (k < 1)
		return 0;
	
	rect.r = r - radius; rect.c = c - radius; rect.h = 2*radius + 1;  rect.w = 2*radius + 1;
	rect.r = CLAMP(rect.r, 0, mat->m  - 1);
	rect.h = CLAMP(rect.h, 0, mat->m - rect.r);
	rect.c = CLAMP(rect.c, 0, mat->n - 1);
	rect.w = CLAMP(rect.w, 0, mat->n - rect.c);

	rect_foreach(&rect, i, j) {
		if ((int)mat->me[rect.r + i][rect.c + j] > k)
			return 0;
	}

	return 1;
}


// NO Lua interface
/*********************************************************************
weight K Means Cluster
Suppose:
	1)  mat: Cluster orignal data matrix, format for RGB:
		r, g, b, w, classno
	2)  ccmat: Cluster center matrix, format for RGB:
		r, g, b, count, orig class, sorted class no
*********************************************************************/
MATRIX *matrix_wkmeans(MATRIX *mat, int k, distancef_t distance)
{
	// Colors == 3 (R, G, B)
#define MAT_COLORS 3
#define WEIGHT_INDEX 3
#define CLASS_INDEX 4
#define SORT_CLASS_INDEX 5

	int i, j, g, a, b, needcheck;
	double d, dt, w;
	MATRIX *ccmat;		// cluster center matrix
	
	CHECK_MATRIX(mat);

	if (distance == NULL)
		distance = __euc_distance2;

	if (mat->n != 5 || k < 1) {
		syslog_error("Matrix column %d != 5, class number %d < 1", mat->n, k);
		return NULL;
	}

	ccmat = matrix_create(k, SORT_CLASS_INDEX + 1); CHECK_MATRIX(ccmat);

	// Initialise Center
	for (i = 0; i < ccmat->m; i++) {
		ccmat->me[i][CLASS_INDEX] = i;			// orig class no
		ccmat->me[i][SORT_CLASS_INDEX] = i;			// sort class no
	}
	for (i = 0; i < mat->m; i++) {
		b = (i * k/mat->m) % k;
		mat->me[i][CLASS_INDEX] = b;
		w = mat->me[i][WEIGHT_INDEX];
		
		for (j = 0; j < MAT_COLORS; j++) 	// R, G, B
			ccmat->me[b][j] += w*mat->me[i][j];
		ccmat->me[b][WEIGHT_INDEX] += w;		// count class
	}

	// Average ccmat !!!
	for (i = 0; i < ccmat->m; i++) {
		if (ccmat->me[i][WEIGHT_INDEX] > MIN_DOUBLE_NUMBER) {
			for (j = 0; j < WEIGHT_INDEX; j++)	// R, G, B
				ccmat->me[i][j]  /= ccmat->me[i][WEIGHT_INDEX];
		}
	}

	int count = 0;
//	int start_time = time_ticks();
	do {
		count++;
//		CheckPoint("count = %d", count);
		needcheck = 0;
		for (i = 0; i < mat->m; i++) {
			a = (int)(mat->me[i][CLASS_INDEX]);	// old class !!!
			b = 0; d = distance(mat->me[i], ccmat->me[0], MAT_COLORS);
			for (j = 1; j < ccmat->m; j++) {
				dt = distance(mat->me[i], ccmat->me[j], MAT_COLORS);
				if (dt < d) {
					b = j; d = dt;
				}
			}
			if (b != a) {	// change mat->me[i] from a to b class
				// adjust a row
				w = mat->me[i][WEIGHT_INDEX];
				ccmat->me[a][WEIGHT_INDEX] -= w;
				if (ABS(ccmat->me[a][WEIGHT_INDEX]) > MIN_DOUBLE_NUMBER) {
					for (g = 0; g < MAT_COLORS; g++) {
						ccmat->me[a][g] *= (ccmat->me[a][WEIGHT_INDEX] + w);
						ccmat->me[a][g] -= w * mat->me[i][g];
						ccmat->me[a][g] /= ccmat->me[a][WEIGHT_INDEX];
					}
				}

				// adjust b row
				ccmat->me[b][WEIGHT_INDEX] += w;
				if (ABS(ccmat->me[b][WEIGHT_INDEX]) > MIN_DOUBLE_NUMBER) {
					for (g = 0; g < MAT_COLORS; g++) {
						ccmat->me[b][g] *= (ccmat->me[b][WEIGHT_INDEX] - w);
						ccmat->me[b][g] += w * mat->me[i][g];
						ccmat->me[b][g] /= ccmat->me[b][WEIGHT_INDEX];
					}
				}
				mat->me[i][CLASS_INDEX] = b;
				needcheck = 1;
			}
		}
	}while (needcheck);

	/*
	printf("Cluster Center: \n");
	matrix_print(ccmat, "%10.2f");
	*/
	matrix_sort(ccmat, WEIGHT_INDEX, 1);		// sorted by w, r, g, b, w, orig, sort
	for (i = 0; i < ccmat->m; i++)
		ccmat->me[i][SORT_CLASS_INDEX] = i;	// sorted class no

	matrix_sort(ccmat, CLASS_INDEX, 0);	// sorted by orig, r, g, b, w, orig, sort

	return ccmat;
}

int matrix_sort(MATRIX *A, int cols, int descend)
{
        check_matrix(A);

        if (cols < 0 || A->n < cols) {
                syslog_error("Bad matrix cols.");
                return RET_ERROR;
        }
        __matrix_qsort_column = cols;
        qsort(A->base, A->m, A->n * sizeof(double), descend? __dcmp_1col : __cmp_1col);

        return RET_OK;
}

MATRIX *matrix_pyrdown(MATRIX *mat)
{
	matrix_gauss_filter(mat, 0.5);
	return matrix_zoom(mat, (mat->m + 1)/2, (mat->n + 1)/2, ZOOM_METHOD_COPY); 
}

MATRIX *matrix_pyrup(MATRIX *mat)
{
	MATRIX *dst;
	
	dst = matrix_zoom(mat, 2*mat->m, 2*mat->n, ZOOM_METHOD_COPY); 
	CHECK_MATRIX(dst);
	matrix_gauss_filter(dst, 0.5);
	return dst;
}

// Cut isolated points
int matrix_clean(MATRIX *mat)
{
	int i, j;

	for(i = 1; i < mat->m - 1; i++) {
		for (j = 1; j < mat->n - 1; j++) {
			if (__matrix_8conn(mat, i, j) <= 1)
				mat->me[i][j] = 0;
		}
	}

	return RET_OK;
}


int matrix_add(MATRIX *A, MATRIX *B)
{
	int i, j;
	
	check_matrix(A);
	check_matrix(B);

	if (A->m != B->m || A->n != B->n) {
		syslog_error("A, B Size is not same.");
		return RET_OK;
	}

	for (i = 0; i < A->m; i++) {
		for (j = 0; j < A->n; j++)
			A->me[i][j] += B->me[i][j];
	}
	
	return RET_OK;
}

// Substract
int matrix_sub(MATRIX *A, MATRIX *B)
{
	int i, j;
	
	check_matrix(A);
	check_matrix(B);

	if (A->m != B->m || A->n != B->n) {
		syslog_error("A, B Size is not same.");
		return RET_OK;
	}

	for (i = 0; i < A->m; i++) {
		for (j = 0; j < A->n; j++)
			A->me[i][j] -= B->me[i][j];
	}
	
	return RET_OK;
}

// Dot divide A/B
int matrix_mul(MATRIX *A, MATRIX *B)
{
	int i, j;
	
	check_matrix(A);
	check_matrix(B);

	if (A->m != B->m || A->n != B->n) {
		syslog_error("A, B Size is not same.");
		return RET_OK;
	}

	for (i = 0; i < A->m; i++) {
		for (j = 0; j < A->n; j++)
			A->me[i][j] *= B->me[i][j];
	}
	
	return RET_OK;
}

// Inter divide A/B
int matrix_div(MATRIX *A, MATRIX *B)
{
	int i, j;
	
	check_matrix(A);
	check_matrix(B);

	if (A->m != B->m || A->n != B->n) {
		syslog_error("A, B Size is not same.");
		return RET_OK;
	}

	for (i = 0; i < A->m; i++) {
		for (j = 0; j < A->n; j++) {
			if (ABS(B->me[i][j]) > MIN_DOUBLE_NUMBER)
				A->me[i][j] /= B->me[i][j];
			else
				A->me[i][j] = 0.0f;
		}
	}
	
	return RET_OK;
}
