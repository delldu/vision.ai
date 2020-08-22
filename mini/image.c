/************************************************************************************
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include "image.h"

#include <stdlib.h>
#include <errno.h>

// Support JPEG image
#include <jpeglib.h>
#include <jerror.h>

// Support PNG image
#include <png.h>


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

int image_memsize(WORD h, WORD w);
void image_membind(IMAGE *img, WORD h, WORD w);

static void __jpeg_errexit (j_common_ptr cinfo)
{
	cinfo->err->output_message (cinfo);
	exit (EXIT_FAILURE);
}

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


int image_valid(IMAGE *img)
{
	return (! img || img->height < 1 || img->width < 1 || ! img->ie || img->magic != IMAGE_MAGIC) ? 0 : 1;
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
		break;
	case '2':		// ASCII Portable graymap
		image_foreach(img, i, j) {
			g = __get_pnmdword(fp);
			if (g > 255)
				g = g * 255/maxval;
			img->ie[i][j].r = img->ie[i][j].g = img->ie[i][j].b = (BYTE)g;
		}
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
		for (i = 0; i < img->height; ++i) {
			jpeg_read_scanlines (&cinfo, lineBuf, 1);
			for (j = 0; j < img->width; j++) {
				img->ie[i][j].r = lineBuf[0][3 * j];
				img->ie[i][j].g = lineBuf[0][3 * j + 1];
				img->ie[i][j].b = lineBuf[0][3 * j + 2];
			}
		}
	} else if (bytes_per_pixel == 1) {
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


IMAGE *image_load(char *fname)
{
	char *extname = strrchr(fname, '.');
	if (extname) {
		if (strcasecmp(extname, ".bmp") == 0)
			return image_loadbmp(fname);
		else if (strcasecmp(extname, ".pbm") == 0 || strcasecmp(extname, ".pgm") == 0  || strcasecmp(extname, ".ppm") == 0)
			return image_loadpnm(fname);
		if (strcasecmp(extname, ".jpg") == 0 || strcasecmp(extname, ".jpeg") == 0)
			return image_loadjpeg(fname);
		if (strcasecmp(extname, ".png") == 0)
			return image_loadpng(fname);
 	}

	syslog_error("ONLY support bmp/jpg/jpeg/pbm/pgm/ppm.");
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

