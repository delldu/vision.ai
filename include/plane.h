/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Fri Aug  4 15:53:52 CST 2017
***
************************************************************************************/


#ifndef _PLANE_H
#define _PLANE_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "common.h"
#include "image.h"
#include "phash.h"

#define PLANE_MODE_LENGTH 64

enum STATUS {
	PLANE_Start = 0,
	PLANE_Window_Exist,			// Report Arraiv
	PLANE_Wheel_Release_0,
	PLANE_Wheel_Fixed,			// Report Wheel Fixed
	PLANE_Door_Close_0,
	PLANE_Door_Open,			// Report Door Open
	PLANE_Door_Close,			// Report Door Close
	PLANE_Wheel_Fixed_0,
	PLANE_Wheel_Release,		// Report Wheel Release
	PLANE_Window_Exist_0,
	PLANE_Window_Dispear,		// Report Depature
	PLANE_End,
};

typedef struct {
	char model[PLANE_MODE_LENGTH];
	int bridge;
	TIME time;		// last status update time
	int status;
} PLANE;

int plane_reset(PLANE *p);
int plane_detect(PLANE *p, IMAGE *img);

#if defined(__cplusplus)
}
#endif

#endif	// _PLANE_H

