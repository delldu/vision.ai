/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Tue Feb  7 12:41:41 CST 2017
***
************************************************************************************/

#include "vision.h"
#include "sview.h"
#include "filter.h"
#include "color.h"
#include "retinex.h"
#include "phash.h"

#define DEFAULT_COLOR_VECTOR_DIMENSION 32
#define DEFAULT_SHAPE_VECTOR_DIMENSION 36
#define DEFAULT_TEXTURE_VECTOR_DIMENSION 32

static int limage_help(lua_State *L)
{
	(void)L;

	printf("Image Object help:\n");
	printf("\n");
	printf("Functions:\n");
	printf("  image.create(h, w)\n");
	printf("  image.load(filename)\n");
	printf("  image.likeness(image-a, image-b, debug)\n");
	printf("  image.match(image-a, image-b, debug)      Sift match\n");
	printf("\n");
	printf("Methods:\n");
	printf("  o:show()\n");
	printf("  o:rows()\n");
	printf("  o:cols()\n");
	printf("  o:get(row,col)\n");
	printf("  o:set(row,col,color)\n");
	printf("  o:copy(src-image)\n");
	printf("  o:zoom(src-image, nh, nw, method)     Method: 0 -- Copy, 1 -- Bline\n");
	printf("  o:sub(src-image, start-r, start-c, nh, nw)\n");
	printf("  o:save(filename)\n");
	printf("\n");
	printf("  o:voice(levs, rows, cols, binary)         Voice means 'vector of image cube entropy' \n");
	printf("\n");
	printf("  o:draw_box(top, left, bottom, right, color, fill)\n");
	printf("  o:draw_line(r1, c1, r2, c2, color)\n");
	printf("  o:draw_text(r, c, text, color)\n");
	printf("  o:line_detect(image, debug)\n");
	printf("\n");
	printf("  o:make_noise(color, rate)\n");
	printf("  o:delete_noise()\n");
	printf("  o:gauss_filter(sigma)\n");
	printf("  o:bilate_filter(space-sigma, value-sigma)\n");
	printf("  o:paste_image(r, c, small-image, alpha)\n");
	printf("\n");
	printf("  o:color_vector(ndim)\n");
	printf("  o:color_entropy(ndim)\n");
	printf("  o:color_cluster(ndim)\n");
	printf("  o:skin_detect()\n");
	printf("\n");
	printf("  o:shape_vector(ndim)\n");
	printf("  o:shape_entropy(ndim)\n");
	printf("  o:shape_likeness(image-a, image-b, ndim)\n");
	printf("  o:shape_contour()\n");
	printf("  o:shape_skeleton()\n");
	printf("  o:shape_midedge()\n");
	printf("  o:shape_bestedge()\n");
	printf("  o:morph_transfer(action)                Action: E-erode, D-dilate, O-open, C-close, B-oborder, b-iborder\n");
	printf("\n");
	printf("  o:texture_vector(ndim)\n");
	printf("  o:texture_entropy(ndim)\n");
	printf("\n");
	printf("  o:object_detect(zoom)\n");

	return 0;
}

static int limage_new(lua_State *L)
{
	lua_Integer h = luaL_checkinteger(L, 1);
	lua_Integer w = luaL_checkinteger(L, 2);
	
	luaL_argcheck(L, 1 <= h, 1, "height out of range");
	luaL_argcheck(L, 1 <= w, 2, "width out of range");

	lua_image_create(L, h, w);
	return 1;
}

static int limage_load(lua_State *L) {
	const char* fname = lua_tostring(L, 1);
	IMAGE *fimg = image_load((char *)fname);
	if (fimg == NULL) {
//		lua_pushnil(L);
		return 0;
	}

	IMAGE *img = lua_image_create(L, fimg->height, fimg->width);
	if (img) {
		memcpy(img->base, fimg->base, img->height * img->width * sizeof(RGB));
	}
	image_destroy(fimg);

	return 1;
}

static int limage_sift_match(lua_State *L) {
	IMAGE *a = lua_check_image(L, 1);
	IMAGE *b = lua_check_image(L, 2);
	sift_match(a, b);
	return 0;
}

static int limage_likeness(lua_State *L) {
	IMAGE *a = lua_check_image(L, 1);
	IMAGE *b = lua_check_image(L, 2);
	int debug = luaL_optinteger (L, 3, 1);
	double d = image_likeness(a, b, debug);
	lua_pushnumber(L, d);
	return 1;
}

static int limage_phash_hamming(lua_State *L) {
	IMAGE *a = lua_check_image(L, 1);
	IMAGE *b = lua_check_image(L, 2);
	int debug = luaL_optinteger (L, 3, 1);

	PHASH f1 = phash_image(a, debug);
	PHASH f2 = phash_image(b, debug);

	int d = phash_hamming(f1, f2);
	printf("PHash Hamming Distance: %d\n", d);

	return 0;
}

static const struct luaL_Reg image_lib_f[] = {
	{ "help", limage_help },
	{ "create", limage_new },
	{ "load", limage_load },
	{ "match", limage_sift_match },
	// Likeness
	{ "likeness", limage_likeness },

	// phash Likeness
	{ "phash_hamming", limage_phash_hamming },
	{ NULL, NULL }
};

static int image2string (lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_pushfstring(L, "[image size of %dx%d]", img->height, img->width);
	return 1;
}

static int limage_show(lua_State *L)
{
	IMAGE *img = lua_check_image(L, 1);
	image_show("Image", img);
	return 0;
}

static int limage_get_rows(lua_State *L)
{
	IMAGE *img = lua_check_image(L, 1);
	lua_pushnumber(L, img->height);
	return 1;
}

static int limage_get_cols(lua_State *L)
{
	IMAGE *img = lua_check_image(L, 1);
	lua_pushnumber(L, img->width);
	return 1;
}

static int limage_get_element(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer i = luaL_checkinteger(L, 2);
	lua_Integer j = luaL_checkinteger(L, 3);
	luaL_argcheck(L, 1 <= i && i <= img->height, 2, "row out of range");
	luaL_argcheck(L, 1 <= j && j <= img->height, 3, "col out of range");
	int color = RGB_RGB(img->ie[i - 1][j - 1].r, img->ie[i - 1][j - 1].g, img->ie[i - 1][j - 1].b);
	lua_pushnumber(L, color);
	return 1;
}

// image:set(r, c, color)
static int limage_set_element(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer i = luaL_checkinteger(L, 2);
	lua_Integer j = luaL_checkinteger(L, 3);
	int color = luaL_checkinteger(L, 4);
	
	luaL_argcheck(L, 1 <= i && i <= img->height, 2, "row out of range");
	luaL_argcheck(L, 1 <= j && j <= img->height, 3, "col out of range");

	img->ie[i - 1][j - 1].r = RGB_R(color);
	img->ie[i - 1][j - 1].g = RGB_G(color);
	img->ie[i - 1][j - 1].b = RGB_B(color);
	
	return 0;
}

static int limage_copy(lua_State *L)
{
	IMAGE *src = lua_check_image(L, 1);
	IMAGE *dst = lua_image_create(L, src->height, src->width);
	if (dst) {
		memcpy(dst->base, src->base, src->height * src->width * sizeof(RGB));
	}
	return 1;
}

static int limage_subimg(lua_State *L)
{
	int i;
	RECT rect;
	IMAGE *src = lua_check_image(L, 1);
	rect.r = luaL_checkinteger(L, 2);	// Start row
	rect.c = luaL_checkinteger(L, 3);	// Start col
	rect.h = luaL_checkinteger(L, 4);	// Heigh
	rect.w = luaL_checkinteger(L, 5);	// Width
	image_rectclamp(src, &rect);

	IMAGE *sub = lua_image_create(L, rect.h, rect.w);
	if (sub) {
		for (i = 0; i < rect.h; i++)
			memcpy(&(sub->ie[i][0]), &(src->ie[i + rect.r][rect.c]), sizeof(RGB)*rect.w);
	}
	return 1;
}

static int limage_zoom(lua_State *L)
{
	IMAGE *dst = NULL;
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer nh = luaL_checkinteger(L, 2);
	lua_Integer nw = luaL_checkinteger(L, 3);
	lua_Integer method = luaL_optinteger (L, 4, 1);	// Default bline
	IMAGE *src = image_zoom(img,nh, nw,method);
	if (src) {
		dst = lua_image_create(L, src->height, src->width);
		memcpy(dst->base, src->base, src->height * src->width * sizeof(RGB));
		image_destroy(src);
	}
	return 1;
}

static int limage_save(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	const char* fname = lua_tostring(L, 2);
	int ret = image_save(img, fname);
	lua_pushnumber(L, ret);
	return 1;
}

static int limage_voice(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer rows = luaL_checkinteger(L, 2);
	lua_Integer cols = luaL_checkinteger(L, 3);
	lua_Integer levs = luaL_checkinteger(L, 4);
	lua_Integer bin = luaL_optinteger(L, 5, 1);	// Default bin = 1
	VECTOR *vec = image_voice(img, rows, cols, levs, bin);
	lua_save_vector(L, vec);
	vector_destroy(vec);

	return 1;
}

static int limage_voice_show(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer rows = luaL_checkinteger(L, 2);
	lua_Integer cols = luaL_checkinteger(L, 3);
	lua_Integer levs = luaL_checkinteger(L, 4);
	
	lua_Integer number = luaL_optinteger(L, 5, 0);	// Default bin = 1
	image_voice_show(img, rows, cols, levs, number);
	return 0;
}

static int limage_entropy(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer rows = luaL_checkinteger(L, 2);
	lua_Integer cols = luaL_checkinteger(L, 3);
	lua_Integer levs = luaL_checkinteger(L, 4);
	lua_Integer debug = luaL_optinteger(L, 5, 0);	// Default debug = 0
	
	MATRIX *mat = image_entropy(img, rows, cols, levs, debug);
	lua_save_matrix(L, mat);
	matrix_destroy(mat);

	return 1;
}

static int limage_object_detect(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer levs = luaL_optinteger(L, 2, 16);
	lua_Integer debug = luaL_optinteger(L, 3, 1);	// Default debug = 1

	object_detect(img, levs);

	if (debug)
		image_drawrects(img);

	lua_save_rectset(L);
	return 1;
}


static int limage_fast_detect(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer debug = luaL_optinteger(L, 2, 1);	// Default debug = 1
	object_fast_detect(img);

	if (debug)
		image_drawrects(img);

	lua_save_rectset(L);
	return 1;
}

static int limage_face_detect(lua_State *L) {
	int i;
	double d = 0;
	RECTS *mrs = rect_set();
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer levs = luaL_optinteger(L, 2, 16);
	lua_Integer debug = luaL_optinteger(L, 3, 1);	// Default debug = 1

	object_detect(img, levs);

	for (i = 0; i < mrs->count; i++) {
		if (mrs->rect[i].h < 32  || mrs->rect[i].w < 32) {
			rect_delete(i); --i; 
			continue;
		}
		d = skin_statics(img, &mrs->rect[i]);
		d /= mrs->rect[i].h * mrs->rect[i].w;
		if (d < 0.3) { 	// 4/9 == 0.4444
			rect_delete(i);
			// 	Delete non-face !!!
		}
	}
	
	// xxxx9999
#if 0	
	int r,c, h, w;
 	int n = mrs->count;
	RECT nr;
	for (i = 0; i < n; i++) {
		nr = mrs->rect[i];
		h = nr.h * 8/100;
		w = nr.w * 8/100;	// 5*8% = 40%

		for (r = -2; r < 2; r++) {
			for (c = -2; c < 2; c++) {
				nr.r = r*h + mrs->rect[i].r;
				nr.c = c*w + mrs->rect[i].c;

				image_rectclamp(img, &nr);
				rect_put(&nr);
			}
		}
	}
#endif	
	
	if (debug)
		image_drawrects(img);

	lua_save_rectset(L);
	return 1;
}


static int limage_draw_box(lua_State *L) {
	RECT rect;
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer top = luaL_checkinteger(L, 2);
	lua_Integer left = luaL_checkinteger(L, 3);
	lua_Integer bottom = luaL_checkinteger(L, 4);
	lua_Integer right = luaL_checkinteger(L, 5);
	lua_Integer color = luaL_optinteger(L, 6, 0xffffff);
	lua_Integer fill = luaL_optinteger(L, 7, 0);

	rect.r = top;
	rect.c = left;
	rect.h = bottom - top + 1;
	rect.w = right - left + 1;
	image_drawrect(img, &rect, color, fill);
	return 0;
}

static int limage_draw_line(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer r1 = luaL_checkinteger(L, 2);
	lua_Integer c1 = luaL_checkinteger(L, 3);
	lua_Integer r2 = luaL_checkinteger(L, 4);
	lua_Integer c2 = luaL_checkinteger(L, 5);
	lua_Integer color = luaL_optinteger(L, 6, 0xffffff);

	image_drawline(img, r1, c1, r2, c2, color);
	return 0;
}

static int limage_line_detect(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	int debug = luaL_optinteger (L, 2, 1);
	line_detect(img, debug);
	return 0;
}

static int limage_draw_text(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer r = luaL_checkinteger(L, 2);
	lua_Integer c = luaL_checkinteger(L, 3);
	const char *text = lua_tostring(L, 4);
	lua_Integer color = luaL_optinteger(L, 5, 0xffffff);

	image_drawtext(img, r, c, (char *)text, color);
	return 0;
}

static int limage_paste_image(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer r = luaL_checkinteger(L, 2);
	lua_Integer c = luaL_checkinteger(L, 3);
	IMAGE *small = lua_check_image(L, 4);
	lua_Number alpha = luaL_optnumber(L, 5, 0.50f);
	image_paste(img, r, c, small, alpha);
	return 0;
}

static int limage_make_noise(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer rate = luaL_optinteger(L, 2, 20);	// Default 20%
	image_make_noise(img, 'A', rate);
	return 0;
}

static int limage_delete_noise(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	image_delete_noise(img);
	return 0;
}

static int limage_gauss_filter(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Number sigma = luaL_optnumber(L, 2, 1.0f);
	image_gauss_filter(img, sigma);
	return 0;
}

static int limage_bilate_filter(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Number hs = luaL_optnumber(L, 2, 1.0f);
	lua_Number hr = luaL_optnumber(L, 3, 1.0f);
	image_bilate_filter(img, hs, hr);
	return 0;
}

static int limage_guided_filter(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	IMAGE *guidance = lua_check_image(L, 2);
	lua_Integer radius = luaL_optinteger(L, 3, 5);
	lua_Number eps = luaL_optnumber(L, 4, 0.01f);	// 0.1
	lua_Integer scale = luaL_optinteger(L, 5, 1);

	image_guided_filter(img, guidance, radius, eps, scale, 1);
	return 0;
}

static int limage_beeps_filter(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Number stdv = luaL_optnumber(L, 2, 20.0f);
	lua_Number dec = luaL_optnumber(L, 3, 0.1f);

	image_beeps_filter(img, stdv, dec, 1);
	return 0;
}

static int limage_dehaze_filter(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer radius = luaL_optinteger(L, 2, 5);
	image_dehaze_filter(img, radius, 1); // Debug = 1
	return 0;
}

static int limage_lee_filter(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Number radius = luaL_optnumber(L, 2, 5);
	lua_Number eps = luaL_optnumber(L, 3, 0.5f);

	image_lee_filter(img, radius, eps, 1);
	
	return 0;
}

static int limage_shape_morph(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	const char *action = lua_tostring(L, 2);
	image_morph(img, *action);
	return 0;
}

// Color
static int limage_color_togray(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	color_togray(img);
	return 0;
}

static int limage_color_torgb565(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	color_torgb565(img);
	return 0;
}

static int limage_3x3encode(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	image_3x3enc(img);
	return 0;
}

static int limage_3x3decode(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	image_3x3dec(img);
	return 0;
}

static int limage_color_vector(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer ndim = luaL_optinteger(L, 2, DEFAULT_COLOR_VECTOR_DIMENSION);
	VECTOR *vec = color_vector(img, NULL, ndim);
	lua_save_vector(L, vec);
	vector_destroy(vec);

	return 1;
}

static int limage_color_entropy(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer ndim = luaL_optinteger(L, 2, DEFAULT_COLOR_VECTOR_DIMENSION);
	double d = color_entropy(img, NULL, ndim);
	lua_pushnumber(L, d);
	return 1;
}

static int limage_color_skindetect(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	skin_detect(img);
	return 0;
}

static int limage_color_cluster(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer num = luaL_optinteger(L, 2, DEFAULT_COLOR_VECTOR_DIMENSION);
	color_cluster(img, num, 1);
	return 0;
}

static int limage_color_balance(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer method = luaL_optinteger(L, 2, 1);	// full reflect 

	color_balance(img, method, 1);
	return 0;
}


// Shape
static int limage_shape_vector(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer ndim = luaL_optinteger(L, 2, DEFAULT_SHAPE_VECTOR_DIMENSION);
	VECTOR *vec = shape_vector(img, NULL, ndim);
	lua_save_vector(L, vec);
	vector_destroy(vec);

	return 1;
}

static int limage_shape_entropy(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer ndim = luaL_optinteger(L, 2, DEFAULT_SHAPE_VECTOR_DIMENSION);
	double d = shape_entropy(img, NULL, ndim);
	lua_pushnumber(L, d);
	return 1;
}

static int limage_shape_contour(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	image_contour(img);
	return 0;
}

static int limage_shape_skeleton(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	image_skeleton(img);
	return 0;
}

static int limage_shape_midedge(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	shape_midedge(img);
	return 0;
}

static int limage_shape_bestedge(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	shape_bestedge(img);
	return 0;
}

static int limage_shape_likeness(lua_State *L) {
	IMAGE *a = lua_check_image(L, 1);
	IMAGE *b = lua_check_image(L, 2);
	lua_Integer ndim = luaL_optinteger(L, 3, 36);
	double d = shape_likeness(a, b, NULL, ndim);
	lua_pushnumber(L, d);
	return 1;
}

// Texture
static int limage_texture_vector(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer ndim = luaL_optinteger(L, 2, DEFAULT_TEXTURE_VECTOR_DIMENSION);
	VECTOR *vec = texture_vector(img, NULL, ndim);
	lua_save_vector(L, vec);
	vector_destroy(vec);

	return 1;
}

static int limage_texture_entropy(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer ndim = luaL_optinteger(L, 2, DEFAULT_TEXTURE_VECTOR_DIMENSION);
	double d = texture_entropy(img, NULL, ndim);
	lua_pushnumber(L, d);
	return 1;
}

static int limage_blend_cluster(lua_State *L)
{
	IMAGE *dst = lua_check_image(L, 1);

	IMAGE *src = lua_check_image(L, 2);
	IMAGE *mask = lua_check_image(L, 3);

	lua_Integer top = luaL_optinteger(L, 4, dst->height/2);
	lua_Integer left = luaL_optinteger(L, 5, dst->width/2);
	lua_Integer debug = luaL_optinteger(L, 6, 0);

	blend_cluster(src, mask, dst, top, left, debug);
	return 0;
}

static int limage_blend_multiband(lua_State *L)
{
#if 0
	IMAGE *a = lua_check_image(L, 1);
	IMAGE *b = lua_check_image(L, 2);
	IMAGE *mask = lua_check_image(L, 3);
	lua_Integer layers = luaL_optinteger(L, 4, 6);

	IMAGE *c = blend_mutiband(a, b, mask, layers, 1);
	// IMAGE *c = blend_3x3codec(a, b, mask, 1);
	image_show("Multi band blending", c);
#else
	IMAGE *src = lua_check_image(L, 1);

	sview_start("camera.model");
	sview_blend(src, 0, 1);
	sview_stop();
#endif
	
	return 0;
}

static int limage_defisheye(lua_State *L)
{
	IMAGE *dst = NULL;
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer strength = luaL_checkinteger(L, 2);
	lua_Number zoom = luaL_checknumber(L, 3);

	IMAGE *src = image_defisheye(img, strength, zoom);
	if (src) {
		dst = lua_image_create(L, src->height, src->width);
		memcpy(dst->base, src->base, src->height * src->width * sizeof(RGB));
		image_destroy(src);
	}
	return 1;
}

static int limage_gauss3x3_filter(lua_State *L)
{
	RECT rect;
	IMAGE *img = lua_check_image(L, 1);
	image_rect(&rect, img);
	image_gauss3x3_filter(img, &rect);
	return 0;
}

static int limage_retinex(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer nscale = luaL_optinteger(L, 2, 3);
	image_retinex(img, nscale);
	return 0;
}

static int limage_digest(lua_State *L) {
	IMAGE *img = lua_check_image(L, 1);
	lua_Integer debug = luaL_optinteger(L, 2, 1); // Debug = 1
	PHASH phash = phash_image(img, debug);
	printf("0x%lx\n", phash);
	return 0;
}


static const struct luaL_Reg image_lib_m[] = {
	{ "__tostring", image2string },
	// Basic
	{ "show", limage_show },
	{ "rows", limage_get_rows },
	{ "cols", limage_get_cols },
	{ "get", limage_get_element },
	{ "set", limage_set_element },
	{ "copy", limage_copy },
	{ "zoom", limage_zoom },
	{ "sub", limage_subimg },
	{ "save", limage_save },
	
	// Color convert
	{ "gray", limage_color_togray },
	{ "rgb565", limage_color_torgb565 },
	{ "enc3x3", limage_3x3encode },
	{ "dec3x3", limage_3x3decode },

	// Voice
	{ "voice", limage_voice },
	{ "voice_show", limage_voice_show },
	{ "entropy", limage_entropy },

	// Object Detect System
	{ "object_detect", limage_object_detect },
	{ "face_detect", limage_face_detect },
	{ "line_detect", limage_line_detect },
	{ "fast_detect", limage_fast_detect },

	// Draw ...
	{ "draw_box", limage_draw_box },
	{ "draw_line", limage_draw_line },
	{ "draw_text", limage_draw_text },

	// Filters
	{ "make_noise", limage_make_noise },
	{ "delete_noise", limage_delete_noise },
	{ "gauss_filter", limage_gauss_filter },
	{ "bilate_filter", limage_bilate_filter },
	{ "guided_filter", limage_guided_filter },
	{ "beeps_filter", limage_beeps_filter },
	{ "dehaze_filter", limage_dehaze_filter },
	{ "lee_filter", limage_lee_filter },

	// Paste Image
	{ "paste_image", limage_paste_image },

	// Color
	{ "color_vector", limage_color_vector },
	{ "color_entropy", limage_color_entropy },
	{ "color_cluster", limage_color_cluster },
	{ "color_balance", limage_color_balance },
	{ "skin_detect", limage_color_skindetect },

	// Shape
	{ "shape_vector", limage_shape_vector },
	{ "shape_entropy", limage_shape_entropy },
	{ "shape_likeness", limage_shape_likeness },
	{ "shape_contour", limage_shape_contour},
	{ "shape_skeleton", limage_shape_skeleton },
	{ "shape_midedge", limage_shape_midedge },
	{ "shape_bestedge", limage_shape_bestedge },
	{ "shape_morph", limage_shape_morph },

	// Texture
	{ "texture_vector", limage_texture_vector },
	{ "texture_entropy", limage_texture_entropy },

	{ "blend_cluster", limage_blend_cluster },
	{ "blend_multiband", limage_blend_multiband },
	{ "defisheye", limage_defisheye },

	{ "gauss3x3", limage_gauss3x3_filter},

	// Image enhancement
	{ "retinex", limage_retinex },

	// Perceptual Hash
	{ "digest", limage_digest },

	{ NULL, NULL }
};

static void setup_copyright(lua_State *L) {
	lua_pushliteral(L, "Lua Image Module(1.0.1), 2008-2017, Dell Du.");
	lua_setfield(L, -2, "copyright");
}

int luaopen_libvision_image(lua_State *L) {
	srand(time(NULL));	

	// Register meta table
	luaL_newmetatable(L, IMAGE_META_NAME);

	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2); /* pushes the metatable, copy element -2 to stack top ==> meta_table*/
	lua_settable(L, -3); /* metatable.__index = metatable */

	luaL_openlib(L, NULL, image_lib_m, 0);
	luaL_openlib(L, "vision.image", image_lib_f, 0);

	setup_copyright(L);
	
	return 1;
}


