/************************************************************************************
***
***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Tue Feb  7 12:41:41 CST 2017
***
************************************************************************************/

#include "vision.h"


static int lvideo_help(lua_State *L)
{
	(void)L;

	printf("Video Object help:\n");
	printf("\n");
	printf("Functions:\n");
	printf("  video.play(filename)\n");
	printf("  video.mds(filename)                               Motion Detection System\n");
	printf("  video.ids(filename, threshold)                    Inbreak Detection System\n");
	printf("  video.sds(filename)                               Skin Detection System\n");
	printf("\n");
	printf("Methods:\n");
	return 0;
}

static int lvideo_play(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer start = luaL_optinteger(L, 2, 0);
	video_play((char *)fname, start);
	
	return 0;
}

static int lvideo_mds(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer start = luaL_optinteger(L, 2, 0);
	video_mds((char *)fname, start);
	return 0;
}

static int lvideo_pds(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer start = luaL_optinteger(L, 2, 0);
	video_pds((char *)fname, start);
	return 0;
}


static int lvideo_compress(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer start = luaL_optinteger(L, 2, 0);
	lua_Number threshold = luaL_optnumber(L, 3, 0.01f);
	video_compress((char *)fname, start, threshold);
	return 0;
}

static int lvideo_match(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer start = luaL_optinteger(L, 2, 0);
	IMAGE *model = lua_check_image(L, 3);
	lua_Number threshold = luaL_optnumber(L, 4, 0.85f);
	video_match((char *)fname, start, model, threshold);
	return 0;
}


static int lvideo_ids(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer start = luaL_optinteger(L, 2, 0);
	lua_Number th = luaL_optnumber(L, 3, 255.0f);
	video_ids((char *)fname, start, th);
	
	return 0;
}

static int lvideo_sview(lua_State *L)
{
	const char* fname = lua_tostring(L, 1);
	lua_Integer method = luaL_checkinteger(L, 2);
 	
	video_sview((char *)fname, method);
	
	return 0;
}


static const struct luaL_Reg video_lib_f[] = {
	{ "play", lvideo_play },
	{ "mds", lvideo_mds },
	{ "ids", lvideo_ids },
	{ "pds", lvideo_pds },
	{ "compress", lvideo_compress },
	{ "match", lvideo_match },
	{ "sview", lvideo_sview },
	{ "help", lvideo_help },

	{ NULL, NULL }
};

static void setup_copyright(lua_State *L) {
	lua_pushliteral(L, "Lua Video Module(1.0.1), 2008-2017, Dell Du.");
	lua_setfield(L, -2, "copyright");
}

int luaopen_libvision_video(lua_State *L) {
	// Register meta table
	luaL_newmetatable(L, VIDEO_META_NAME);

	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2); /* pushes the metatable */
	lua_settable(L, -3); /* metatable.__index = metatable */

	luaL_openlib(L, "vision.video", video_lib_f, 0);

	setup_copyright(L);
	
	return 1;
}


