
/************************************************************************************
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include "video.h"
#include "image.h"
#include "frame.h"
#include "sview.h"
#include "plane.h"
#include "color.h"

static double __math_snr(int m, char *orig, char *now)
{
	int i, k;
	long long s = 0, n = 0;			// signal/noise
	double d;

	for (i = 0; i < m; i++)  {
		s += orig[i] * orig[i];
		k = orig[i] - now[i];
		n += k * k;
	}
	
	if (n == 0)	// force !!!
		n = 1;
	d = s/n;
	
	return 10.0f * log(d);
}

static int __simple_match(char *buf, char *pattern)
{
	return strncmp(buf, pattern, strlen(pattern)) == 0;
}

/*
width=384
height=288
pix_fmt=yuv420p
avg_frame_rate=25/1
*/
static int __video_probe(char *filename, VIDEO *v)
{
	FILE *fp;
	char cmd[256], buf[256];

	snprintf(cmd, sizeof(cmd) - 1, "ffprobe -show_streams %s 2>/dev/null", filename);
	fp = popen(cmd, "r");
	if (! fp) {
		syslog_error("Run %s", cmd);
		return RET_ERROR;
	}
	
	while(fgets(buf, sizeof(buf) - 1, fp)) {
		if (strlen(buf) > 0)
			buf[strlen(buf) - 1] = '\0';
		if (__simple_match(buf,  "width="))
			v->width = atoi(buf + sizeof("width=") - 1);
		else if (__simple_match(buf,  "height="))
			v->height = atoi(buf + sizeof("height=") - 1);
		else if (__simple_match(buf,  "pix_fmt="))
			v->format = frame_format(buf + sizeof("pix_fmt=") - 1);
		else if (__simple_match(buf,  "avg_frame_rate="))
			v->frame_speed = atoi(buf + sizeof("avg_frame_rate=") -1);
	}
	pclose(fp);

	return RET_OK;
}

static VIDEO *__yuv420_open(char *filename, int width, int height)
{
	int i;
	FILE *fp;
	VIDEO *v = NULL;

	// Allocate video 
	v = (VIDEO *)calloc((size_t)1, sizeof(VIDEO)); 
	if (! v) { 
		syslog_error("Allocate memeory.");
		return NULL; 
	}
	v->frame_speed = 25;
	v->format = MAKE_FOURCC('4','2','0','P');
	v->width = width;
	v->height = height;
	
	if ((fp = fopen(filename, "r")) == NULL) {
		syslog_error("Open %s\n", filename);
		return NULL; 
	}
	v->_fp = fp;

	for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
		v->frames[i] = frame_create(v->format, v->width, v->height);
		if (! v->frames[i]) {
			syslog_error("Allocate memory");
			return NULL;
		}
	}

	v->frame_size = frame_size(v->format, v->width, v->height);
	// Allocate memory
	for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
		v->_frame_buffer[i].startmap = (BYTE *)calloc((size_t)v->frame_size, sizeof(BYTE));
		if (v->_frame_buffer[i].startmap == NULL) {
			syslog_error("Allocate memory");
			goto fail;
		}
		else
			frame_binding(v->frames[i], (BYTE *)v->_frame_buffer[i].startmap);
	}

	v->frame_index = 0;

	v->magic = VIDEO_MAGIC;

	return v;
fail:
	video_close(v);
	
	return NULL;
}


#ifdef CONFIG_SDL
int model_match(IMAGE *img, PHASH model, double threshold)
{
	int i, match_index;
	double t, likeness;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();

	phash_objects(img);

	match_index = -1;
	likeness = 0.0f;
	for (i = 0; i < mrs->count; i++) {
		if (mrs->rect[i].h < 2*8 || mrs->rect[i].w < 2*8) {
			mrs->rect[i].h = mrs->rect[i].w = 0;
			continue;
		}
		t = phash_likeness(mps->phash[i], model);
		if (t > likeness) {
			likeness = t;
			match_index = i;
		}
	}

	if (match_index >= 0 && likeness > threshold) {
		image_drawrect(img, &(mrs->rect[match_index]), color_picker(), 0);
	}

	return RET_OK;
}


#include <SDL/SDL.h>
// File surface -- RGB 24
static void __fill_surface(SDL_Surface *rgb24, IMAGE *image)
{
	int i, j;
	int bpp = rgb24->format->BytesPerPixel;
	BYTE *p;

	image_foreach(image, i, j) {
		p = (BYTE *)rgb24->pixels + i * rgb24->pitch + j * bpp;
		p[0] = image->ie[i][j].r; p[1] = image->ie[i][j].g; p[2] = image->ie[i][j].b;
//		p[0] = image->ie[i][j].b; p[1] = image->ie[i][j].g; p[2] = image->ie[i][j].r;
	}
}

static int __video_play(VIDEO * video)
{
	int b_pause = 0;
	int b_fullscreen = 0;
	int b_quit = 0;
	int i_wait;
	
	SDL_Event event;
	int64_t i_start;
	SDL_Surface *rgbsurface, *screen;
	Uint32 rmask, gmask, bmask, amask;
	char title[256];
	IMAGE *image;
	FRAME *fg;

	/* Open SDL */
	if (SDL_Init(SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
		syslog_error("Init SDL.");
		return RET_ERROR;
	}

	/* Clean up on exit */
	// atexit(SDL_Quit) is evil !!!;
	SDL_Quit();
	SDL_WM_SetCaption("VIDEO", NULL);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen = SDL_SetVideoMode(video->width, video->height, 24,
	                     SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
	if (screen == NULL) {
		syslog_error("SDL_SetVideoMode, %s.", SDL_GetError());
		return RET_ERROR;
	}
	image = image_create(video->height, video->width);	check_image(image);
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x00000000; // 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0x00000000; // 0xff000000;
#endif
	rgbsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, video->width, video->height, 24, rmask, gmask, bmask, amask);
	if(rgbsurface == NULL) {
		syslog_error("Create RGBSurface, %s. ", SDL_GetError());
		return RET_ERROR;
	}

	if (video->frame_speed == 0)
		video->frame_speed = 25;

	while (! b_quit && ! feof(video->_fp)) {
		i_start = SDL_GetTicks();

		if (! b_pause) {
			fg = video_read(video);
			frame_toimage(fg, image);
			snprintf(title, sizeof(title) - 1, "Frame:%03d", video->frame_index);
			SDL_WM_SetCaption(title, NULL);
			// color_balance(image, 1, 0);
			// color_togray(image);
			__fill_surface(rgbsurface, image);
			SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
			SDL_UpdateRect(screen, 0, 0, video->width, video->height);
		}

		// Process events
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				if (b_fullscreen)
					SDL_WM_ToggleFullScreen(screen);
				b_quit = 1;
				break;
					
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
					if (b_fullscreen)
						SDL_WM_ToggleFullScreen(screen);
					b_quit = 1;
					break;

				case SDLK_s:
					{
						char filename[FILENAME_MAXLEN];
						if (image_valid(image)) {
 							snprintf(filename, sizeof(filename) - 1, "video_%d.jpg", video->frame_index);
							image_save(image, filename);
						}
					}
					break;
				case SDLK_f:
					if (SDL_WM_ToggleFullScreen(screen))
						b_fullscreen = 1 - b_fullscreen;
					break;

				case SDLK_SPACE:
					b_pause = (b_pause) ? 0 : 1;
					break;

				default:
					break;
				}
				break;
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 24,
					SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
				SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
				SDL_UpdateRect(screen, 0, 0, video->width, video->height);
				break;

			default:
				break;
			}
		}
			

		/* wait */
		i_wait = 1000 / video->frame_speed - (SDL_GetTicks() - i_start);
		if (i_wait > 0)
			SDL_Delay(i_wait);
	}

	image_destroy(image);

	SDL_FreeSurface(rgbsurface);

	return RET_OK;
}

static int __video_mds(VIDEO * video)
{
	int b_pause = 0;
	int b_fullscreen = 0;
	int b_quit = 0;
	int i_wait;
	
	SDL_Event event;
	int64_t i_start;
	SDL_Surface *rgbsurface, *screen;
	Uint32 rmask, gmask, bmask, amask;
	char title[256];
	IMAGE *A, *B, *C, *bg;

	/* Open SDL */
	if (SDL_Init(SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
		syslog_error("Init SDL.");
		return RET_ERROR;
	}

	/* Clean up on exit */
	// atexit(SDL_Quit) is evil !!!;
	SDL_Quit();
	SDL_WM_SetCaption("VIDEO", NULL);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen = SDL_SetVideoMode(video->width, video->height, 24,
	                     SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
	if (screen == NULL) {
		syslog_error("SDL_SetVideoMode, %s.", SDL_GetError());
		return RET_ERROR;
	}

	A = image_create(video->height, video->width); check_image(A);
	B = image_create(video->height, video->width); check_image(B);
	C = image_create(video->height, video->width); check_image(C);
	bg = image_create(video->height, video->width); check_image(bg);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x00000000; // 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0x00000000; // 0xff000000;
#endif
	rgbsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, video->width, video->height, 24, rmask, gmask, bmask, amask);
	if(rgbsurface == NULL) {
		syslog_error("Create RGBSurface, %s. ", SDL_GetError());
		return RET_ERROR;
	}

	if (video->frame_speed == 0)
		video->frame_speed = 25;

	frame_toimage(video_read(video), bg);
	frame_toimage(video_read(video), A);
	frame_toimage(video_read(video), B);

	while (! b_quit && ! feof(video->_fp)) {
		i_start = SDL_GetTicks();

		if (! b_pause) {
			frame_toimage(video_read(video), C);

			motion_updatebg(A, B, C, bg);
			memcpy(A->base, B->base, B->height * B->width * sizeof(RGB));			// A = B
			memcpy(B->base, C->base, C->height * C->width * sizeof(RGB));			// B = C
			
			motion_detect(C, bg, 1);
//			object_fast_detect(C);
//			image_drawrects(C);

			snprintf(title, sizeof(title) - 1, "Frame:%03d", video->frame_index);
			SDL_WM_SetCaption(title, NULL);
			__fill_surface(rgbsurface, C);
			SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
			SDL_UpdateRect(screen, 0, 0, video->width, video->height);
		}

		// Process events
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				if (b_fullscreen)
					SDL_WM_ToggleFullScreen(screen);
				b_quit = 1;
				break;
					
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
					if (b_fullscreen)
						SDL_WM_ToggleFullScreen(screen);
					b_quit = 1;
					break;

				case SDLK_s:
					{
						char filename[FILENAME_MAXLEN];
						if (image_valid(C)) {
 							snprintf(filename, sizeof(filename) - 1, "mds_%d.jpg", video->frame_index);
							image_save(C, filename);
						}
					}
					break;
				case SDLK_f:
					if (SDL_WM_ToggleFullScreen(screen))
						b_fullscreen = 1 - b_fullscreen;
					break;

				case SDLK_SPACE:
					b_pause = (b_pause) ? 0 : 1;
					break;

				default:
					break;
				}
				break;
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 24,
					SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
				SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
				SDL_UpdateRect(screen, 0, 0, video->width, video->height);
				break;

			default:
				break;
			}
		}
			
		/* wait */
		i_wait = 1000 / video->frame_speed - (SDL_GetTicks() - i_start);
		if (i_wait > 0)
			SDL_Delay(i_wait);
	}
	image_destroy(C);
	image_destroy(B);
	image_destroy(A);
	image_destroy(bg);

	SDL_FreeSurface(rgbsurface);

	return RET_OK;
}

static int __video_pds(VIDEO * video)
{
	int b_pause = 0;
	int b_fullscreen = 0;
	int b_quit = 0;
	int i_wait;
	
	SDL_Event event;
	int64_t i_start, i_begin, i_count;
	SDL_Surface *rgbsurface, *screen;
	Uint32 rmask, gmask, bmask, amask;
	char title[256];
	IMAGE *A, *B, *C, *bg;
	PLANE plane;

	/* Open SDL */
	if (SDL_Init(SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
		syslog_error("Init SDL.");
		return RET_ERROR;
	}

	/* Clean up on exit */
	// atexit(SDL_Quit) is evil !!!;
	SDL_Quit();
	SDL_WM_SetCaption("VIDEO", NULL);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen = SDL_SetVideoMode(video->width, video->height, 24,
	                     SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
	if (screen == NULL) {
		syslog_error("SDL_SetVideoMode, %s.", SDL_GetError());
		return RET_ERROR;
	}

	A = image_create(video->height, video->width); check_image(A);
	B = image_create(video->height, video->width); check_image(B);
	C = image_create(video->height, video->width); check_image(C);
	bg = image_create(video->height, video->width); check_image(bg);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x00000000; // 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0x00000000; // 0xff000000;
#endif
	rgbsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, video->width, video->height, 24, rmask, gmask, bmask, amask);
	if(rgbsurface == NULL) {
		syslog_error("Create RGBSurface, %s. ", SDL_GetError());
		return RET_ERROR;
	}

	if (video->frame_speed == 0)
		video->frame_speed = 25;

	frame_toimage(video_read(video), bg);
	frame_toimage(video_read(video), A);
	frame_toimage(video_read(video), B);

	i_begin = SDL_GetTicks();
	i_count = 0;
	plane_reset(&plane);
	
	while (! b_quit && ! feof(video->_fp)) {
		i_start = SDL_GetTicks();

		if (! b_pause) {
			frame_toimage(video_read(video), C);

			motion_updatebg(A, B, C, bg);
			memcpy(A->base, B->base, B->height * B->width * sizeof(RGB));			// A = B
			memcpy(B->base, C->base, C->height * C->width * sizeof(RGB));			// B = C
			
			// motion_detect(C, bg, 1);
			plane_detect(&plane, C);
//			object_fast_detect(C);
//			image_drawrects(C);
			i_count++;
			snprintf(title, sizeof(title), "fps:%.2f/%d", 1000.0f*i_count/(SDL_GetTicks() - i_begin), video->frame_index); 

			SDL_WM_SetCaption(title, NULL);
			__fill_surface(rgbsurface, C);
			SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
			SDL_UpdateRect(screen, 0, 0, video->width, video->height);

			snprintf(title, sizeof(title) - 1, "output/plane_%d.jpg", video->frame_index);
			image_save(C, title);
		}

		// Process events
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				if (b_fullscreen)
					SDL_WM_ToggleFullScreen(screen);
				b_quit = 1;
				break;
					
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
					if (b_fullscreen)
						SDL_WM_ToggleFullScreen(screen);
					b_quit = 1;
					break;

				case SDLK_s:
					{
						char filename[FILENAME_MAXLEN];
						if (image_valid(C)) {
 							snprintf(filename, sizeof(filename) - 1, "mds_%d.jpg", video->frame_index);
							image_save(C, filename);
						}
					}
					break;
				case SDLK_f:
					if (SDL_WM_ToggleFullScreen(screen))
						b_fullscreen = 1 - b_fullscreen;
					break;

				case SDLK_SPACE:
					b_pause = (b_pause) ? 0 : 1;
					break;

				default:
					break;
				}
				break;
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 24,
					SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
				SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
				SDL_UpdateRect(screen, 0, 0, video->width, video->height);
				break;

			default:
				break;
			}
		}
			
		/* wait */
		i_wait = 1000 / video->frame_speed - (SDL_GetTicks() - i_start);
		if (i_wait > 0)
			SDL_Delay(i_wait);
	}
	image_destroy(C);
	image_destroy(B);
	image_destroy(A);
	image_destroy(bg);

	SDL_FreeSurface(rgbsurface);

	return RET_OK;
}


static int __video_compress(VIDEO * video, double threshold)
{
	int b_pause = 0;
	int b_fullscreen = 0;
	int b_quit = 0;

	SDL_Event event;
	SDL_Surface *rgbsurface, *screen;
	Uint32 rmask, gmask, bmask, amask;
	char title[256];
	IMAGE *A, *B, *C, *bg;
	double ratio;
	int reindex = 0;

	/* Open SDL */
	if (SDL_Init(SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
		syslog_error("Init SDL.");
		return RET_ERROR;
	}

	/* Clean up on exit */
	// atexit(SDL_Quit) is evil !!!;
	SDL_Quit();
	SDL_WM_SetCaption("VIDEO", NULL);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen = SDL_SetVideoMode(video->width, video->height, 24,
	                     SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
	if (screen == NULL) {
		syslog_error("SDL_SetVideoMode, %s.", SDL_GetError());
		return RET_ERROR;
	}

	A = image_create(video->height, video->width); check_image(A);
	B = image_create(video->height, video->width); check_image(B);
	C = image_create(video->height, video->width); check_image(C);
	bg = image_create(video->height, video->width); check_image(bg);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x00000000; // 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0x00000000; // 0xff000000;
#endif
	rgbsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, video->width, video->height, 24, rmask, gmask, bmask, amask);
	if(rgbsurface == NULL) {
		syslog_error("Create RGBSurface, %s. ", SDL_GetError());
		return RET_ERROR;
	}

	if (video->frame_speed == 0)
		video->frame_speed = 25;

	frame_toimage(video_read(video), bg);
	frame_toimage(video_read(video), A);
	frame_toimage(video_read(video), B);

	while (! b_quit && ! feof(video->_fp)) {
		if (! b_pause) {
			frame_toimage(video_read(video), C);
			motion_updatebg(A, B, C, bg);
			memcpy(A->base, B->base, B->height * B->width * sizeof(RGB));			// A = B
			memcpy(B->base, C->base, C->height * C->width * sizeof(RGB));			// B = C
			
			motion_detect(C, bg, 0);
			ratio = motion_ratio(C);

			if (ratio > threshold) {
				reindex++;
				snprintf(title, sizeof(title) - 1, "output/image_%d.jpg", reindex);
				image_save(C, title);
				image_drawrects(C);
			}
			snprintf(title, sizeof(title) - 1, "Frame:%03d", video->frame_index);

			SDL_WM_SetCaption(title, NULL);
			__fill_surface(rgbsurface, C);
			SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
			SDL_UpdateRect(screen, 0, 0, video->width, video->height);
		}

		// Process events
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				if (b_fullscreen)
					SDL_WM_ToggleFullScreen(screen);
				b_quit = 1;
				break;
					
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
					if (b_fullscreen)
						SDL_WM_ToggleFullScreen(screen);
					b_quit = 1;
					break;

				case SDLK_s:
					{
						char filename[FILENAME_MAXLEN];
						if (image_valid(C)) {
 							snprintf(filename, sizeof(filename) - 1, "uniq_%d.jpg", video->frame_index);
							image_save(C, filename);
						}
					}
					break;
				case SDLK_f:
					if (SDL_WM_ToggleFullScreen(screen))
						b_fullscreen = 1 - b_fullscreen;
					break;

				case SDLK_SPACE:
					b_pause = (b_pause) ? 0 : 1;
					break;

				default:
					break;
				}
				break;
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 24,
					SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
				SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
				SDL_UpdateRect(screen, 0, 0, video->width, video->height);
				break;

			default:
				break;
			}
		}
	}
	image_destroy(C);
	image_destroy(B);
	image_destroy(A);
	image_destroy(bg);

	SDL_FreeSurface(rgbsurface);

	return RET_OK;
}


static int __video_match(VIDEO * video, IMAGE *model, double threshold)
{
	int b_pause = 0;
	int b_fullscreen = 0;
	int b_quit = 0;

	SDL_Event event;
	SDL_Surface *rgbsurface, *screen;
	Uint32 rmask, gmask, bmask, amask;
	int64_t i_begin, i_count;
	char title[256];
	IMAGE *C;
	PHASH model_hash;

	check_image(model);
	model_hash = phash_image(model, 1);
	
	/* Open SDL */
	if (SDL_Init(SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
		syslog_error("Init SDL.");
		return RET_ERROR;
	}

	/* Clean up on exit */
	// atexit(SDL_Quit) is evil !!!;
	SDL_Quit();
	SDL_WM_SetCaption("VIDEO", NULL);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen = SDL_SetVideoMode(video->width, video->height, 24,
	                     SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
	if (screen == NULL) {
		syslog_error("SDL_SetVideoMode, %s.", SDL_GetError());
		return RET_ERROR;
	}

	C = image_create(video->height, video->width); check_image(C);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x00000000; // 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0x00000000; // 0xff000000;
#endif
	rgbsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, video->width, video->height, 24, rmask, gmask, bmask, amask);
	if(rgbsurface == NULL) {
		syslog_error("Create RGBSurface, %s. ", SDL_GetError());
		return RET_ERROR;
	}

	if (video->frame_speed == 0)
		video->frame_speed = 25;

	i_begin = SDL_GetTicks();
	i_count = 0;
	while (! b_quit && ! feof(video->_fp)) {
		if (! b_pause) {
			frame_toimage(video_read(video), C);
			image_gauss_filter(C, 0.5f);
			object_detect(C, 16);
#if 1			
			image_drawrects(C);
#else
			model_match(C, model_hash, threshold);
#endif
			i_count++;
			snprintf(title, sizeof(title), "fps:%.2f/%d", 1000.0f*i_count/(SDL_GetTicks() - i_begin), video->frame_index); 
			// image_drawtext(sview_image(), 10, 10, fps, 0xff00ff);
			SDL_WM_SetCaption(title, NULL);
			__fill_surface(rgbsurface, C);
			SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
			SDL_UpdateRect(screen, 0, 0, video->width, video->height);
		}

		// Process events
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				if (b_fullscreen)
					SDL_WM_ToggleFullScreen(screen);
				b_quit = 1;
				break;
					
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
					if (b_fullscreen)
						SDL_WM_ToggleFullScreen(screen);
					b_quit = 1;
					break;

				case SDLK_s:
					{
						char filename[FILENAME_MAXLEN];
						if (image_valid(C)) {
 							snprintf(filename, sizeof(filename) - 1, "uniq_%d.jpg", video->frame_index);
							image_save(C, filename);
						}
					}
					break;
				case SDLK_f:
					if (SDL_WM_ToggleFullScreen(screen))
						b_fullscreen = 1 - b_fullscreen;
					break;

				case SDLK_SPACE:
					b_pause = (b_pause) ? 0 : 1;
					break;

				default:
					break;
				}
				break;
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 24,
					SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
				SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
				SDL_UpdateRect(screen, 0, 0, video->width, video->height);
				break;

			default:
				break;
			}
		}
	}
	image_destroy(C);

	SDL_FreeSurface(rgbsurface);

	return RET_OK;
}


static int __video_sview(VIDEO * video, int method)
{
	int b_pause = 0;
	int b_fullscreen = 0;
	int b_quit = 0;
	int i_wait;
	
	SDL_Event event;
	int64_t i_start, i_begin, i_count;
	SDL_Surface *rgbsurface, *screen;
	Uint32 rmask, gmask, bmask, amask;
	char title[256], fps[128];
	char filename[FILENAME_MAXLEN];
	IMAGE *image;
	FRAME *fg;

	/* Open SDL */
	if (SDL_Init(SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
		syslog_error("Init SDL.");
		return RET_ERROR;
	}

	/* Clean up on exit */
	// atexit(SDL_Quit) is evil !!!;
	SDL_Quit();
	SDL_WM_SetCaption("VIDEO", NULL);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen = SDL_SetVideoMode(video->width, video->height, 24,
	                     SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
	if (screen == NULL) {
		syslog_error("SDL_SetVideoMode, %s.", SDL_GetError());
		return RET_ERROR;
	}
	image = image_create(video->height, video->width);
	if (! image_valid(image)) {
		syslog_error("Create image.");
		return RET_ERROR;
	}
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x00000000; // 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0x00000000; // 0xff000000;
#endif
	rgbsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, video->width, video->height, 24, rmask, gmask, bmask, amask);
	if(rgbsurface == NULL) {
		syslog_error("Create RGBSurface, %s. ", SDL_GetError());
		return RET_ERROR;
	}

	if (video->frame_speed == 0)
		video->frame_speed = 25;

	i_begin = SDL_GetTicks();
	i_count = 0;
	while (! b_quit && ! feof(video->_fp)) {
		i_start = SDL_GetTicks();

		if (! b_pause) {
			fg = video_read(video);
			frame_toimage(fg, image);
			i_count++;
			snprintf(title, sizeof(title) - 1, "Frame:%03d", video->frame_index);
			SDL_WM_SetCaption(title, NULL);
			sview_blend(image, method, 1);

			snprintf(fps, sizeof(fps), "fps:%.2f/%d", 1000.0f*i_count/(SDL_GetTicks() - i_begin), video->frame_index); 
			image_drawtext(sview_image(), 10, 10, fps, 0xff00ff);
			__fill_surface(rgbsurface, sview_image());

#if 0
			snprintf(filename, sizeof(filename) - 1, "output/frame_%d.jpg", video->frame_index);
			image_save(sview_image(), filename);
#endif
			
			SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
			SDL_UpdateRect(screen, 0, 0, video->width, video->height);
		}

		// Process events
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				if (b_fullscreen)
					SDL_WM_ToggleFullScreen(screen);
				b_quit = 1;
				break;
					
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_ESCAPE:
					if (b_fullscreen)
						SDL_WM_ToggleFullScreen(screen);
					b_quit = 1;
					break;

				case SDLK_s:
					{
						if (image_valid(image)) {
 							snprintf(filename, sizeof(filename) - 1, "sview_%d.jpg", video->frame_index);
							image_save(sview_image(), filename);
						}
					}
					break;
				case SDLK_f:
					if (SDL_WM_ToggleFullScreen(screen))
						b_fullscreen = 1 - b_fullscreen;
					break;

				case SDLK_SPACE:
					b_pause = (b_pause) ? 0 : 1;
					break;

				default:
					break;
				}
				break;
			case SDL_VIDEORESIZE:
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 24,
					SDL_HWSURFACE | SDL_RESIZABLE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_ANYFORMAT);
				SDL_BlitSurface(rgbsurface, NULL, screen, NULL);
				SDL_UpdateRect(screen, 0, 0, video->width, video->height);
				break;

			default:
				break;
			}
		}

		/* wait */
		i_wait = 1000 / video->frame_speed - (SDL_GetTicks() - i_start);
		if (i_wait > 0)
			SDL_Delay(i_wait);
	}

	image_destroy(image);

	SDL_FreeSurface(rgbsurface);

	return RET_OK;
}


#else
static int __video_play(VIDEO * video)
{
	int n, r, c;
	RECT rect;
	FRAME *f;
	IMAGE *image, *edge;
	char filename[256];
	
	image = image_create(video->height, video->width);
	if (! image_valid(image)) {
		syslog_error("Create image.");
		return RET_ERROR;
	}
	edge = image_create(video->height, video->width);
	if (! image_valid(edge)) {
		syslog_error("Create image.");
		return RET_ERROR;
	}

	n = 0;
	while((f = video_read(video)) != NULL && n < 25) {
		frame_toimage(f, edge);
		
		shape_bestedge(edge);
		image_mcenter(edge,'R', &r, &c);
		rect.w = rect.h = 20; rect.r = r - 10; 	rect.c = c - 10;
		// sprintf(filename, "edge-%03d.jpg", n);
		// image_save(edge, filename);
		
		frame_toimage(f, image);
		image_foreach(edge, r, c) {
			if (edge->ie[r][c].r == 255) {
				image->ie[r][c].r = 255;
				image->ie[r][c].g = 255;
				image->ie[r][c].b = 255;
			}
		}
		image_drawrect(image, &rect, 0x00ff00, 0);
		snprintf(filename, sizeof(filename) - 1, "image-%03d.jpg", ++n);
		// image_save(image, filename);
	}
	image_destroy(edge);
	image_destroy(image);

	return RET_OK;
}

static int __video_mds(VIDEO * video)
{
	int page;
	IMAGE *C;

	C = image_create(video->height, video->width); check_image(C);

	srandom((unsigned int)time(NULL));
	page = random() % 300;  // 144, 331

	while(page > 0 && ! feof(video->_fp)) {
		frame_toimage(video_read(video), C);
		page--;
	}
	object_fast_detect(C);
	image_drawrects(C);
	image_save(C, "diff.jpg");

	image_destroy(C);

	return RET_OK;
}

static int __video_pds(VIDEO * video)
{
	int page;
	IMAGE *C;

	C = image_create(video->height, video->width); check_image(C);

	srandom((unsigned int)time(NULL));
	page = random() % 300;  // 144, 331

	while(page > 0 && ! feof(video->_fp)) {
		frame_toimage(video_read(video), C);
		page--;
	}
	object_fast_detect(C);
	image_drawrects(C);
	image_save(C, "diff.jpg");

	image_destroy(C);

	return RET_OK;
}


static int __video_compress(VIDEO * video, double threshold)
{
	int page;
	IMAGE *C;

	C = image_create(video->height, video->width); check_image(C);

	srandom((unsigned int)time(NULL));
	page = random() % 300;  // 144, 331

	while(page > 0 && ! feof(video->_fp)) {
		frame_toimage(video_read(video), C);
		page--;
	}
	object_fast_detect(C);
	image_drawrects(C);
	image_save(C, "diff.jpg");

	image_destroy(C);

	return RET_OK;
}

static int __video_match(VIDEO * video,  IMAGE *model, double threshold)
{
	int page;
	IMAGE *C;

	check_image(model);
	C = image_create(video->height, video->width); check_image(C);

	srandom((unsigned int)time(NULL));
	page = random() % 300;  // 144, 331

	while(page > 0 && ! feof(video->_fp)) {
		frame_toimage(video_read(video), C);
		page--;
	}
	object_fast_detect(C);
	image_drawrects(C);
	image_save(C, "diff.jpg");

	image_destroy(C);

	return RET_OK;
}



static int __video_sview(VIDEO * video, int method)
{
	int page;
	IMAGE *C;

	C = image_create(video->height, video->width); check_image(C);

	srandom((unsigned int)time(NULL));
	page = random() % 300;  // 144, 331

	while(page > 0 && ! feof(video->_fp)) {
		frame_toimage(video_read(video), C);
		if (video->frame_index < start)
			continue;
		page--;
	}
	object_fast_detect(C);
	image_drawrects(C);
	image_save(C, "sview.jpg");

	image_destroy(C);

	return RET_OK;
}

#endif



int video_valid(VIDEO *v)
{
	if (! v || v->height < 1 || v->width < 1 || v->frame_size < 1 || v->magic != VIDEO_MAGIC)
		return 0;

	return frame_goodfmt(v->format);
}

void video_info(VIDEO *v)
{
	if (! v)
		return;
	
	printf("Video information:");
	printf("frame size  : %d (%d x %d) pixels\n", v->frame_size, v->width, v->height);
	printf("frame format: %c%c%c%c\n", GET_FOURCC1(v->format), GET_FOURCC2(v->format), GET_FOURCC3(v->format), GET_FOURCC4(v->format));
}

// Default camera device is /dev/video0
VIDEO *video_open(char *filename, int start)
{
	int i;
	FILE *fp;
	VIDEO *v = NULL;
	struct v4l2_format v4l2fmt;
	struct v4l2_streamparm parm;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers req;
	char cmdline[FILENAME_MAXLEN];

	// Allocate video 
	v = (VIDEO *)calloc((size_t)1, sizeof(VIDEO)); 
	if (! v) { 
		syslog_error("Allocate memeory.");
		return NULL; 
	}
	v->frame_speed = 25;
	
	if (strncmp(filename, "/dev/video", 10) == 0) {
		if ((fp = fopen(filename, "r")) == NULL) {
			syslog_error("Open %s\n", filename);
			goto fail;
		}
		v->_fp = fp;
		v->video_source = VIDEO_FROM_CAMERA;

		v->frame_speed = 25;
		// Set/Get camera parameters.
		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		parm.parm.capture.timeperframe.numerator = 1;
		parm.parm.capture.timeperframe.denominator = 30;  // frames/1 second
		parm.parm.capture.capturemode = 0;
		if (ioctl(fileno(fp), VIDIOC_S_PARM, &parm) < 0) {
			syslog_error("Set video parameter.");
			goto fail;
		}
		if (ioctl(fileno(fp), VIDIOC_G_PARM, &parm) < 0) {
			syslog_error("Get camera parameters.");
			goto fail;
		}
		v->frame_speed = parm.parm.capture.timeperframe.denominator;
		v4l2fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fileno(fp), VIDIOC_G_FMT, &v4l2fmt) < 0) {
			syslog_error("Get format.");
			goto fail;
		}
		for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
			v->frames[i] = frame_create(v4l2fmt.fmt.pix.pixelformat, v4l2fmt.fmt.pix.width, v4l2fmt.fmt.pix.height);
			if (! v->frames[i]) {
				syslog_error("Allocate memory");
				return NULL;
			}
		}

		// Allocate frame buffer
		memset(&req, 0, sizeof(req));
		req.count = VIDEO_BUFFER_NUMS;
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory = V4L2_MEMORY_MMAP;
		if (ioctl(fileno(fp), VIDIOC_REQBUFS, &req) < 0) {
			syslog_error("VIDIOC_REQBUFS.");
			goto fail;
		}
		for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
			memset(&buf, 0, sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;
			if (ioctl(fileno(fp), VIDIOC_QUERYBUF, &buf) < 0) {
				syslog_error("VIDIOC_QUERYBUF.");
				goto fail;
			}
			v->_frame_buffer[i].length = buf.length;
			v->_frame_buffer[i].offset = (size_t) buf.m.offset;
			v->_frame_buffer[i].startmap = mmap (NULL /* start anywhere */,
	                              buf.length,
	                              PROT_READ | PROT_WRITE /* required */,
	                              MAP_SHARED /* recommended */,
	                              fileno(fp), buf.m.offset);

	        if (v->_frame_buffer[i].startmap == MAP_FAILED) {
				syslog_error("mmap.");
				goto fail;
	        }
			else
				frame_binding(v->frames[i], (BYTE *)v->_frame_buffer[i].startmap);
			
		}
		for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
			memset(&buf, 0, sizeof(buf));
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;
			buf.m.offset = v->_frame_buffer[i].offset;
			if (ioctl(fileno(fp), VIDIOC_QBUF, &buf) < 0) {
				syslog_error("VIDIOC_QBUF.");
				goto fail;
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fileno(fp), VIDIOC_STREAMON, &type) < 0) {
			syslog_error("VIDIOC_STREAMON.");
			goto fail;
		}
		v->frame_size = frame_size(v->format, v->width, v->height);
	}
	else {
		// ...
		if (__video_probe(filename, v) != RET_OK)
			goto fail;

		snprintf(cmdline, sizeof(cmdline) - 1, "ffmpeg -i %s -ss %d -f rawvideo - 2>/dev/null", filename, start);
		if ((fp = popen(cmdline, "r")) == NULL) {
			syslog_error("Open %s\n", cmdline);
			goto fail;
		}
		v->_fp = fp;

		for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
			v->frames[i] = frame_create(v->format, v->width, v->height);
			if (! v->frames[i]) {
				syslog_error("Allocate memory");
				return NULL;
			}
		}

		v->frame_size = frame_size(v->format, v->width, v->height);
		// Allocate memory
		for (i = 0; i < VIDEO_BUFFER_NUMS; i++) {
			v->_frame_buffer[i].startmap = (BYTE *)calloc((size_t)v->frame_size, sizeof(BYTE));
			if (v->_frame_buffer[i].startmap == NULL) {
				syslog_error("Allocate memory");
				goto fail;
			}
			else
				frame_binding(v->frames[i], (BYTE *)v->_frame_buffer[i].startmap);
		}
	}
	v->frame_index = 0;

	v->magic = VIDEO_MAGIC;

	return v;
fail:
	video_close(v);
	
	return NULL;
}

void video_close(VIDEO *v)
{
	int i, type;

	if (! v)
		return;

	if (v->video_source == VIDEO_FROM_CAMERA) {
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ioctl(fileno(v->_fp), VIDIOC_STREAMOFF, &type);

		for (i = 0; i < VIDEO_BUFFER_NUMS; i++)
			munmap (v->_frame_buffer[i].startmap, v->_frame_buffer[i].length);
	}
	else {
		for (i = 0; i < VIDEO_BUFFER_NUMS; i++)
			free(v->_frame_buffer[i].startmap);
	}
	for (i = 0; i < VIDEO_BUFFER_NUMS; i++)
		frame_destroy(v->frames[i]);

	pclose(v->_fp);
	free(v);
}


FRAME *video_read(VIDEO *v)
{
	int n = v->frame_size;
	struct v4l2_buffer v4l2buf;
	FRAME *f = NULL;
	
	if (v->video_source == VIDEO_FROM_CAMERA) {
		memset(&v4l2buf, 0, sizeof(struct v4l2_buffer));
		v4l2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		v4l2buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(fileno(v->_fp), VIDIOC_DQBUF, &v4l2buf) < 0) {
			syslog_error("VIDIOC_DQBUF.");
			return NULL;
		}
		f = v->frames[v4l2buf.index];
		ioctl(fileno(v->_fp), VIDIOC_QBUF, &v4l2buf);
	}
	else {
		v->_buffer_index++;
		v->_buffer_index %= VIDEO_BUFFER_NUMS;
		f = v->frames[v->_buffer_index];

		n = fread(f->Y, v->frame_size, 1, v->_fp);
	}

	v->frame_index++;
	return (n == 0)? NULL : f;		// n==0 ==> end of file
}

int video_play(char *filename, int start)
{
	VIDEO *video;

	video = video_open(filename, start);
	check_video(video);

	video_info(video);
	__video_play(video);
 	video_close(video);

	return RET_OK;
}

// motion detect system
int video_mds(char *filename, int start)
{
	VIDEO *video;

	video = video_open(filename, start);
	check_video(video);

	video_info(video);
	__video_mds(video);
 	video_close(video);

	return RET_OK;
}

// plane detect system
int video_pds(char *filename, int start)
{
	VIDEO *video;

	video = video_open(filename, start);
	check_video(video);

	video_info(video);
	__video_pds(video);
 	video_close(video);

	return RET_OK;
}


int video_compress(char *filename, int start, double threshold)
{
	VIDEO *video;

	video = video_open(filename, start);
	check_video(video);

	video_info(video);
	__video_compress(video, threshold);
 	video_close(video);

	return RET_OK;
}

int video_match(char *filename, int start, IMAGE *model, double threshold)
{
	VIDEO *video;

	video = video_open(filename, start);
	check_video(video);

	video_info(video);
	__video_match(video, model, threshold);
 	video_close(video);

	return RET_OK;
}

// inbreak detect system
int video_ids(char *filename, int start, double threshold)
{
	double d;
	FRAME *bg, *fg;
	VIDEO *video = video_open(filename, start);
	check_video(video);

	bg = video_read(video);
	while(1) {
		fg = video_read(video);
		d = __math_snr(video->frame_size, (char *)bg->Y, (char *)fg->Y);
		if (d < threshold) {
			printf("Video IDS: Some object has been detected !!!\n");
		}
		bg = fg;
	}
	video_close(video);

	return RET_OK;
}

int yuv420_play(char *filename, int width, int height)
{
	VIDEO *video;

	video = __yuv420_open(filename, width, height);
	check_video(video);

	video_info(video);
	__video_play(video);
 	video_close(video);

	return RET_OK;
}

int video_sview(char *filename, int method)
{
	VIDEO *video;

	if (sview_start("camera.model") != RET_OK) {
		syslog_error("Reading camera model.");
		return RET_ERROR;
	}

	video = video_open(filename, 0);
	check_video(video);

	video_info(video);
	__video_sview(video, method);
 	video_close(video);

	sview_stop();
	return RET_OK;
}

