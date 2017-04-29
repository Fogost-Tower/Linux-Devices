/*
 *  tslib/plugins/mousebuts.c
 *
 *  Copyright (C) 2003 Cacko Team (http://www.cacko.biz), 2003
 *
 * This file is placed under the LGPL.  Please see the file
 * COPYING for more details.
 *
 * Touchscreen mouse buttons emulation
 */
 
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#include <stdio.h>

#include "tslib.h"
#include "tslib-filter.h"

#define NR_LAST	4

struct tslib_mousebuts {
	struct tslib_module_info	module;
	int		x, y;
	long		time;
	int 		pressure;
	int		fMotion;
	int		fLeftBut;
	int		fReset;
};

static int mousebuts_read(struct tslib_module_info *info, struct ts_sample *samp, int nr)
{
	struct tslib_mousebuts *buts = (struct tslib_mousebuts *)info;
	struct ts_sample *src = samp, *dest = samp;
	int ret, i = 0;
	long t;
	struct timeval tv;

	ret = info->next->ops->read(info->next, samp, nr);
	if (ret >= 0) {
	    if (buts->fLeftBut && (src->pressure == 0)) {
		buts->fReset = 1;
		buts->time = 0;
		buts->pressure = 0;
		if (--buts->fLeftBut) dest->pressure = 500;
		else {
		    buts->time = 0;
		}
	    } else if (buts->fReset && src->pressure) {
		buts->fLeftBut = 0;
		buts->x = src->x;
		buts->y = src->y;
		buts->time = src->tv.tv_sec * 1000 + src->tv.tv_usec;
		dest->pressure = 0;
		buts->pressure = 0;
		buts->fMotion = 0;
		buts->fReset = 0;
	    } else if (src->pressure) {
		if ((abs(src->x - buts->x) < 5) &&
		    (abs(src->y - buts->y) < 5) &&
		    (buts->fMotion == 0)) {
		    t = (src->tv.tv_sec * 1000 + src->tv.tv_usec) - buts->time;
		    if (t > 60) {
			dest->pressure = 1000;
			buts->fLeftBut = 0;
		    } else {
			dest->pressure = 0;
			buts->fLeftBut = 2;
		    }
		    buts->pressure = dest->pressure;
		} else {
		    buts->x = src->x;
		    buts->y = src->y;
		    buts->time = src->tv.tv_sec * 1000 + src->tv.tv_usec;
		    dest->pressure = buts->pressure?buts->pressure:500;
		    buts->fMotion = 1;
		}
	    } else if (buts->fReset == 0) {
		buts->fReset = 1;
	    }
	}
	return ret;
}

static int mousebuts_fini(struct tslib_module_info *info)
{
	free(info);
}

static const struct tslib_ops mousebuts_ops =
{
	read:	mousebuts_read,
	fini:	mousebuts_fini,
};

struct tslib_module_info *mod_init(struct tsdev *dev, const char *params)
{
	struct tslib_mousebuts *buts;

	buts = malloc(sizeof(struct tslib_mousebuts));
	if (buts == NULL)
		return NULL;

	buts->module.ops = &mousebuts_ops;
	buts->x = -1;
	buts->y = -1;
	buts->time = 0;
	buts->pressure = 0;
	buts->fMotion = 0;
	buts->fLeftBut = 0;
	buts->fReset = 1;

	return &buts->module;
}
