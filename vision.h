/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Thu Feb  9 23:06:54 CST 2017
***
************************************************************************************/


#ifndef _VISION_H
#define _VISION_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


// luaL_argcheck(L, 1 <= i && i <= vec->m, 2, "index out of range");

// bad argument #1 to 'get' (number expected, got string)
#define lua_check_argment(L, cond, index, msg) \
	do { \
		if (! (cond)) { printf("bad argument #%d: %s", index, msg); return 0; } \
	} while(0)


// Vector
#include <vector.h>
int lua_save_vector(lua_State *L, VECTOR *vec);

// Matrix
#include <matrix.h>
int lua_save_matrix(lua_State *L, MATRIX *mat);


// Image
#include <image.h>
#define IMAGE_META_NAME "vision.image"
IMAGE *lua_check_image(lua_State *L, int index);
IMAGE *lua_image_create(lua_State *L, int h, int w);

int lua_save_rectset(lua_State *L);

// Video
#include <video.h>
#define VIDEO_META_NAME "vision.video"

int luaopen_vision_image(lua_State *L);
int luaopen_vision_video(lua_State *L);

#include <blend.h>

#if defined(__cplusplus)
}
#endif

#endif	// _VISION_H

