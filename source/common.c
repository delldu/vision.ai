
/************************************************************************************
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Sat Jul 31 14:19:59 HKT 2010
***
************************************************************************************/

#include "common.h"

#include <sys/wait.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>
#include <regex.h>
#include <sys/stat.h>
#include <fcntl.h>

static DOTS __global_dot_set;
static LINES __global_line_set;
static CIRCLES __global_circle_set;
static OVALS __global_oval_set;
static RECTS __global_rect_set;

static TIME __system_ms_time;

int file_size(char *filename)
{
	struct stat s;
	return (stat(filename, &s) != 0 || !S_ISREG(s.st_mode)) ? -1 : s.st_size;
}

int file_load(const char *filename, char *buf, int size)
{
	int cnt = -1;
	int fd = open(filename, O_RDONLY);
	if (fd >= 0) {
		cnt = read(fd, buf, size);
		if (cnt >= 0 && cnt < size)
			buf[cnt] = '\0';
		close(fd);
	} else {
		syslog_error("Loading file (%s).", filename);
	}
	return (cnt >= 0) ? RET_OK : RET_ERROR;
}


int buf_scanf(char *buf, const char *pattern, ...)
{
#define MAXSUBREGS 32
	int i, j, en, len;
	regex_t reg;
	regmatch_t subregs[MAXSUBREGS];

	char *var;
	va_list args;

	en = regcomp(&reg, pattern, REG_EXTENDED | REG_ICASE);
	if (en != 0) {
		syslog_error("compile regular express [%s] error.", pattern);
		return RET_ERROR;
	}

	en = regexec(&reg, buf, MAXSUBREGS, subregs, 0);
	if (en == 0) {
		va_start(args, pattern);
		for (j = 1; j <= (int)reg.re_nsub; j++) {
			var = va_arg(args, char *);
			if (! var)
				continue;
			len = subregs[j].rm_eo - subregs[j].rm_so;
			for (i = 0; i < len; i++)
				*var++ = buf[subregs[j].rm_so + i];
			*var = '\0';
		}
		va_end(args);
	}
	regfree(&reg);

#undef MAXSUBREGS

	return (en == 0) ? subregs[0].rm_eo - subregs[0].rm_so : -1;
}

int get_token(char *buf, char deli, int maxcnt, char *tv[])
{
	int squote = 0;
	int dquote = 0;
	int n = 0;	

	if (! buf)
		return 0;
	tv[n] = buf;
	while (*buf && (n + 1) < maxcnt) {
		if ((squote % 2) == 0 && (dquote % 2) == 0 && deli == *buf) {
			*buf = '\0'; ++buf;
			while (*buf == deli)
				++buf;
			tv[++n] = buf;
			squote = dquote = 0;		// new start !!!
		}
		squote += (*buf == '\'') ? 1 : 0;
		dquote += (*buf == '\"') ? 1 : 0;
		++buf;
	}
	return (n + 1);
}

int bit_count(int val)
{
	int num = 0;
	while(val) {
		++num;
		val &= val - 1;
	}
	return num;
}

int math_log2(int n)
{
	int i, hbit = 0, on;
	on = n;
	for (i = 0; i < (int)(8*sizeof(int)); i++) {
		if (n & 1)
			hbit = i;
		n = n >> 1;
	}
	if (on > (1 << hbit))
		++hbit;

	return hbit;
}

// Carte to polar coordinate
void math_topolar(int x, int y, double *r, double *a)
{
	double dr, da;

	if (x == 0) {
		if (y >= 0) {
			*a = 90; *r = y;
		}
		else {
			*a = 270; *r = -y;
		}
		return;
	}

	// x != 0
	dr = sqrt(x * x + y * y) + 0.5000;
	da = asin(1.0000 * y/dr)/3.1415926 * 180;

	*r = dr;
	if (x > 0) { //  (x, y) in 1/4 section
		*a = da;
		if (y < 0)
			*a += 360;
	}
	else  //  (x, y) in 2/3 section
		*a = 180 - da;
}


int math_arcindex(double a, int arcstep)
{
	int n;
	n = (2 * a + arcstep) / (2 * arcstep); // [a/arcstep + 1/2]
	if (n >= 360/arcstep)
		n = 0;
	return n;
}

// Guass band width
int math_gsbw(double sigma)
{
	int  dim;
	double d;

	d = 3 * sigma; dim = (int)d;
	if (d > (double)dim)
		dim++;

	return 2 * dim + 1;
}

DOTS *dot_set()
{
	return &__global_dot_set;
}

void dot_put(int r, int c)
{
	DOTS *dots = dot_set();
	if (dots->count < MAX_OBJECT_NUM) {
		dots->dot[dots->count].r = r;
		dots->dot[dots->count].c = c;
		dots->count++;
	}
	else {
		syslog_debug("Too many dots.");
	}
}

LINES *line_set()
{
	return &__global_line_set;
}

void line_put(int r1, int c1, int r2, int c2)
{
	LINES *lines = line_set();

	if (lines->count < MAX_OBJECT_NUM) {
		lines->line[lines->count].r1 = r1;
		lines->line[lines->count].c1 = c1;
		lines->line[lines->count].r2 = r2;
		lines->line[lines->count].c2 = c2;
		lines->count++;
	}
}

CIRCLES *circle_set()
{
	return &__global_circle_set;
}

void circle_put(int x, int y, int r)
{
	CIRCLES *circles = circle_set();

	if (circles->count < MAX_OBJECT_NUM) {
		circles->circle[circles->count].x = x;
		circles->circle[circles->count].y = y;
		circles->circle[circles->count].r = r;
		circles->count++;
	}
}

OVALS *oval_set()
{
	return &__global_oval_set;
}

void oval_put(int x, int y, int a, int b)
{
	OVALS *ovals = oval_set();

	if (ovals->count < MAX_OBJECT_NUM) {
		ovals->oval[ovals->count].x = x;
		ovals->oval[ovals->count].y = y;
		ovals->oval[ovals->count].a = a;
		ovals->oval[ovals->count].b = b;
		ovals->count++;
	}
}

int lcs_len(char *a, char *b)
{
	int m, n, i, j, pos, **score;

	if ((m = strlen(a)) == 0 || (n = strlen(b)) == 0)
		return 0;

	score = (int **) malloc((m + 1) * sizeof(int));
	for (i = 0; i <= m; ++i)
		score[i] = (int *) malloc((n + 1) * sizeof(int));

	for (i = 0; i <= m; ++i)
		score[i][0] = 0;
	for (j = 0; j <= n; ++j)
		score[0][j] = 0;

	/* dynamic programming loop that computes the score arrays. */
	for (i = 1; i <= m; ++i) {
		for (j = 1; j <= n; ++j) {
			if (a[i - 1] == b[j - 1])
				score[i][j] = score[i - 1][j - 1] + 1;
			else
				score[i][j] = score[i - 1][j - 1] + 0;
			if (score[i - 1][j] >= score[i][j])
				score[i][j] = score[i - 1][j];
			if (score[i][j - 1] >= score[i][j])
				score[i][j] = score[i][j - 1];
		}
	}

	/* The length of the longest substring is score[m][n] */
	pos = score[m][n];

	for (i = 0; i <= m; ++i)
		free(score[i]);
	free(score);

	return pos;
}                                                   

RECTS *rect_set()
{
	return &__global_rect_set;
}

int rect_put(RECT *rect)
{
	RECTS *mrs = rect_set();

	// Too small objects, ignore !
	if (rect->h < 8 || rect->w < 8)
		return RET_ERROR;

	if (20 * rect->h < rect->w || 20 * rect->w < rect->h) // Too narrow
		return RET_ERROR;

	if (mrs->count < MAX_OBJECT_NUM) {
		mrs->rect[mrs->count] = *rect;
		mrs->count++;
		return RET_OK;
	}

	return RET_ERROR;
}

int rect_delete(int i)
{
	RECTS *mrs = rect_set();

	if (i >= 0 && i < mrs->count) {
		mrs->count--;
		mrs->rect[i] = mrs->rect[mrs->count];
	
		return RET_OK;
	}

	return RET_ERROR;
}


// return ms
TIME get_time()
{
	TIME ms;
	struct timeval t;
	
	gettimeofday(&t, NULL);
	ms = t.tv_sec * 1000 + t.tv_usec/1000;

	return ms;
}

void time_reset()
{
	__system_ms_time = get_time();
}

void time_spend(char *prompt)
{
	printf("%s spend %ld ms.\n", prompt, get_time() - __system_ms_time);

	time_reset();
}

