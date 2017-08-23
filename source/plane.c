/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Fri Aug  4 15:53:30 CST 2017
***
************************************************************************************/


#include "plane.h"
#include "phash.h"
#include "color.h"

static int airbus_window_exist(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.75f;
	double t, max, ratio;
	int window_index;
	
	// Airbus
	PHASH airbus_windows[] = {
		0xff810000007000ff,	// window/airbus_001.jpg
		0xe3e100004070e0f1	// window/airbus_002.jpg
		};

	max = 0.0f;
	window_index = -1;
	for (i = 0; i < mrs->count; i++) {
		// Location is on left ?
		if (mrs->rect[i].c >= img->width/2)
			continue;
		
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio < 0.5f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(airbus_windows), airbus_windows);
		if (t > max) {
			max = t;
			window_index = i;
		}
	}
	CheckPoint("max = %f", max);
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[window_index]), color_picker(), 0);
	}
	
	return (max >= threshold)?1 : 0;
}

static int boeing_window_exist(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.75f;	// 550
	double t, max, ratio;
	int window_index;
	
	// Boeing
	PHASH boeing_windows[] = {
		0x7f1f8780c0e0f81b,		// window/boeing_001.jpg
		0xff9900008081f3ff		// window/boeing_002.jpg
		};

	max = 0.0f;
	window_index = -1;
	for (i = 0; i < mrs->count; i++) {
		// Location is on left ?
		if (mrs->rect[i].c >= img->width/2)
			continue;
		
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio < 0.5f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(boeing_windows), boeing_windows);
		if (t > max) {
			max = t;
			window_index = i;
		}
	}

	CheckPoint("max = %f", max);
	if (max >= threshold) {
		CheckPoint("-------------boeing window ---------------------------");
		image_drawrect(img, &(mrs->rect[window_index]), color_picker(), 0);
	}

	return (max >= threshold)?1 : 0;
}

static int window_exist(IMAGE *img)
{
	static int count = 0;

	CheckPoint("-------------window detection---------------------------");

	if (airbus_window_exist(img) || boeing_window_exist(img))
		count++;

	return (count >= 2);
}

static int common_dispear(IMAGE *img)
{
	int i, loc;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.85f;
	double t, max, ratio;
	int window_index;
	
	PHASH stop_phashs[] = {
		0xffbfa181c7e7e3e3,	// stop/stop_001.jpg
		0xf0f9fd99f87c8020	// stop/a_leave_001.jpg
		};

	max = 0.0f;
	window_index = -1;
	for (i = 0; i < mrs->count; i++) {
		// Location is on left ?
		loc = mrs->rect[i].c + mrs->rect[i].w/2;
		if (loc >= img->width/2)
			continue;
		
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio < 0.5f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(stop_phashs), stop_phashs);
		if (t > max) {
			max = t;
			window_index = i;
		}
	}
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[window_index]), color_picker(), 0);
	}

	return (max >= threshold)?1 : 0;
}

static int window_dispear(IMAGE *img)
{
	static int count = 0;

	if (common_dispear(img))
		count++;

	return (count >= 2);
}

static int common_wheel_release(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.75f;
	double t, max, ratio;
	int wheel_index;
	
	PHASH wheel_release_phashs[] = {
		0xf3ff8103019c3d03,	// wheel/release_001.jpg
		0xff01040e9e9f8dc7, // wheel/release_002.jpg
		0x1011d3c3d3d9983	// wheel/release_003.jpg
		};

	max = 0.0f;
	wheel_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
#if 1		
		if (ratio < 0.3f || ratio >= 3.0f) //
			continue;
#endif
		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(wheel_release_phashs), wheel_release_phashs);
		if (t > max) {
			max = t;
			wheel_index = i;
		}
	}
	CheckPoint("max = %f", max);
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[wheel_index]), color_picker(), 0);
	}

	return (max >= threshold)?1 : 0;
}

static int wheel_release(IMAGE *img)
{
	static int count = 0;

	if (common_wheel_release(img))
		count++;

	return (count >= 2);
}

static int common_wheel_fixed(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.70f;
	double t, max, ratio;
	int wheel_index;
	
	PHASH wheel_fixed_phashs[] = {
		0xc3c3cbdd989880ff,	// wheel/fixed_001.jpg 
		0xc3819d3cbcdb83c1, // wheel/fixed_002.jpg
		0xf1c7c3c1891d0001  // wheel/fixed_003.jpg
		};
	max = 0.0f;
	wheel_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio < 0.3f || ratio >= 3.0f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(wheel_fixed_phashs), wheel_fixed_phashs);
		if (t > max) {
			max = t;
			wheel_index = i;
		}
	}
	CheckPoint("max = %f", max);
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[wheel_index]), color_picker(), 0);
	}

	return (max >= threshold)?1 : 0;
}


static int wheel_fixed(IMAGE *img)
{
	static int count = 0;

	if (common_wheel_fixed(img))
		count++;

	return (count >= 2);
}

static int airbus_door_opened(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.80f;
	double t, max, ratio;
	int handler_index, catseye_index;
	
	// Airbus
	PHASH airbus_door_open_handlers[] = {
		0xfbdbfefcf4f46000,	// door/a_handler_001.jpg
		0xbfbffefcfcfc7c05	// door/a_handler_002.jpg
		};
	PHASH airbus_door_open_catseyes[] = {
		0xf7fdfcfe009c81c3,	// door/a_cats_eye_001.jpg
		0xffc9d8989899c1ff	// door/a_cats_eye_002.jpg
		};

	max = 0.0f;
	handler_index = -1;
	for (i = 0; i < mrs->count; i++) {
		if (mrs->rect[i].h < mrs->rect[i].w) // flat rectangle ?
			continue;

		// Location is on left ?
		if (mrs->rect[i].c >= img->width/2)
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(airbus_door_open_handlers), airbus_door_open_handlers);
		if (t > max) {
			max = t;
			handler_index = i;
		}
	}
	CheckPoint("Airbus Handler: max = %f", max);
	if (max < threshold)
		return 0;

	// Found handler, checking cats eye ...
	max = 0.0f;
	catseye_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio <= 0.5f || ratio >= 2.0f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(airbus_door_open_catseyes), airbus_door_open_catseyes);
		if (t > max) {
			max = t;
			catseye_index = i;
		}
	}

	CheckPoint("Airbus Cats eys: max = %f", max);
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[handler_index]), 0xff0000, 0);
		image_drawrect(img, &(mrs->rect[catseye_index]), 0x0000ff, 0);
		return 1;
	}

	return 0;
}

static int boeing_door_opened(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.80f;
	double t, max, ratio;
	int handler_index, catseye_index;
	
	// Boeing
	PHASH boeing_door_open_handlers[] = {
		0xffc7010c0081ffff,	// door/b_handler_001.jpg 
		0xffe70000000081ff	// door/b_handler_002.jpg 
	};
	PHASH boeing_door_open_catseyes[] = {
		0xffcfc38383c3e7ff,	// door/b_cats_eye_001.jpg 
		0xffc381818181d3ff	// door/b_cats_eye_002.jpg 
	};

	
	CheckPoint("... Detect Boeing Handler ...");

	max = 0.0f;
	handler_index = -1;
	for (i = 0; i < mrs->count; i++) {
		if (mrs->rect[i].h > mrs->rect[i].w) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(boeing_door_open_handlers), boeing_door_open_handlers);
		if (t > max) {
			max = t;
			handler_index = i;
		}
	}
	CheckPoint("Boeing Handler: max = %f", max);
	
	if (max < threshold)
		return 0;
	
	CheckPoint("... Detect Boeing Cats Eye ...");
	max = 0.0f;
	catseye_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio <= 0.5f || ratio >= 2.0f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(boeing_door_open_catseyes), boeing_door_open_catseyes);
		if (t > max) {
			max = t;
			catseye_index = i;
		}
	}

	CheckPoint("Boeing Cats Eys: max = %f", max);
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[handler_index]), color_picker(), 0);
		image_drawrect(img, &(mrs->rect[catseye_index]), color_picker(), 0);

		return 1;
	}

	return 0;
}

static int door_opened(IMAGE *img)
{
	static int count = 0;

	CheckPoint(" ---- Door opened detection --------------------");

	if (airbus_door_opened(img) || boeing_door_opened(img))
		count++;

	return (count >= 2);
}

static int airbus_door_closed(IMAGE *img)
{
	int i, loc;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.85f;
	double t, max, ratio;
	int handler_index, catseye_index;
	
	// Airbus
	PHASH airbus_door_open_handlers[] = {
		0xfbdbfefcf4f46000,	// door/a_handler_001.jpg
		0xbfbffefcfcfc7c05	// door/a_handler_002.jpg
	};
	PHASH airbus_door_open_catseyes[] = {
		0xf7fdfcfe009c81c3,	// door/a_cats_eye_001.jpg
		0xffc9d8989899c1ff	// door/a_cats_eye_002.jpg
	};

	max = 0.0f;
	handler_index = -1;
	for (i = 0; i < mrs->count; i++) {
		// Location is on right ?
		loc = mrs->rect[i].c + mrs->rect[i].w/2;
		if (loc < img->width/2)
			continue;
		
		if (mrs->rect[i].h < mrs->rect[i].w) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(airbus_door_open_handlers), airbus_door_open_handlers);
		if (t > max) {
			max = t;
			handler_index = i;
		}
	}
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[handler_index]), color_picker(), 0);
	}
	else 
		handler_index = -1;

	max = 0.0f;
	catseye_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio <= 0.5f || ratio >= 2.0f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(airbus_door_open_catseyes), airbus_door_open_catseyes);
		if (t > max) {
			max = t;
			catseye_index = i;
		}
	}
	if (max >= threshold)
		image_drawrect(img, &(mrs->rect[catseye_index]), color_picker(), 0);
	else
		catseye_index = -1;;

	return (handler_index >= 0 && catseye_index >= 0)?1 : 0;
}

static int boeing_door_closed(IMAGE *img)
{
	int i;
	RECTS *mrs = rect_set();
	PHASHS *mps = phash_set();
	double threshold = 0.85f;
	double t, max, ratio;
	int handler_index, catseye_index;
	
	// Boeing
	PHASH boeing_door_open_handlers[] = {
		0x3f074f435f7d5f1f	 // door/b_opened_handler_001.jpg
	};
	PHASH boeing_door_open_catseyes[] = {
		0xcfcfff978703ffff	//	door/b_opened_cats_eye_001.jpg
	};

	max = 0.0f;
	handler_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio >= 0.5f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(boeing_door_open_handlers), boeing_door_open_handlers);
		if (t > max) {
			max = t;
			handler_index = i;
		}
	}
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[handler_index]), color_picker(), 0);
	}
	else 
		handler_index = -1;

	max = 0.0f;
	catseye_index = -1;
	for (i = 0; i < mrs->count; i++) {
		ratio = 1.0f*mrs->rect[i].h/mrs->rect[i].w;
		if (ratio <= 0.5f || ratio >= 2.0f) //
			continue;

		t = phash_maxlike(mps->phash[i], ARRAY_SIZE(boeing_door_open_catseyes), boeing_door_open_catseyes);
		if (t > max) {
			max = t;
			catseye_index = i;
		}
	}
	if (max >= threshold) {
		image_drawrect(img, &(mrs->rect[catseye_index]), color_picker(), 0);
	}
	else 
		catseye_index = -1;

	return (handler_index >= 0 && catseye_index >= 0)?1 : 0;
}

// xxxx1111
static int door_closed(IMAGE *img)
{
	static int count = 0;

	if (airbus_door_closed(img) || boeing_door_closed(img))
		count++;

	return (count >= 2);
}

int plane_reset(PLANE *p)
{
	p->bridge = 0;
	p->model[0] = '\0';
	p->status = PLANE_Start;
	p->time = get_time();
	
	return RET_OK;
}

int plane_report(PLANE *p)
{
	switch(p->status) {
	case PLANE_Window_Exist:
		CheckPoint("Plane Arraive.");
		break;
	case PLANE_Wheel_Fixed:
		CheckPoint("Wheel Fixed.");
		break;
	case PLANE_Wheel_Release:
		CheckPoint("Wheel Release.");
		break;
	case PLANE_Door_Open:
		CheckPoint("Door Open.");
		break;
	case PLANE_Door_Close:
		CheckPoint("Door Close.");
		break;
	case PLANE_Window_Dispear:
		CheckPoint("Plane Departure.");
		break;
	}
	return RET_OK;
}

int plane_detect(PLANE *p, IMAGE *img)
{
	object_detect(img, 16);
	
	phash_objects(img);

//	model_match(img, 0xfbdbfefcf4f46000, 0.8f);
//	return RET_OK; // xxxx9999


	p->status = PLANE_Door_Close_0; // Test

	
	switch(p->status) {
	case PLANE_Start:
		if (window_exist(img)) {
			p->time = get_time();
			p->status = PLANE_Window_Exist;
			plane_report(p);
		}
		break;
	case PLANE_Window_Exist:
		if (wheel_release(img)) {	// 550
			p->time = get_time();
			p->status = PLANE_Wheel_Release_0;
		}
		break;
	case PLANE_Wheel_Release_0:
		if (wheel_fixed(img)) {
			p->time = get_time();
			p->status = PLANE_Wheel_Fixed;
			plane_report(p);
		}
		break;
	case PLANE_Wheel_Fixed:
		if (door_closed(img)) {
			p->time = get_time();
			p->status = PLANE_Door_Close_0;
		}
		break;
	case PLANE_Door_Close_0:
		if (door_opened(img)) {
			p->time = get_time();
			p->status = PLANE_Door_Open;
			plane_report(p);
 		}
		break;
	case PLANE_Door_Open:
		if (door_closed(img)) {
			p->time = get_time();
			p->status = PLANE_Door_Close;
			plane_report(p);
 		}
		break;
	case PLANE_Door_Close:
		if (wheel_fixed(img)) {
			p->time = get_time();
			p->status = PLANE_Wheel_Fixed_0;
 		}
		break;
	case PLANE_Wheel_Fixed_0:
		if (wheel_release(img)) {
			p->time = get_time();
			p->status = PLANE_Wheel_Release;
			plane_report(p);
 		}
		break;
	case PLANE_Wheel_Release:
		if (window_exist(img)) {
			p->time = get_time();
			p->status = PLANE_Window_Exist_0;
 		}
		break;
	case PLANE_Window_Exist_0:
		if (window_dispear(img)) {
			p->time = get_time();
			p->status = PLANE_Window_Dispear;
			plane_report(p);
 		}
		break;
	case PLANE_Window_Dispear:
		p->time = get_time();
		p->status = PLANE_End;
		break;
	case PLANE_End:
		p->status = PLANE_Start;
		break;
	default:
		break;
	}
	
	return RET_OK;
}

