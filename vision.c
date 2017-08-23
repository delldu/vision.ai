/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Tue Feb  7 12:41:41 CST 2017
***
************************************************************************************/

#include "vision.h"

// Save vector to lua stack
int lua_save_vector(lua_State *L, VECTOR *vec)
{
	int i;

	lua_newtable(L);
	for (i = 0; i < vec->m; i++) {
		lua_pushnumber(L, i + 1);
		lua_pushnumber(L, vec->ve[i]);
		lua_settable(L, -3);
	}
	return 1;
}

// Save matrix to lua stack
int lua_save_matrix(lua_State *L, MATRIX *mat)
{
	int i, j, n;

	n = 1;
	lua_newtable(L);
	for (i = 0; i < mat->m; i++) {
		for (j = 0; j < mat->n; j++) {
			lua_pushnumber(L, n++);
			lua_pushnumber(L, mat->me[i][j]);
			lua_settable(L, -3);
		}
		
	}
	return 1;
}

// Save rect set to lua stack
int lua_save_rectset(lua_State *L)
{
	int i, n = 1;
	RECTS *mrs = rect_set();
	
	lua_newtable(L);

	for (i = 0; i < mrs->count; i++) {
		if (mrs->rect[i].h < 1 || mrs->rect[i].w < 1)
			continue;
		
		lua_pushnumber(L, n++);
		lua_pushnumber(L, mrs->rect[i].r);
		lua_settable(L, -3);

		lua_pushnumber(L, n++);
		lua_pushnumber(L, mrs->rect[i].c);
		lua_settable(L, -3);

		lua_pushnumber(L, n++);
		lua_pushnumber(L, mrs->rect[i].h);
		lua_settable(L, -3);

		lua_pushnumber(L, n++);
		lua_pushnumber(L, mrs->rect[i].w);
		lua_settable(L, -3);
	}
	return 1;
}

// Image
IMAGE *lua_check_image(lua_State *L, int index)
{
	void *ud = luaL_checkudata(L, index, IMAGE_META_NAME);
	luaL_argcheck(L, ud != NULL, index, "`image' expected");
	return (IMAGE *)ud;
}

// Create image on Lua Stack
IMAGE *lua_image_create(lua_State *L, int h, int w)
{
	size_t nbytes = image_memsize(h, w);
	IMAGE *img = (IMAGE *)lua_newuserdata(L, nbytes);
	if (img == NULL) {
		lua_pushnil(L);
		syslog_error("Allocate image memory.");
		return NULL;
	}
	else {
		// Set meta table
		luaL_getmetatable(L, IMAGE_META_NAME);
		lua_setmetatable(L, -2);
		image_membind(img, h, w);
	}
	return img;
}
