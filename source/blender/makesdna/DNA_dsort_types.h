/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_dsort_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_DSORT_TYPES_H__
#define __DNA_DSORT_TYPES_H__

/**
 * DSort settings made as separate structure. Seems like a better
 * idea as they are used both in Sort Modifier and Morph Modifier.
*/

typedef struct DSortSettings {	
	struct Object *object; /* Object reference */
	float (*coords)[3]; /* needs to be cached if auto_refresh is on */

	int coords_num;
	/* generall settings */
	int sort_type;
	short reverse;
	short pad1; /* for future use */

	/* for DSORT_TYPE_AXIS */
	int axis;
	/* for DSORT_TYPE_RANDOMIZE */
	int random_seed;
	/* for DSORT_TYPE_SELECTED */
	short use_original_mesh;
	short connected_first;
	/* for DSORT_TYPE_WEIGHTS */
	short vgroup;
	short pad2; /* for future use */

	int sort_elems;
} DSortSettings;

enum {
	DSORT_TYPE_AXIS  = 1,
	DSORT_TYPE_SELECTED  = 2,
	DSORT_TYPE_CURSOR  = 3,
	DSORT_TYPE_WEIGHTS  = 4,
	DSORT_TYPE_RANDOM = 5,
	DSORT_TYPE_NONE = 6
};

enum {
	DSORT_AXIS_X = 1,
	DSORT_AXIS_Y = 2,
	DSORT_AXIS_Z = 3
};

#define DSORT_ELEMS_VERTS	(1 << 0)
#define DSORT_ELEMS_EDGES	(1 << 1)
#define DSORT_ELEMS_FACES	(1 << 2)

#endif