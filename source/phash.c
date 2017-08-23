/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Mon Aug  7 10:24:38 CST 2017
***
************************************************************************************/


#include "phash.h"

#define PHASH_ROWS 8
#define PHASH_COLS 8

static PHASHS __global_phash_set;

PHASHS *phash_set()
{
	return &__global_phash_set;
}

int phash_hamming(PHASH f1, PHASH f2)
{
	int h;
	PHASH d;
	
	h = 0;
	d = f1 ^ f2;
	while(d) {
		h++;
		d &= d - 1;
	}

	return h;
}


double phash_likeness(PHASH f1, PHASH f2)
{
	double t;

	t = phash_hamming(f1, f2);
	
	t /= (8.0f*sizeof(PHASH));

	return 1.0f - t;
}


double phash_maxlike(PHASH d, int n, PHASH *model)
{
	int i;
	double t, likeness;
	likeness = 0.0f;

	for (i = 0; i < n; i++) {
		t = phash_likeness(d, model[i]);
			
		if (t > likeness) {
			likeness = t;
		}
	}
	return likeness;
}


// Perceptual hash
// Hash Image ...
PHASH phash_image(IMAGE *image, int debug)
{
	BYTE n;
	RECT rect;
	PHASH finger;
	int i, j, k, i2, j2, bh, bw, avg, count[PHASH_ROWS][PHASH_COLS]; // bh -- block height, bw -- block width

	bh = image->height/PHASH_ROWS;
	bw = image->width/PHASH_COLS;

	rect.h = bh*PHASH_ROWS;
	rect.w = bw*PHASH_COLS;
	rect.r = (image->height - rect.h)/2;
	rect.c = (image->width - rect.w)/2;

	// Statistics
	k = avg = 0;
	memset(count, 0, PHASH_ROWS*PHASH_COLS*sizeof(int));
	rect_foreach(&rect,i,j) {
		i2 = (i/bh); j2 = (j/bw);
		color_rgb2gray(image->ie[i + rect.r][j + rect.c].r, image->ie[i + rect.r][j + rect.c].g, image->ie[i + rect.r][j + rect.c].b, &n);
		n /= 4; // 64 Level Gray
		count[i2][j2] += n; avg += n; k++;
 	}

	// Average
	avg /= k;
	for (i = 0; i < PHASH_ROWS; i++) {
		for (j = 0; j < PHASH_COLS; j++) {
			count[i][j] /= (bh*bw);
			count[i][j] = (count[i][j] >= avg)? 1 : 0;
		}
	}

	// Collect fingerprint
	finger = 0L;
	for (i = 0; i < PHASH_ROWS; i++) {
		for (j = 0; j < PHASH_COLS; j++) {
			k = PHASH_ROWS * PHASH_COLS - (i * PHASH_COLS + j) - 1;
			if (count[i][j])
				finger |= (0x01L << k);
		}
	}


	if (debug) {
		printf("Image Fingerprint: 0x%lx\n", finger);
		for (i = 0; i < PHASH_ROWS; i++) {
			for (j = 0; j < PHASH_COLS; j++) {
				k = PHASH_ROWS * PHASH_COLS - (i * PHASH_COLS + j) - 1;
				printf("%d ", (int)((finger >> k)&0x1));
			}
			printf("\n");
		}
	}

	return finger;
}

// Hash Objects ...
int phash_objects(IMAGE *img)
{
	int i;
	IMAGE *subimg;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();

	check_image(img);
	for (i = 0; i < mrs->count; i++) {
		if (mrs->rect[i].h < 2*PHASH_ROWS || mrs->rect[i].w < 2*PHASH_COLS) {
			mrs->rect[i].h = mrs->rect[i].w = 0;
			mps->phash[i] = 0L;
			continue;
		}
		subimg = image_subimg(img, &mrs->rect[i]); check_image(subimg);
		mps->phash[i] = phash_image(subimg, 0);
		image_destroy(subimg);
	}
	mps->count = mrs->count;

	return RET_OK;
}

