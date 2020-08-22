
/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include "image.h"
#include "color.h"
#include "filter.h"

#include <stdlib.h>
#include <errno.h>

// Support JPEG image
#define CONFIG_JPEG 1

#ifdef CONFIG_JPEG
#include <jpeglib.h>
#include <jerror.h>
#endif


#define IMAGE_MAGIC MAKE_FOURCC('I','M','A','G')

#define BITMAP_NO_COMPRESSION 0
#define BITMAP_WIN_OS2_OLD 12
#define BITMAP_WIN_NEW     40
#define BITMAP_OS2_NEW     64

#define FILE_END(fp) (ferror(fp) || feof(fp))


// voice = vector of image cube entroy !
#define CUBE_DEF_ROWS 16
#define CUBE_DEF_COLS 16
#define CUBE_DEF_LEVS 16

typedef struct {
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
} BITMAP_FILEHEADER;

typedef struct {
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAP_INFOHEADER;

typedef struct {
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
} BITMAP_RGBQUAD;



#define IMAGE_MAX_NB_SIZE 25
RGB *__image_rgb_nb[IMAGE_MAX_NB_SIZE];


extern int color_rgbcmp(RGB *c1, RGB *c2);
extern void color_rgbsort(int n, RGB *cv[]);

#define CONFIG_PNG 1

#ifdef CONFIG_JPEG
static void __jpeg_errexit (j_common_ptr cinfo)
{
	cinfo->err->output_message (cinfo);
	exit (EXIT_FAILURE);
}
#endif

static WORD __get_word(FILE *fp)
{
	WORD c0, c1;
	c0 = getc(fp);  c1 = getc(fp);
	return ((WORD) c0) + (((WORD) c1) << 8);
}

static DWORD __get_dword(FILE *fp)
{
	DWORD c0, c1, c2, c3;
	c0 = getc(fp);  c1 = getc(fp);  c2 = getc(fp);  c3 = getc(fp);
	return ((DWORD) c0) + (((DWORD) c1) << 8) + (((DWORD) c2) << 16) + (((WORD) c3) << 24);
}

static void __put_word(FILE *fp, WORD i)
{
	WORD c0, c1;
  	c0 = ((WORD) i) & 0xff;  c1 = (((WORD) i)>>8) & 0xff;
	putc(c0, fp);   putc(c1,fp);
}

static void __put_dword(FILE *fp, DWORD i)
{
	int c0, c1, c2, c3;
	c0 = ((DWORD) i) & 0xff;  
	c1 = (((DWORD) i)>>8) & 0xff;
	c2 = (((DWORD) i)>>16) & 0xff;
	c3 = (((DWORD) i)>>24) & 0xff;

	putc(c0, fp);   putc(c1, fp);  putc(c2, fp);  putc(c3, fp);
}

static char __get_pnmc(FILE *fp)
{
	char ch = getc(fp);
	if (ch == '#') {
		do {
			ch = getc(fp);
		} while (ch != '\n' && ch != '\r');
	}
	return ch;
}

static DWORD __get_pnmdword(FILE *fp)
{
	char ch;
	DWORD i = 0, pi;

	do {
		ch = __get_pnmc(fp);
	} while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

	if (ch < '0' || ch > '9')
		return 0;

	do {
		pi = ch - '0';
		i = i * 10 + pi;
		ch = __get_pnmc(fp);
	} while (ch >= '0' && ch <= '9');

	return i;
}


static int __nb3x3_map(IMAGE *img, int r, int c)
{
	int i, j, k;
	k = 0;
	for (i = -1; i <= 1; i++) {
		for (j = -1; j <= 1; j++) {
			__image_rgb_nb[k] = &(img->base[(r + i) * img->width + c + j]);
			++k;		
		}
	}
	color_rgbsort(3 * 3, __image_rgb_nb);
	return RET_OK;	
}

static void __draw_hline(IMAGE *img, int *x, int *y, int run_length, int x_advance, int r, int g, int b)
{
	int i;

	if (run_length < 0)
		run_length *= -1;

	for (i = 0; i < run_length; i++) {
		img->ie[*y][*x + (i * x_advance)].r = r;
		img->ie[*y][*x + (i * x_advance)].g = g;
		img->ie[*y][*x + (i * x_advance)].b = b;
	}

	*x += run_length * x_advance;
	*y += 1;
}

static void __draw_vline(IMAGE *img, int *x, int *y, int run_length, int x_advance, int r, int g, int b)
{
	int i;

	if (run_length < 0)
		run_length *= -1;

	for (i = 0; i < run_length; i++) {
		img->ie[*y + i][*x].r = r;
		img->ie[*y + i][*x].g = g;
		img->ie[*y + i][*x].b = b;
	}

	*x += x_advance;
	*y += run_length;
}

static int __color_rgbfind(RGB *c, int n, RGB *cv[])
{
	int i, k;

	k = color_rgbcmp(c, cv[n/2]);
	if (k == 0)
		return n/2;
	if (k < 0 ) {
		for (i = n/2 - 1; i >= 0; i--) {
			if (color_rgbcmp(c, cv[i]) == 0)
				return i;
		}
	}
	// k > 0
	for (i = n/2 + 1; i < n; i++) {
		if (color_rgbcmp(c, cv[i]) == 0)
			return i;
	}
	// not found

	return -1;
}

static void *__image_malloc(WORD h, WORD w)
{
	void *img = (IMAGE *)calloc((size_t)1, image_memsize(h, w));
	if (! img) { 
		syslog_error("Allocate memeory.");
		return NULL; 
	}
	return img;
}

extern int text_puts(IMAGE *image, int r, int c, char *text, int color);

int image_memsize(WORD h, WORD w)
{
	int size;
	
	size = sizeof(IMAGE); 
	size += (h * w) * sizeof(RGB);
	size += h * sizeof(RGB *);
	return size;
}

// mem align !
void image_membind(IMAGE *img, WORD h, WORD w)
{
	int i;
	void *base = (void *)img;
	
	img->magic = IMAGE_MAGIC;
	img->height = h;
	img->width = w;
	img->base = (RGB *)(base + sizeof(IMAGE));	// Data
	img->ie = (RGB **)(base + sizeof(IMAGE) + (h * w) * sizeof(RGB));	// Skip head and data
	for (i = 0; i < h; i++) 
		img->ie[i] = &(img->base[i * w]); 
}

IMAGE *image_create(WORD h, WORD w)
{
	void *base = __image_malloc(h, w);
	if (! base) { 
		return NULL; 
	}
	image_membind((IMAGE *)base, h, w);
	return (IMAGE *)base;
}

int image_clear(IMAGE *img)
{
	check_image(img);
	memset(img->base, 0, img->height * img->width * sizeof(RGB));
	return RET_OK;
}

BYTE image_getvalue(IMAGE *img, char oargb, int r, int c)
{
	BYTE n;
	switch(oargb) {
	case 'A':
		color_rgb2gray(img->ie[r][c].r, img->ie[r][c].g, img->ie[r][c].b, &n);
		return n;
		break;
	case 'R':
		return img->ie[r][c].r;
		break;
	case 'G':
		return img->ie[r][c].g;
		break;
	case 'B':
		return img->ie[r][c].b;
		break;
	case 'H':	// Hue !!!
		{
			BYTE s, v;
			color_rgb2hsv(img->ie[r][c].r, img->ie[r][c].g, img->ie[r][c].b, &n, &s, &v);
			return n;
		}
		break;
	}
	return 0;
}

void image_setvalue(IMAGE *img, char oargb, int r, int c, BYTE x)
{
	switch(oargb) {
	case 'A':
		img->ie[r][c].r = x;
		img->ie[r][c].g = x;
		img->ie[r][c].b = x;
		break;
	case 'R':
		img->ie[r][c].r = x;
		break;
	case 'G':
		img->ie[r][c].g = x;
		break;
	case 'B':
		img->ie[r][c].b = x;
		break;
	}
}

MATRIX *image_getplane(IMAGE *img, char oargb)
{
	BYTE n;
	int i, j;
	MATRIX *mat;

	CHECK_IMAGE(img);
	CHECK_ARGBH(oargb);
	mat = matrix_create(img->height, img->width);
	CHECK_MATRIX(mat);

	switch(oargb) {
	case 'A':
		image_foreach(img, i, j) {
			color_rgb2gray(img->ie[i][j].r, img->ie[i][j].g, img->ie[i][j].b, &n);
			mat->me[i][j] = (double)n;
		}
		break;
	case 'R':
		image_foreach(img, i, j)
			mat->me[i][j] = (double)img->ie[i][j].r;
		break;
	case 'G':
		image_foreach(img, i, j)
			mat->me[i][j] = (double)img->ie[i][j].g;
		break;
	case 'B':
		image_foreach(img, i, j)
			mat->me[i][j] = (double)img->ie[i][j].b;
		break;
	}

	return mat;
}

int image_setplane(IMAGE *img, char oargb, MATRIX *mat)
{
	int i, j;

	check_image(img);
	check_argb(oargb);
	check_matrix(mat);

	switch(oargb) {
	case 'A':
		for (i = 0; i < (int)img->height && i < (int)mat->m; i++) {
			for (j = 0; j < (int)img->width && j < (int)mat->n; j++) {
				img->ie[i][j].r = CLAMP(mat->me[i][j], 0, 255);
				img->ie[i][j].g = CLAMP(mat->me[i][j], 0, 255);
				img->ie[i][j].b = CLAMP(mat->me[i][j], 0, 255);
			}
		}
		break;
	case 'R':
		for (i = 0; i < img->height && i < mat->m; i++) {
			for (j = 0; j < img->width && j < mat->n; j++) {
				img->ie[i][j].r = CLAMP(mat->me[i][j], 0, 255);
			}
		}
		break;
	case 'G':
		for (i = 0; i < img->height && i < mat->m; i++) {
			for (j = 0; j < img->width && j < mat->n; j++) {
				img->ie[i][j].g = CLAMP(mat->me[i][j], 0, 255);
			}
		}
		break;
	case 'B':
		for (i = 0; i < img->height && i < mat->m; i++) {
			for (j = 0; j < img->width && j < mat->n; j++) {
				img->ie[i][j].b = CLAMP(mat->me[i][j], 0, 255);
			}
		}
		break;
	}

	return RET_OK;
}

int image_valid(IMAGE *img)
{
	return (! img || img->height < 1 || img->width < 1 || ! img->ie || img->magic != IMAGE_MAGIC) ? 0 : 1;
}

// (i + di, j + dj) will move to outside ?
int image_outdoor(IMAGE *img, int i, int di, int j, int dj)
{
	return (i + di < 0 || i + di >= img->height || j + dj < 0 || j + dj >= img->width);
}

int image_rectclamp(IMAGE *img, RECT *rect)
{
	if (! image_valid(img) || ! rect)
		return RET_ERROR;

	rect->r = CLAMP(rect->r, 0, img->height - 1);
	rect->h = CLAMP(rect->h, 0, img->height - rect->r);
	rect->c = CLAMP(rect->c, 0, img->width - 1);
	rect->w = CLAMP(rect->w, 0, img->width - rect->c);

	return RET_OK;
}

void image_destroy(IMAGE *img)
{
	if (! image_valid(img)) 
		return; 
	free(img);
}

// PBM/PGM/PPM ==> pnm format file
static IMAGE *image_loadpnm(char *fname)
{
	BYTE fmt, c1, c2;
	int i, j, r, g, b, height, width, bitshift, maxval = 255;
	FILE *fp;
 	IMAGE *img;
 
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		syslog_error("open file %s.", fname);
		return NULL;
	}

	// read header ?
	if (getc(fp) != 'P')
		goto bad_format;
	fmt = getc(fp);
	if (fmt < '1'  || fmt > '6')
		goto bad_format;

	width = __get_pnmdword(fp);
	height = __get_pnmdword(fp);
	if (width < 1 || height < 1)
		goto bad_format;
	if (fmt != '1' && fmt != '4')			// not pbm format
		maxval = __get_pnmdword(fp);

	img = image_create(height,  width);
	if (! img) {
		syslog_error("Create image.");
		// Allocate error !!!
		goto read_fail;
	}

	switch(fmt) {
	case '1':		// ASCII Portable bitmap
		image_foreach(img, i, j) {
			g = __get_pnmc(fp);
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = (g > 0)? 0 : 255;
		}
		img->format = IMAGE_BITMAP;
		break;
	case '2':		// ASCII Portable graymap
		image_foreach(img, i, j) {
			g = __get_pnmdword(fp);
			if (g > 255)
				g = g * 255/maxval;
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = (BYTE)g;
		}
		img->format = IMAGE_GRAY;
		break;
	case '3':		// ASCII Portable pixmap
		image_foreach(img, i, j) {
			r = __get_pnmdword(fp); if (r > 255) r = r*255/maxval;
			g = __get_pnmdword(fp); if (g > 255) g = g*255/maxval;
			b = __get_pnmdword(fp); if (b > 255) b = b*255/maxval;
			img->ie[i][j].r = (BYTE)r;
			img->ie[i][j].g = (BYTE)g;
			img->ie[i][j].b = (BYTE)b;
		}
		img->format = IMAGE_RGB;
		break;
	case '4':		// Binary Portable bitmap
		for (i = 0; i < img->height; i++) {
			bitshift = -1; 	// must be init per row !!!
			for (j = 0; j < img->width; j++) {
				if (bitshift == -1) {
					c1 = getc(fp);
					bitshift = 7;
				}
				g = ( c1 >> bitshift) & 1; g = (g == 0)? 255 : 0;
				img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = (BYTE)g;
				--bitshift;
			}
		}
		img->format = IMAGE_BITMAP;
		break;
	case '5':		// Binary Portable graymap
		image_foreach(img, i, j) {
			if (maxval < 256) 
				img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = getc(fp);
			else {
				c1 = getc(fp); c2 = getc(fp);
				g = c1 << 8 | c2;
				g = g*255/maxval;
				img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = g;;
			}
		}
		img->format = IMAGE_GRAY;
		break;
	case '6':		// Binary Portable pixmap
		image_foreach(img, i, j) {
			if (maxval < 256) {
				img->ie[i][j].r = getc(fp);
				img->ie[i][j].g = getc(fp);
				img->ie[i][j].b = getc(fp);
			}
			else {
				c1 = getc(fp); c2 = getc(fp); r = c1 << 8 | c2; r = r*255/maxval;
				c1 = getc(fp); c2 = getc(fp); g = c1 << 8 | c2; g = g*255/maxval;
				c1 = getc(fp); c2 = getc(fp); b = c1 << 8 | c2; b = b*255/maxval;

				img->ie[i][j].r = r; 
				img->ie[i][j].g = g;
				img->ie[i][j].b = b;;
			}
		}
		img->format = IMAGE_RGB;
		break;
	default:
		goto bad_format;
		break;
	}		
	
	fclose(fp);

	return img;

bad_format:
read_fail:		
	fclose(fp);
	return NULL;
 }

static IMAGE *image_loadbmp(char *fname)
{
	FILE *fp;
	int i, j, n, r, g, b, ret;
	BITMAP_FILEHEADER file_header;
	BITMAP_INFOHEADER info_header;
	BITMAP_RGBQUAD color_index_table[256];
 	IMAGE *img;
 
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		syslog_error("open file %s.", fname);
		return NULL;
 	}

	file_header.bfType = __get_word(fp);
	file_header.bfSize = __get_dword(fp);
	file_header.bfReserved1 = __get_word(fp);
	file_header.bfReserved2 = __get_word(fp);
	file_header.bfOffBits = __get_dword(fp);

	if (file_header.bfType != 0X4D42) 	// 'BM'
		goto bad_format;

	info_header.biSize = __get_dword(fp);
	if (info_header.biSize == BITMAP_WIN_NEW || info_header.biSize == BITMAP_OS2_NEW) {
		info_header.biWidth = __get_dword(fp);
		info_header.biHeight = __get_dword(fp);
		info_header.biPlanes = __get_word(fp);
		info_header.biBitCount = __get_word(fp);
		info_header.biCompression = __get_dword(fp);
		info_header.biSizeImage = __get_dword(fp);
		info_header.biXPelsPerMeter = __get_dword(fp);
		info_header.biYPelsPerMeter = __get_dword(fp);
		info_header.biClrUsed = __get_dword(fp);
		info_header.biClrImportant = __get_dword(fp);
	}
	else { // Old format
		info_header.biWidth = __get_word(fp);
		info_header.biHeight = __get_word(fp);
		info_header.biPlanes = __get_word(fp);
		info_header.biBitCount = __get_word(fp);
		info_header.biCompression = 0;
		info_header.biSizeImage = (((info_header.biPlanes * info_header.biBitCount * info_header.biWidth) + 31)/32) * 
						4 * info_header.biHeight;
		info_header.biXPelsPerMeter = 0;
		info_header.biYPelsPerMeter = 0;
		info_header.biClrUsed = 0;
		info_header.biClrImportant = 0;
	}

	n = info_header.biBitCount;
	if ((n != 1 && n != 4 && n != 8 && n != 24  && n != 32)) 
		goto bad_format;

	if (info_header.biPlanes != 1 || info_header.biCompression != BITMAP_NO_COMPRESSION)  // uncompress image
		goto bad_format;

	// read color map ?
	if (info_header.biBitCount != 24  && n != 32) {
		n =(info_header.biClrUsed)? (int)info_header.biClrUsed : 1 << info_header.biBitCount;
		for (i = 0; i < n; i++) {
			ret = fread(&color_index_table[i], sizeof(BITMAP_RGBQUAD), 1, fp);
			assert(ret >= 0);
		}
	}

	img = image_create(info_header.biHeight,  info_header.biWidth);
	if (! img) {
		syslog_error("Create image.");
		// Allocate error !!!
		goto read_fail;
	}
	
	// Begin to read image data
	if (info_header.biBitCount == 1) {
		img->format = IMAGE_BITMAP;
		BYTE c = 0, index;
		n = ((info_header.biWidth + 31)/32) * 32;  /* padded to be a multiple of 32 */
		for (i = info_header.biHeight - 1; i >= 0 && ! FILE_END(fp); i--) {
			for (j = 0; j < n && ! FILE_END(fp); j++) {
				if (j % 8 == 0) 
					c = getc(fp);
				if (j < info_header.biWidth) {
					index = (c & 0x80) ? 1 : 0; c <<= 1;
					img->ie[i][j].r = color_index_table[index].rgbRed;
					img->ie[i][j].g = color_index_table[index].rgbGreen;
					img->ie[i][j].b = color_index_table[index].rgbBlue;
				}
			}
		}
	}
	else if (info_header.biBitCount == 4) {
		img->format = IMAGE_RGB;
		BYTE c = 0, index;
		n = ((info_header.biWidth + 7)/8) * 8; 	/* padded to a multiple of 8pix (32 bits) */
		for (i = info_header.biHeight - 1; i >=0 && ! FILE_END(fp); i--) {
			for (j = 0; j < n && ! FILE_END(fp); j++) {
				if (j % 2 == 0)
					c = getc(fp);
				if ( j < info_header.biWidth) {
					index  = (c & 0xf0) >> 4; c <<= 4;
					img->ie[i][j].r = color_index_table[index].rgbRed;
					img->ie[i][j].g = color_index_table[index].rgbGreen;
					img->ie[i][j].b = color_index_table[index].rgbBlue;
				}
			}
		}
	}
	else if (info_header.biBitCount == 8) {
		img->format = IMAGE_RGB;
		BYTE c;	
		n = ((info_header.biWidth + 3)/4) * 4; 		/* padded to a multiple of 4pix (32 bits) */
		for (i = info_header.biHeight - 1; i >= 0 && ! FILE_END(fp); i--) {
			for (j = 0; j < n && ! FILE_END(fp); j++) {
				c = getc(fp);
				if (j < info_header.biWidth) {
					img->ie[i][j].r = color_index_table[c].rgbRed;
					img->ie[i][j].g = color_index_table[c].rgbGreen;
					img->ie[i][j].b = color_index_table[c].rgbBlue;
				}
			}
		}
	}
	else {
		img->format = IMAGE_RGB;
		n =(4 - ((info_header.biWidth * 3) % 4)) & 0x03;  /* # pad bytes */
		for (i = info_header.biHeight - 1; i >= 0 && ! FILE_END(fp); i--) {
			for (j = 0; j < info_header.biWidth && ! FILE_END(fp); j++) {
				b = getc(fp);  // * blue
				g = getc(fp);  // * green
				r = getc(fp);  // * red
				if (info_header.biBitCount == 32) {	   // dump alpha ?
					getc(fp);
				}

				img->ie[i][j].r = r;
				img->ie[i][j].g = g;
				img->ie[i][j].b = b;
			}
			if (info_header.biBitCount == 24)	{
				for (j = 0; j < n && ! FILE_END(fp); j++) // unused bytes 
					getc(fp);
			}
		}
	}

	fclose(fp);

	return img;

bad_format:
read_fail:		
	fclose(fp);
	return NULL;
 }

// We just support to save 24bit for simple
int image_savebmp(IMAGE *img, const char *fname)
{
	int i, j, k, n, nbits, bpline;
	FILE *fp;

	fp = fopen(fname, "wb");
	if (! fp) {
		syslog_error("Open file %s.", fname);
		return RET_ERROR;
	}

	nbits = 24; 	// biBitCount
	bpline =((img->width * nbits + 31) / 32) * 4;   /* # bytes written per line */

	__put_word(fp, 0x4d42);		// BM
	__put_dword(fp, 14 + 40 + bpline * img->height);	// File header size = 10, infor head size = 40, bfSize
	__put_word(fp, 0);		// Reserved1
	__put_word(fp, 0);		// Reserved2
	__put_dword(fp, 14 + 40);	// bfOffBits, 10 + 40

	__put_dword(fp, 40);				// biSize
	__put_dword(fp, img->width);			// biWidth
	__put_dword(fp, img->height);			// biHeight
	__put_word(fp, 1);				// biPlanes
	__put_word(fp, 24);				// biBitCount = 24
	__put_dword(fp, BITMAP_NO_COMPRESSION);		// biCompression
	__put_dword(fp, bpline * img->height);		// biSizeImage
	__put_dword(fp, 3780);				// biXPerlPerMeter 96 * 39.375
	__put_dword(fp, 3780);				// biYPerlPerMeter
	__put_dword(fp, 0);				// biClrUsed
	__put_dword(fp, 0);				// biClrImportant

	/* write out the colormap, 24 bit no need */
	/* write out the image data */
	n = (4 - ((img->width * 3) % 4)) & 0x03;  	/* # pad bytes to write at EOscanline */
	for (i = 0; i < img->height; i++) {
		k = img->height - i - 1;
		for (j = 0; j < img->width; j++) {
			putc(img->ie[k][j].b, fp);	// Blue
			putc(img->ie[k][j].g, fp);	// Green
			putc(img->ie[k][j].r, fp);	// Red
		}
		for (j = 0; j < n; j++)
			putc(0, fp);
	}

	fclose(fp);
	
	return RET_OK;
}

#ifdef CONFIG_JPEG
static IMAGE *image_loadjpeg(char *fname)
{
	JSAMPARRAY lineBuf;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr err_mgr;
	int bytes_per_pixel;
	FILE *fp = NULL;
	IMAGE *img = NULL;
	int i, j;
		
	if ((fp = fopen (fname, "rb")) == NULL) {
		syslog_error("Open file %s.", fname);
		goto read_fail;
	}

	cinfo.err = jpeg_std_error (&err_mgr);
	err_mgr.error_exit = __jpeg_errexit;	

	jpeg_create_decompress (&cinfo);
	jpeg_stdio_src (&cinfo, fp);
	jpeg_read_header (&cinfo, 1);
	cinfo.do_fancy_upsampling = 0;
	cinfo.do_block_smoothing = 0;
	jpeg_start_decompress (&cinfo);

	bytes_per_pixel = cinfo.output_components;
	lineBuf = cinfo.mem->alloc_sarray ((j_common_ptr) &cinfo, JPOOL_IMAGE, (cinfo.output_width * bytes_per_pixel), 1);
	img = image_create(cinfo.output_height, cinfo.output_width);
	if (! img) {
		syslog_error("Create image.");
		goto read_fail;
	}

	if (bytes_per_pixel == 3) {
		img->format = IMAGE_RGB;
		for (i = 0; i < img->height; ++i) {
			jpeg_read_scanlines (&cinfo, lineBuf, 1);
			for (j = 0; j < img->width; j++) {
				img->ie[i][j].r = lineBuf[0][3 * j];
				img->ie[i][j].g = lineBuf[0][3 * j + 1];
				img->ie[i][j].b = lineBuf[0][3 * j + 2];
			}
		}
	} else if (bytes_per_pixel == 1) {
		img->format = IMAGE_GRAY;
		for (i = 0; i < img->height; ++i) {
			jpeg_read_scanlines (&cinfo, lineBuf, 1);
			for (j = 0; j < img->width; j++) {
				img->ie[i][j].r = lineBuf[0][j];
				img->ie[i][j].g = lineBuf[0][j];
				img->ie[i][j].b = lineBuf[0][j];
			}			
		}
	} else {
		syslog_error("Color channels is %d (1 or 3).", bytes_per_pixel);
		goto read_fail;
	}
	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);
	fclose (fp);

	return img;
read_fail:
	if (fp)
		fclose(fp);
	if (img)
		image_destroy(img);

	return NULL;
}

static int image_savejpeg(IMAGE *img, const char * filename, int quality)
{
	int i, j, row_stride;
	BYTE *ic;		// image context
	FILE * outfile;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	JSAMPLE *img_buffer;

	if (! image_valid(img)) {
		syslog_error("Bad image.");
		return RET_ERROR;
	}
	if ((outfile = fopen(filename, "wb")) == NULL) {
		syslog_error("Create file (%s).", filename);
		return RET_ERROR;
	}
	if (sizeof(RGB) != 3) {
		ic = (BYTE *)malloc(img->height * img->width * 3 * sizeof(BYTE));
		if (! ic) {
			syslog_error("Allocate memory.");
			return RET_ERROR;
		}
		img_buffer = (JSAMPLE *)ic; // img->base;
		image_foreach(img, i, j) {
			*ic++ = img->ie[i][j].r;
			*ic++ = img->ie[i][j].g;
			*ic++ = img->ie[i][j].b;
		}
	}
	else
		img_buffer = (JSAMPLE *)img->base;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = img->width;
	cinfo.image_height = img->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB; 
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	cinfo.next_scanline = 0;
	row_stride = img->width * 3;
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &img_buffer[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);

	jpeg_destroy_compress(&cinfo);

	if (sizeof(RGB) != 3) {
		free(img_buffer);
	}

	return RET_OK;
}
#endif

#ifdef CONFIG_PNG
#include <png.h>
IMAGE *image_loadpng(char *fname)
{
	FILE *fp;	
	IMAGE *image = NULL;
	png_struct *png_ptr = NULL;
	png_info *info_ptr = NULL;
	png_byte buf[8];
	png_byte *png_pixels = NULL;
	png_byte **row_pointers = NULL;
	png_byte *pix_ptr = NULL;
	png_uint_32 row_bytes;

	png_uint_32 width, height;
//	unsigned char r, g, b, a = '\0';
	int bit_depth, channels, color_type, alpha_present, ret;
	png_uint_32 i, row, col;

	if ((fp = fopen (fname, "rb")) == NULL) {
		syslog_error("Loading PNG file (%s). error no: %d", fname, errno);
		goto read_fail;
	}
	

	/* read and check signature in PNG file */
	ret = fread(buf, 1, 8, fp);
	if (ret != 8 || ! png_check_sig(buf, 8)) {
		syslog_error("Png check sig");
		return NULL;
	}

	/* create png and info structures */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		syslog_error("Png create read struct");
		return NULL;	/* out of memory */
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		syslog_error("Png create info struct");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;	/* out of memory */
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		syslog_error("Png jmpbuf");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	/* set up the input control for C streams */
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);	/* we already read the 8 signature bytes */

	/* read the file information */
	png_read_info(png_ptr, info_ptr);

	/* get size and bit-depth of the PNG-image */
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	/* 
	 * set-up the transformations
	 */

	/* transform paletted images into full-color rgb */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	/* expand images to bit-depth 8 (only applicable for grayscale images) */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand(png_ptr);
	/* transform transparency maps into full alpha-channel */
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_expand(png_ptr);

#ifdef NJET
	/* downgrade 16-bit images to 8 bit */
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	/* transform grayscale images into full-color */
	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		    png_set_gray_to_rgb(png_ptr);
	/* only if file has a file gamma, we do a correction */
	if (png_get_gAMA(png_ptr, info_ptr, &file_gamma))
		png_set_gamma(png_ptr, (double) 2.2, file_gamma);
#endif

	/* all transformations have been registered; now update info_ptr data,
	 * get rowbytes and channels, and allocate image memory */

	png_read_update_info(png_ptr, info_ptr);

	/* get the new color-type and bit-depth (after expansion/stripping) */
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	/* calculate new number of channels and store alpha-presence */
	if (color_type == PNG_COLOR_TYPE_GRAY)
		channels = 1;
	else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		channels = 2;
	else if (color_type == PNG_COLOR_TYPE_RGB)
		channels = 3;
	else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
		channels = 4;
	else
		channels = 0;	/* should never happen */
	alpha_present = (channels - 1) % 2;

	/* row_bytes is the width x number of channels x (bit-depth / 8) */
	row_bytes = png_get_rowbytes(png_ptr, info_ptr);

	if ((png_pixels =  (png_byte *)malloc(row_bytes * height * sizeof(png_byte))) == NULL) {
		syslog_error("Alloc memeory.");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	if ((row_pointers =  (png_byte **)malloc(height * sizeof(png_bytep))) == NULL) {
		syslog_error("Alloc memeory.");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(png_pixels);
		png_pixels = NULL;
		return NULL;
	}

	/* set the individual row_pointers to point at the correct offsets */
	for (i = 0; i < (height); i++)
		row_pointers[i] = png_pixels + i * row_bytes;

	/* now we can go ahead and just read the whole image */
	png_read_image(png_ptr, row_pointers);

	/* read rest of file and get additional chunks in info_ptr - REQUIRED */
	png_read_end(png_ptr, info_ptr);

	/* clean up after the read, and free any memory allocated - REQUIRED */
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);

	/* write data to PNM file */
	pix_ptr = png_pixels;

	image = image_create(width, height);

	for (row = 0; row < height; row++) {
		for (col = 0; col < width; col++) {
			if (bit_depth == 16) {
				image->ie[row][col].r = (((unsigned char)*pix_ptr++ << 8) + (unsigned char)*pix_ptr++);
				image->ie[row][col].g = (((unsigned char)*pix_ptr++ << 8) + (unsigned char)*pix_ptr++);
				image->ie[row][col].b = (((unsigned char)*pix_ptr++ << 8) + (unsigned char)*pix_ptr++);
				if (alpha_present)
					image->ie[row][col].a = (((unsigned char)*pix_ptr++ << 8) + (unsigned char)*pix_ptr++);
			} else {
				image->ie[row][col].r = (unsigned char)*pix_ptr++;
				image->ie[row][col].g = (unsigned char)*pix_ptr++;
				image->ie[row][col].b = (unsigned char)*pix_ptr++;
				if (alpha_present)
					image->ie[row][col].a = (unsigned char)*pix_ptr++;
			}
		}
	}

	if (row_pointers != (unsigned char **)NULL)
		free(row_pointers);
	if (png_pixels != (unsigned char *)NULL)
		free(png_pixels);
	
	fclose(fp);
	return image;

read_fail:
	if (fp)
		fclose(fp);
	if (image)
		image_destroy(image);

	return NULL;
}



#endif

IMAGE *image_load(char *fname)
{
	char *extname = strrchr(fname, '.');
	if (extname) {
		if (strcasecmp(extname, ".bmp") == 0)
			return image_loadbmp(fname);
		else if (strcasecmp(extname, ".pbm") == 0 || strcasecmp(extname, ".pgm") == 0  || strcasecmp(extname, ".ppm") == 0)
			return image_loadpnm(fname);
#ifdef CONFIG_JPEG
		if (strcasecmp(extname, ".jpg") == 0 || strcasecmp(extname, ".jpeg") == 0)
			return image_loadjpeg(fname);
#endif
#ifdef CONFIG_PNG
		if (strcasecmp(extname, ".png") == 0)
			return image_loadpng(fname);
#endif		
 	}

#ifdef CONFIG_JPEG
	syslog_error("ONLY support bmp/jpg/jpeg.");
#else
	syslog_error("ONLY support bmp.");
#endif
	return NULL;	
}


int image_save(IMAGE *img, const char *fname)
{
	char *extname = strrchr(fname, '.');
	check_image(img);
	
	if (extname) {
		if (strcasecmp(extname, ".bmp") == 0)
			return image_savebmp(img, fname);
		if (strcasecmp(extname, ".jpg") == 0 || strcasecmp(extname, ".jpeg") == 0)
			return image_savejpeg(img, fname, 100);
 	}
	
	syslog_error("ONLY Support bmp/jpg/jpeg/h2p image.");
	return RET_ERROR;
}

IMAGE *image_copy(IMAGE *img)
{
	IMAGE *copy;
	
	if (! image_valid(img)) {
		syslog_error("Bad image.");
		return NULL;
	}

	if ((copy = image_create(img->height, img->width)) == NULL) {
		syslog_error("Create image.");
		return NULL;
	}
	memcpy(copy->base, img->base, img->height * img->width * sizeof(RGB));
	return copy;
}

IMAGE *image_zoom(IMAGE *img, int nh, int nw, int method)
{
	int i, j, i2, j2;
	double di, dj,  d1, d2, d3, d4, u, v, d;
	IMAGE *copy;

	CHECK_IMAGE(img);
	copy = image_create(nh, nw); CHECK_IMAGE(copy);
		
	di = 1.0 * img->height/copy->height;
	dj = 1.0 * img->width/copy->width;
	if (method == ZOOM_METHOD_BLINE) {
		/**********************************************************************
		d1    d2
		    (p)
		d3    d4
		f(i+u,j+v) = (1-u)(1-v)f(i,j) + (1-u)vf(i,j+1) + u(1-v)f(i+1,j) + uvf(i+1,j+1)
		**********************************************************************/
		image_foreach(copy, i, j) {
			i2 = (int)(di * i);
			u = di * i - i2;
			j2 = (int)(dj * j);
			v = dj * j - j2;
			if (i2 == img->height - 1 || j2 == img->width - 1) {
				copy->ie[i][j].r =  img->ie[i2][j2].r;
				copy->ie[i][j].g =  img->ie[i2][j2].g;
				copy->ie[i][j].b =  img->ie[i2][j2].b;
			}
			else {
				// Red
				d1 =  img->ie[i2][j2].r;
				d2 =  img->ie[i2][j2 + 1].r;
				d3 =  img->ie[i2 + 1][j2].r;
				d4 =  img->ie[i2 + 1][j2 + 1].r;
				d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
				copy->ie[i][j].r = (int)d;
				// Green
				d1 =  img->ie[i2][j2].g;
				d2 =  img->ie[i2][j2 + 1].g;
				d3 =  img->ie[i2 + 1][j2].g;
				d4 =  img->ie[i2 + 1][j2 + 1].g;
				d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
				copy->ie[i][j].g = (int)d;
				// Blue
				d1 =  img->ie[i2][j2].b;
				d2 =  img->ie[i2][j2 + 1].b;
				d3 =  img->ie[i2 + 1][j2].b;
				d4 =  img->ie[i2 + 1][j2 + 1].b;
				d = (1.0 - u) * (1.0 - v) * d1 + (1.0 - u)*v*d2 + u*(1.0 - v)*d3 + u*v*d4;
				copy->ie[i][j].b = (int)d;
			}
		}
	}
	else {
		image_foreach(copy, i, j) {
			i2 = (int)(di * i);
			j2 = (int)(dj * j);
			copy->ie[i][j].r = img->ie[i2][j2].r;
			copy->ie[i][j].g = img->ie[i2][j2].g;
			copy->ie[i][j].b = img->ie[i2][j2].b;
		}
	}
	
	return copy;
}

int image_drawrect(IMAGE *img, RECT *rect, int color, int fill)
{
	int i, j;
	BYTE r, g, b;

	check_image(img);
	image_rectclamp(img, rect);

	r = RGB_R(color);
	g = RGB_G(color);
	b = RGB_B(color);

	if (fill) {
		rect_foreach(rect, i, j) {
			img->ie[rect->r + i][rect->c + j].r = r;
			img->ie[rect->r + i][rect->c + j].g = g;
			img->ie[rect->r + i][rect->c + j].b = b;
		}
	}
	else {
		// ||
		for (i = rect->r; i < rect->r + rect->h; i++) {
			img->ie[i][rect->c].r = r;
			img->ie[i][rect->c + rect->w - 1].r = r;
			img->ie[i][rect->c].g = g;
			img->ie[i][rect->c + rect->w - 1].g = g;
			img->ie[i][rect->c].b = b;
			img->ie[i][rect->c + rect->w - 1].b = b;
		}

		// ==
		for (j = rect->c; j < rect->c + rect->w; j++) {
			img->ie[rect->r][j].r = r;
			img->ie[rect->r + rect->h - 1][j].r = r;
			img->ie[rect->r][j].g = g;
			img->ie[rect->r + rect->h - 1][j].g = g;
			img->ie[rect->r][j].b = b;
			img->ie[rect->r + rect->h - 1][j].b = b;
		}
	}

	return RET_OK;
}

int image_drawline(IMAGE *img, int r1, int c1, int r2, int c2, int color)
{
	int i, j;
	BYTE r, g, b;
	int temp, adj_up, adj_down, error_term,  x_advance, x_delta, y_delta, whole_step,
	    initial_pixel_count, final_pixel_count, run_length;

	check_image(img);
	r = RGB_R(color);
	g = RGB_G(color);
	b = RGB_B(color);

	r1 = CLAMP(r1, 0, img->height - 1);
	r2 = CLAMP(r2, 0, img->height - 1);
	c1 = CLAMP(c1, 0, img->width - 1);
	c2 = CLAMP(c2, 0, img->width - 1);

	/* draw top to bottom */
	if (r1 > r2) {
		temp = r1; r1 = r2; r2 = temp;
		temp = c1; c1 = c2; c2 = temp;
	}

	/* Figure out whether we're going left or right, and how far we're going horizontally. */
	if ((x_delta = c2 - c1) < 0) {
		x_advance = -1; x_delta = -x_delta;
	} else {
		x_advance = 1;
	}

	/* Figure out how far we're going vertically */
	y_delta = r2 - r1;

	/* Special-case horizontal, vertical, and diagonal lines, for speed and to avoid nasty boundary conditions and division by 0. */
	if (x_delta == 0) { /* Vertical line */
		for (i = 0; i <= y_delta; i++) {
			img->ie[r1 + i][c1].r = r;
			img->ie[r1 + i][c1].g = g;
			img->ie[r1 + i][c1].b = b;
		}
		return RET_OK;
	}

	if (y_delta == 0) { /* Horizontal line */
		if (x_advance < 0) {
			for (i = x_delta; i >= 0; i += x_advance) {
				img->ie[r1][c1 - i].r = r;
				img->ie[r1][c1 - i].g = g;
				img->ie[r1][c1 - i].b = b;
			}
		} else {
			for (i = 0; i <= x_delta; i += x_advance) {
				img->ie[r1][c1 + i].r = r;
				img->ie[r1][c1 + i].g = g;
				img->ie[r1][c1 + i].b = b;
			}
		}
		return RET_OK;
	}

	if (x_delta == y_delta) {
		/* Diagonal line */
		for (i = 0; i <= x_delta; i++) {
			j = (i * x_advance);
			img->ie[r1 + i][c1 + j].r = r;
			img->ie[r1 + i][c1 + j].g = g;
			img->ie[r1 + i][c1 + j].b = b;
		}
		return RET_OK;
	}

	/* Determine whether the line is X or Y major, and handle accordingly */
	if (x_delta >= y_delta) {
		/* X major line */
		/* Minimum # of pixels in a run in this line */
		whole_step = x_delta / y_delta;

		/* 
		 * Error term adjust each time Y steps by 1; used to tell 
		 * when one extra pixel should be drawn as part of a run, 
		 * to account for fractional steps along the X axis per 
		 * 1-pixel steps along y
		 */
		adj_up = (x_delta % y_delta) * 2;

		/* 
		 * Error term adjust when the error term turns over, used 
		 * to factor out the X step made at that time
		 */
		adj_down = y_delta * 2;

		/* 
		 * Initial error term; reflects an inital step of 0.5 along 
		 * the Y axis
		 */
		error_term = (x_delta % y_delta) - (y_delta * 2);

		/* 
		 * The initial and last runs are partial, because Y advances 
		 * only 0.5 for these runs, rather than 1.  Divide one full 
		 * run, plus the initial pixel, between the initial and last 
		 * runs
		 */
		initial_pixel_count = (whole_step / 2) + 1;
		final_pixel_count = initial_pixel_count;

		/* 
		 * If the basic run length is even and there's no fractional 
		 * advance, we have one pixel that could go to either the 
		 * inital or last partial run, which we'll arbitrarily allocate
		 * to the last run
		 */
		if ((adj_up == 0) && ((whole_step & 0x01) == 0))
			initial_pixel_count--;

		/* 
		 * If there're an odd number of pixels per run, we have 1 pixel
		 * that can't be allocated to either the initial or last 
		 * partial run, so we'll add 0.5 to error term so this pixel 
		 * will be handled by the normal full-run loop
		 */
		if ((whole_step & 0x01) != 0)
			error_term += y_delta;

		/* Draw the first, partial run of pixels */
		__draw_hline(img, &c1, &r1, initial_pixel_count, x_advance, r, g, b);

		/* Draw all full runs */
		for (i = 0; i < (y_delta - 1); i++) {
			/* run is at least this long */
			run_length = whole_step;

			/* 
			 * Advance the error term and add an extra pixel if 
			 * the error term so indicates
			 */
			if ((error_term += adj_up) > 0) {
				run_length++;
				/* reset the error term */
				error_term -= adj_down;
			}

			/* Draw this scan line's run */
			__draw_hline(img, &c1, &r1, run_length, x_advance, r, g, b);
		}

		/* Draw the final run of pixels */
		__draw_hline(img, &c1, &r1, final_pixel_count, x_advance, r, g, b);
		return RET_OK;
	} else {
		/* Y major line */

		/* Minimum # of pixels in a run in this line */
		whole_step = y_delta / x_delta;

		/* 
		 * Error term adjust each time X steps by 1; used to tell when 
		 * 1 extra pixel should be drawn as part of a run, to account 
		 * for fractional steps along the Y axis per 1-pixel steps 
		 * along X
		 */
		adj_up = (y_delta % x_delta) * 2;

		/* 
		 * Error term adjust when the error term turns over, used to 
		 * factor out the Y step made at that time
		 */
		adj_down = x_delta * 2;

		/* Initial error term; reflects initial step of 0.5 along the 
		 * X axis 
		 */
		error_term = (y_delta % x_delta) - (x_delta * 2);

		/* 
		 * The initial and last runs are partial, because X advances 
		 * only 0.5 for these runs, rather than 1.  Divide one full 
		 * run, plus the initial pixel, between the initial and last 
		 * runs
		 */
		initial_pixel_count = (whole_step / 2) + 1;
		final_pixel_count = initial_pixel_count;

		/* 
		 * If the basic run length is even and there's no fractional 
		 * advance, we have 1 pixel that could go to either the 
		 * initial or last partial run, which we'll arbitrarily 
		 * allocate to the last run
		 */
		if ((adj_up == 0) && ((whole_step & 0x01) == 0))
			initial_pixel_count--;

		/* 
		 * If there are an odd number of pixels per run, we have one 
		 * pixel that can't be allocated to either the initial or last 
		 * partial run, so we'll ad 0.5 to the error term so this 
		 * pixel will be handled by the normal rull-run loop
		 */
		if ((whole_step & 0x01) != 0)
			error_term += x_delta;

		/* Draw the first, partial run of pixels */
		__draw_vline(img, &c1, &r1, initial_pixel_count, x_advance, r, g, b);

		/* Draw all full runs */
		for (i = 0; i < (x_delta - 1); i++) {
			/* run is at least this long */
			run_length = whole_step;

			/* 
			 * Advance the error term and add an extra pixel if the
			 * error term so indicates
			 */
			if ((error_term += adj_up) > 0) {
				run_length++;
				/* reset the error term */
				error_term -= adj_down;
			}

			/* Draw this scan line's run */
			__draw_vline(img, &c1, &r1, run_length, x_advance, r, g, b);
		}

		/* Draw the final run of pixels */
		__draw_vline(img, &c1, &r1, final_pixel_count, x_advance, r, g, b);
		return RET_OK;
	}

	return RET_OK;
}

int image_drawtext(IMAGE *image, int r, int c, char *texts, int color)
{
	return text_puts(image, r, c, texts, color);
}

// NO Lua interface
// make sure dimesion of res is more than 2
int image_estimate(char oargb, IMAGE *orig, IMAGE *curr, VECTOR *res)
{
	int i, j, k;
	long long n, sum;
	double d;

	check_argb(oargb);
	check_image(orig);
	check_image(curr);
	
	if (curr->height != orig->height || curr->width != orig->width) {
		syslog_error("Different size between two images.");
		return RET_ERROR;
	}

	n = sum = 0;
	switch(oargb) {
	case 'A':
		image_foreach(orig, i, j) {
			k = orig->ie[i][j].r - curr->ie[i][j].r;
			n += k * k;
			k = orig->ie[i][j].g - curr->ie[i][j].g;
			n += k * k;
			k = orig->ie[i][j].b - curr->ie[i][j].b;
			n += k * k;
			k = (orig->ie[i][j].r * orig->ie[i][j].r) + (orig->ie[i][j].g * orig->ie[i][j].g) + (orig->ie[i][j].b * orig->ie[i][j].b);
			sum += k;
		}
		break;
 	case 'R':
		image_foreach(orig, i, j) {
			k = orig->ie[i][j].r - curr->ie[i][j].r;
			n += k * k;
			k = (orig->ie[i][j].r * orig->ie[i][j].r);
			sum += k;
		}
 		break;
	case 'G':
		image_foreach(orig, i, j) {
			k = orig->ie[i][j].g - curr->ie[i][j].g;
			n += k * k;
			k = (orig->ie[i][j].g * orig->ie[i][j].g);
			sum += k;
		}
 		break;
	case 'B':
		image_foreach(orig, i, j) {
			k = orig->ie[i][j].b - curr->ie[i][j].b;
			n += k * k;
			k = (orig->ie[i][j].b * orig->ie[i][j].b);
			sum += k;
		}
 		break;
	default:
		syslog_error("Bad color %c (ARGB).", oargb);
		return RET_ERROR;
		break;
	}

	if (sum == 0)		// Force sum == 1
		sum = 1;
	d = 1.0f * n/sum;
	printf("NMSE = %f\n", d);
	if (res && res->m > 0)
		res->ve[0] = d;

	// Calculate 10 * logf(3 * 255 * 255 * orig->height * orig->width / n);
	if (n == 0)
		n = 1;		// FORCE n == 1
	if (oargb == 'A')
		d =  log(195075.0f * orig->height * orig->width / n);
	else
		d =  log(65025.0f * orig->height * orig->width / n);
	d = 10.0f * d;
	printf("PSNR = %f\n", d);
	if (res && res->m > 1)
		res->ve[1] = d;

	return RET_OK;
}

int image_paste(IMAGE *img, int r, int c, IMAGE *small, double alpha)
{
	int i, j, x2, y2, offx, offy;
	
	check_image(img);
	if (alpha < 0 || alpha > 1.00f) {
		syslog_error("Bad paste alpha parameter %f (0 ~ 1.0f).", alpha);
		return RET_ERROR;
	}

	// Calculate (image & small) region
	offx = offy = 0;
	y2 = r + small->height;
	x2 = c + small->width;
	if (r < 0) {
		r = 0;	offy = -r;
	}
	if (c < 0) {
		c = 0; offx = -c;
	}
	if (y2 > img->height)
		y2 = img->height;
	if (x2 > img->width)
		x2 = img->width;

	// Merge
	for (i = 0; i < y2 - r; i++) {
		for (j = 0; j < x2 - c; j++) {
			img->ie[i+r][j+c].r = (BYTE)(alpha * img->ie[i+r][j+c].r + (1.0f - alpha) * small->ie[i+offy][j+offx].r);
			img->ie[i+r][j+c].g = (BYTE)(alpha * img->ie[i+r][j+c].g + (1.0f - alpha) * small->ie[i+offy][j+offx].g);
			img->ie[i+r][j+c].b = (BYTE)(alpha * img->ie[i+r][j+c].b + (1.0f - alpha) * small->ie[i+offy][j+offx].b);
		}
	}

	return RET_OK;	
}

int image_rect_paste(IMAGE *bigimg, RECT *bigrect, IMAGE *smallimg, RECT *smallrect)
{
	int i, j;
	check_image(bigimg);
	check_image(smallimg);

	image_rectclamp(bigimg, bigrect);
	image_rectclamp(smallimg, smallrect);

	if (bigrect->h != smallrect->h || bigrect->w != smallrect->w) {
		syslog_error("Paste size is not same.");
		return RET_ERROR;
	}

	for (i = 0; i < smallrect->h; i++) {
		for (j = 0; j < smallrect->w; j++) {
			bigimg->ie[i + bigrect->r][j + bigrect->c].r = smallimg->ie[i + smallrect->r][j + smallrect->c].r;
			bigimg->ie[i + bigrect->r][j + bigrect->c].g = smallimg->ie[i + smallrect->r][j + smallrect->c].g;
			bigimg->ie[i + bigrect->r][j + bigrect->c].b = smallimg->ie[i + smallrect->r][j + smallrect->c].b;
		}
	}
	return RET_OK;
}

int image_make_noise(IMAGE *img, char orgb, int rate)
{
	int i, j, v;
 
	check_argb(orgb);
	check_image(img);

	srandom((unsigned int)time(NULL));
	image_foreach(img, i, j) {
		if (i == 0 || i == img->height - 1 || j == 0 || j == img->width - 1)
			continue;

		v = random() % 100;
		if (v >= rate)
			continue;
		v = (v % 2 == 0) ? 0 : 255;
		// Add noise
		image_setvalue(img, orgb, i, j, (BYTE)v);
	}

	return RET_OK;
}

// Auto middle value filter
int image_delete_noise(IMAGE *img)
{
	int i, j, index;
	RGB *mid, *cell;
	IMAGE *orig;

	check_image(img);
	orig = image_copy(img);
	check_image(orig);
	for (i = 1; i < img->height - 1; i++) {
		for (j = 1; j < img->width - 1; j++) {
			__nb3x3_map(orig, i, j);
			cell = &(img->base[i * img->width + j]);
			index = __color_rgbfind(cell, 3 * 3, __image_rgb_nb);
			
			if (ABS(index - 4) > 1) {
				mid = __image_rgb_nb[4];
				cell->r = mid->r;
				cell->g = mid->g;
				cell->b = mid->b;
			}
		}
	}
	image_estimate('A', orig, img, NULL);
	image_destroy(orig);

	return RET_OK;
}

int image_statistics(IMAGE *img, char orgb, double *avg, double *stdv)
{
	RECT rect;
	
	image_rect(&rect, img);
	return image_rect_statistics(img, &rect, orgb, avg, stdv);
}


int image_rect_statistics(IMAGE *img, RECT *rect, char orgb, double *avg, double *stdv)
{
	BYTE n;
	int i, j;
	double davg, dstdv;

	check_image(img);

	image_rectclamp(img, rect);

	davg = dstdv = 0.0f;
	switch(orgb) {
	case 'A':
		rect_foreach(rect, i, j) {
			color_rgb2gray(img->ie[rect->r + i][rect->c + j].r, 
					img->ie[rect->r + i][rect->c + j].g, img->ie[rect->r + i][rect->c + j].b, &n);
			davg += n;
			dstdv += n*n;
		}
		break;
	case 'R':
		rect_foreach(rect, i, j) {
			n = img->ie[rect->r + i][rect->c + j].r;
			davg += n;
			dstdv += n*n;
		}

		break;
	case 'G':
		rect_foreach(rect, i, j) {
			n = img->ie[rect->r + i][rect->c + j].g;
			davg += n;
			dstdv += n*n;
		}

		break;
	case 'B':
		rect_foreach(rect, i, j) {
			n = img->ie[rect->r + i][rect->c + j].b;
			davg += n;
			dstdv += n*n;
		}

		break;
	default:
		break;
	}
	
	davg /= (rect->h * rect->w);
	*avg = davg;
	*stdv = sqrt(dstdv/(rect->h * rect->w) - davg*davg);

	return RET_OK;
}


// action: EDOCBb, E-erode, D-dilate, O-open, C-close, B-oborder, b-iborder
int image_morph(IMAGE *img, char action)
{
	int i, j;
	MATRIX *temp, *mat;

	temp = matrix_create(3,3); check_matrix(temp);
	matrix_pattern(temp, "3x3disc");
	
	mat = image_getplane(img, 'A'); check_matrix(mat);

	matrix_morph(action, mat, temp);

	image_foreach(img, i, j) {
		img->ie[i][j].r &= 0x08;
		img->ie[i][j].g &= 0x08;
		img->ie[i][j].b &= 0x08;
	}

	image_setplane(img, 'R', mat);
	image_setplane(img, 'G', mat);
	image_setplane(img, 'B', mat);

	matrix_destroy(mat);
	matrix_destroy(temp);

	return RET_OK;	
}

int image_show(char *title, IMAGE *img)
{
	return image_save(img, "image_show.jpg");
}


IMAGE *image_hmerge(IMAGE *image1, IMAGE *image2)
{
	int i, j, k;
	IMAGE *image;

	CHECK_IMAGE(image1);
	CHECK_IMAGE(image2);

	image = image_create(MAX(image1->height, image2->height), image1->width + image2->width);
	CHECK_IMAGE(image);
	// paste image 1
	image_foreach(image1, i, j) {
		image->ie[i][j].r = image1->ie[i][j].r;
		image->ie[i][j].g = image1->ie[i][j].g;
		image->ie[i][j].b = image1->ie[i][j].b;
	}

	// paste image 2
	k = image1->width;
	image_foreach(image2, i, j) {
		image->ie[i][j + k].r = image2->ie[i][j].r;
		image->ie[i][j + k].g = image2->ie[i][j].g;
		image->ie[i][j + k].b = image2->ie[i][j].b;
	}

	return image;
}

// matter center
int image_mcenter(IMAGE *img, char orgb, int *crow, int *ccol)
{
	RECT rect;
	image_rect(&rect, img);
	return image_rect_mcenter(img, &rect, orgb, crow, ccol);
}

// matter center
int image_rect_mcenter(IMAGE *img, RECT *rect, char orgb, int *crow, int *ccol)
{
	BYTE n;
	int i, j;
	long long m00, m01, m10;

	check_argb(orgb);
	check_image(img);
	image_rectclamp(img, rect);

	if (! crow || ! ccol) {
		syslog_error("Result is NULL.");
		return RET_ERROR;
	}

	m00 = m10 = m01 = 0;
	switch(orgb) {
		case 'A':
			rect_foreach(rect,i,j) {
				color_rgb2gray(img->ie[i + rect->r][j + rect->c].r,
					img->ie[i + rect->r][j + rect->c].g,
					img->ie[i + rect->r][j + rect->c].b, &n);
				m00 += n;
				m10 += i * n;
				m01 += j * n;
			}
			break;
		case 'R':
			rect_foreach(rect,i,j) {
				n = img->ie[i + rect->r][j + rect->c].r;
				m00 += n;
				m10 += i * n;
				m01 += j * n;
			}
			break;
		case 'G':
			rect_foreach(rect,i,j) {
				n = img->ie[i + rect->r][j + rect->c].g;
				m00 += n;
				m10 += i * n;
				m01 += j * n;
			}
			break;
		case 'B':
			rect_foreach(rect,i,j) {
				n = img->ie[i + rect->r][j + rect->c].b;
				m00 += n;
				m10 += i * n;
				m01 += j * n;
			}
			break;
		default:
			break;
	}
	
	if (m00 == 0) {
		*crow = rect->r + rect->h/2;
		*ccol = rect->c + rect->w/2;
		return RET_OK;
	}	
	*crow =  (int)(m10 / m00 + rect->r + rect->h/2);
	*ccol = (int)(m01 / m00 + rect->c + rect->w/2);

	return RET_OK;
}

// r,g,b, w, class
MATRIX *image_classmat(IMAGE *image)
{
	int i, j, k, n, weight[0xffff + 1];
	int r, g, b;
	MATRIX *mat;

	memset(weight, 0, ARRAY_SIZE(weight) * sizeof(int));
	image_foreach(image, i, j) {
		k = RGB565_NO(image->ie[i][j].r, image->ie[i][j].g, image->ie[i][j].b);
		weight[k]++;
	}
	
	// Count no-zero colors
	for (n = 0, i = 0; i < ARRAY_SIZE(weight); i++) {
		if (weight[i])
			n++;
	}

	mat = matrix_create(n, 5); CHECK_MATRIX(mat);
	for (n = 0, i = 0; i < ARRAY_SIZE(weight); i++) {
		if (weight[i]) {
			r = RGB565_R(i);
			g = RGB565_G(i);
			b = RGB565_B(i);
			
			mat->me[n][0] = r;
			mat->me[n][1] = g;
			mat->me[n][2] = b;
			mat->me[n][3] = weight[i];
			n++;
		}
	}

	return mat;
}


// voice = vector of image cube entropy !
VECTOR *image_voice(IMAGE *image, int rows, int cols, int levs, int binary)
{
	double d1, d2, e;
	int i, j, k, n, bh, bw, ball, *count;	// block
	int threshold = MAX(128/levs, 2);
	VECTOR *voice = NULL;
	MATRIX *graymat;

	voice = vector_create(rows * cols * levs); CHECK_VECTOR(voice);

	count = color_count(image, rows, cols, levs);
	if (! count) {
		vector_destroy(voice);
		return NULL;
	}

	bh = (image->height + rows - 1)/rows;
	bw = (image->width + cols - 1)/cols;
	ball = bh * bw;
	
	n = 0;
	graymat = image_gstatics(image, rows, cols);
	// Calculate entroy
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			for (k = 0; k < levs; k++) {
				d1 = count[VOICE_CELL_OFFSET(i, j, k)];
				d1 /= ball;
				d2 = 1.0f - d1;
				e = (d1 >= MIN_DOUBLE_NUMBER)? -d1 * log2(d1) : 0.0f;
				e += (d2 >= MIN_DOUBLE_NUMBER)? -d2 * log2(d2) : 0.0f;
				if (binary)
					e = (e > 0.5)? 1.0f : 0.0f;
				// clear edges from cluster
				if (i > 0 && i < rows - 1 && j > 0 && j < cols -1 && \
					ABS(graymat->me[i][j - 1] - graymat->me[i][j + 1]) < threshold && \
					ABS(graymat->me[i-1][j] - graymat->me[i+1][j]) < threshold)
					e = 0;
				voice->ve[n] = e;
				n++;
			}
		}
	}
	free(count);
	matrix_destroy(graymat);

	return voice;
}

void image_voice_show(IMAGE *image, int rows, int cols, int levs, int number)
{
	int i, j, k, v;
	
	IMAGE *show;
	VECTOR *voice = image_voice(image, rows, cols, levs, 1);

	number = MAX(1, number);
	number = MIN(levs, number);

	show = image_create(rows, cols);
 	image_foreach(show, i, j) {
		k = VOICE_CELL_OFFSET(i, j, number);
		v = (int)(voice->ve[k] * 255.0f);
		show->ie[i][j].r = v;
		show->ie[i][j].g = v;
		show->ie[i][j].b = v;
	}

	image_destroy(show);
}


double image_likeness(IMAGE *a, IMAGE *b, int debug)
{
	int i, j, k, n, hamming;
	double likeness;
	VECTOR *v1, *v2;
	MATRIX *difference = NULL;
	int rows = CUBE_DEF_ROWS, cols = CUBE_DEF_COLS, levs = CUBE_DEF_LEVS;

	if (debug) {
		difference = matrix_create(rows, cols); check_matrix(difference);
	}
	
	v1 = image_voice(a, rows, cols, levs, 1); check_vector(v1);
	v2 = image_voice(b, rows, cols, levs, 1); check_vector(v2);

	hamming = vector_hamming(v1, v2);
	likeness = 1.0f - (double)hamming/(double)v1->m;

	if (debug) {
		n = 0;
		for (k = 0; k < levs; k++) {
			for (i = 0; i < rows; i++) {
				for (j = 0; j < cols; j++) {
					if (ABS(v1->ve[n] - v2->ve[n]) >= MIN_DOUBLE_NUMBER)
						difference->me[i][j] += 1;
					n++;
				}
			}
		}

		printf("A fingerprint: ");
		vector_print(v1, "%d");
		printf("Hex fingerprint: 0x");
		for (i = 0; i < n; i += 8) {
			k = 0;
			for (j = 0; j < 7; j++) {
				if (v1->ve[i + j] > 0)
					k |= (1 << (7 - j));
			}
			printf("%02x", k);
 		}
		printf("\n");

		printf("B fingerprint: ");
		vector_print(v2, "%d");
		printf("Hex fingerprint: 0x");
		for (i = 0; i < n; i += 8) {
			k = 0;
			for (j = 0; j < 7; j++) {
				if (v2->ve[i + j] > 0)
					k |= (1 << (7 - j));
			}
			printf("%02x", k);
 		}
		printf("\n");

		printf("Image likeness(dim: %d) = %f\n", rows * cols *levs, likeness);
		printf("Difference matrix:");
		matrix_print(difference, "%d");

		RECT rect;
		IMAGE *c = image_hmerge(a, b);
		image_drawline(c, 0, a->width, c->height, a->width, 0xffffff);
		double d = 0;
		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
				if (difference->me[i][j] > CUBE_DEF_LEVS/2) {
					d++;
 					rect.h = a->height/rows;
					rect.w = a->width/cols;
					rect.r = i*rect.h;
					rect.c = j*rect.w;
					rect.h++; rect.w++;
 					image_drawrect(c, &rect, 0xff0000, 0);

					rect.h = b->height/rows;
					rect.w = b->width/cols;
					rect.r = i*rect.h;
					rect.c = a->width + j*rect.w;
					rect.h++; rect.w++;
					image_drawrect(c, &rect, 0xff0000, 0);
 				}
			}
		}
		image_save(c, "likeness.bmp");
		image_destroy(c);
		d /= (rows*cols);
		printf("Block likeness: %f\n", 1 - d);
	}

	if (debug) {
		matrix_destroy(difference);
	}
	vector_destroy(v1);
	vector_destroy(v2);

	return likeness;
}


double image_cosine(IMAGE *a, IMAGE *b, int debug)
{
	double likeness;
	VECTOR *v1, *v2;
	int rows = CUBE_DEF_ROWS, cols = CUBE_DEF_COLS, levs = CUBE_DEF_LEVS;

	v1 = image_voice(a, rows, cols, levs, 0); check_vector(v1);
	v2 = image_voice(b, rows, cols, levs, 0); check_vector(v2);

	vector_cosine(v1, v2, &likeness);

	if (debug) {
		printf("Image Cosine Likeness: %f\n\n", likeness);
		printf("Image A: "); vector_print(v1, "%f");
		printf("Image B: "); vector_print(v2, "%f");
	}

	vector_destroy(v1);
	vector_destroy(v2);

	return likeness;
}

static void image_entropy_show(IMAGE *image, int rows, int cols, MATRIX *entropy)
{
	int i, j;
	int bh, bw;

	RECT rect;
	char buf[16];
	int color = 0x00ffff; // color_picker();

	bh = (image->height + rows - 1)/rows;
	bw = (image->width + cols - 1)/cols;

	rect.h = bh; rect.w = bw;
	for (i = 0; i < rows; i++) {
		rect.r = i*bh;
		for (j = 0; j < cols; j++) {
			rect.c = j*bw;
			image_drawrect(image, &rect, color, 0);
			if (entropy->me[i][j] > MIN_DOUBLE_NUMBER) {
				snprintf(buf, sizeof(buf) - 1, "%.2f", entropy->me[i][j]);
				image_drawtext(image, rect.r + 1, rect.c + 1, buf, color);
			}
		}
	}
}


MATRIX *image_entropy(IMAGE *image, int rows, int cols, int levs, int debug)
{
	int i, j, k, bh, bw, ball;	// block
	int *count;
	double d, e;
	MATRIX *mat;

	mat = matrix_create(rows, cols); CHECK_MATRIX(mat);

	count = color_count(image, rows, cols, levs);
	if (count == NULL)
		return NULL;

	// Calculate entroy
	bh = (image->height + rows - 1)/rows;
	bw = (image->width + cols - 1)/cols;
	ball = bh * bw;
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			e = 0.0f;
			for (k = 0; k < levs; k++) {
				d = count[VOICE_CELL_OFFSET(i, j, k)]; d /= ball;
				e += (d >= MIN_DOUBLE_NUMBER)? -d*log2(d): 0.0f;
			}
			mat->me[i][j] = e;
		}
	}

	if (debug)
		image_entropy_show(image, rows, cols, mat);

	free(count);

	return mat;
}

IMAGE *image_subimg(IMAGE *img, RECT *rect)
{
	int i; 
	IMAGE *sub;
	
	image_rectclamp(img, rect);
	sub = image_create(rect->h, rect->w); CHECK_IMAGE(sub);
#if 0
	int j;
	for (i = 0; i < rect->h; i++) {
		for (j = 0; j < rect->w; j++) {
			sub->ie[i][j].r = img->ie[i + rect->r][j + rect->c].r;
			sub->ie[i][j].g = img->ie[i + rect->r][j + rect->c].g;
			sub->ie[i][j].b = img->ie[i + rect->r][j + rect->c].b;
		}
	}
#else	// Fast
	for (i = 0; i < rect->h; i++) {
		memcpy(&(sub->ie[i][0]), &(img->ie[i + rect->r][rect->c]), sizeof(RGB)*rect->w);
	}
#endif
	return sub;
}

void image_drawrects(IMAGE *img)
{
	int i;
	
	RECTS *mrs = rect_set();
//	printf("Motion Set: %d\n", mrs->count);
	for (i = 0; i < mrs->count; i++) {
		// printf("  %d: h = %d, w = %d\n", i, mrs->rect[i].h, mrs->rect[i].w);
		mrs->rect[i].r += rand()%3;
		image_drawrect(img, &mrs->rect[i],	color_picker(), 0);
	}
}

// Gray stattics
MATRIX *image_gstatics(IMAGE *img, int rows, int cols)
{
	BYTE gray;
	MATRIX *mat;
	int i, j, bh, bw, ball, i1, j1;
	
	mat = matrix_create(rows, cols); CHECK_MATRIX(mat);
	
	bh = (img->height + rows - 1)/rows;
	bw = (img->width + cols - 1)/cols;
	image_foreach(img,  i, j) {
		i1 = (i/bh); j1 = (j/bw);
		color_rgb2gray(img->ie[i][j].r, img->ie[i][j].g, img->ie[i][j].b, &gray);
		mat->me[i1][j1] += gray;
	}
	ball = bh * bw;
	matrix_foreach(mat,i,j)
		mat->me[i][j] /= ball;

	return mat;
}


IMAGE *image_prydown(IMAGE *img)
{
	int hh, hw;	// half height, width

	CHECK_IMAGE(img);
	hh = (img->height + 1)/2;
	hw = (img->width + 1)/2;

	image_gauss_filter(img, 2);
	return image_zoom(img, hh, hw, ZOOM_METHOD_COPY);
}

IMAGE *image_pryup(IMAGE *img)
{
	IMAGE *dst;
	CHECK_IMAGE(img);

	dst = image_zoom(img, 2*img->height, 2*img->width, ZOOM_METHOD_BLINE);
	CHECK_IMAGE(dst);
		
	image_gauss_filter(dst, 2);

	return dst;
}

// Center: cr, cc
// Radius: rr, rc


IMAGE *image_defisheye1(IMAGE *src, int cr, int cc, float rr, float rc, float zoom)
{
	int i, j, r, c, h, w;
	IMAGE *dst;
	
	CHECK_IMAGE(src);

	h = (int)rr * zoom;
	w = (int)rc * zoom;
	dst = image_create(h, w); CHECK_IMAGE(dst);
	h /= 2; w /= 2;
	image_foreach(dst, i, j) {
		r = cr + (int)rr*(i - h)/sqrt(rr*rr + (i - h)*(i - h));
		c = cc + (int)rc*(j - w)/sqrt(rc*rc + (j - w)*(j - w));

		if (r >= 0 && r < src->height && c >= 0 && c < src->width) {
			dst->ie[i][j].r = src->ie[r][c].r;
			dst->ie[i][j].g = src->ie[r][c].g;
			dst->ie[i][j].b = src->ie[r][c].b;
		}
	}

	return dst;
}


/*
input:
    strength as floating point >= 0.  0 = no change, high numbers equal stronger correction.
    zoom as floating point >= 1.  (1 = no change in zoom)

algorithm:

    set halfWidth = imageWidth / 2
    set halfHeight = imageHeight / 2
    
    if strength = 0 then strength = 0.00001
    set correctionRadius = squareroot(imageWidth ^ 2 + imageHeight ^ 2) / strength

    for each pixel (x,y) in destinationImage
        set newX = x - halfWidth
        set newY = y - halfHeight

        set distance = squareroot(newX ^ 2 + newY ^ 2)
        set r = distance / correctionRadius
        
        if r = 0 then
            set theta = 1
        else
            set theta = arctangent(r) / r

        set sourceX = halfWidth + theta * newX * zoom
        set sourceY = halfHeight + theta * newY * zoom

        set color of pixel (x, y) to color of source image pixel at (sourceX, sourceY)


*/

IMAGE *image_defisheye(IMAGE *src, double strength, double zoom)
{
	int i, j, r, c, h, w;
	double d, distance, radius, theta;
	
	IMAGE *dst;
	
	CHECK_IMAGE(src);

	h = src->height; w = src->width;

	if (strength < MIN_DOUBLE_NUMBER)
		strength = MIN_DOUBLE_NUMBER;

	radius = sqrt(src->height * src->height + src->width * src->width)/strength;
	dst = image_create(h, w); CHECK_IMAGE(dst);

	h /= 2; w /= 2;
	image_foreach(dst, i, j) {
		r = i - h;
		c = j - w;
		
		distance = sqrt(r*r + c*c);
		d = distance/radius;
		if (d < MIN_DOUBLE_NUMBER)
			theta = 1.0f;
		else
			theta = atan(d)/d;

		r = (int)(h + theta * r * zoom);
		c = (int)(w + theta * c * zoom);

		if (r >= 0 && r < src->height && c >= 0 && c < src->width) {
			dst->ie[i][j].r = src->ie[r][c].r;
			dst->ie[i][j].g = src->ie[r][c].g;
			dst->ie[i][j].b = src->ie[r][c].b;
		}
	}

	return dst;
}

