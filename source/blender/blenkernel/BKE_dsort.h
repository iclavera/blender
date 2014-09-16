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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_DSORT_H__
#define __BKE_DSORT_H__

/** \file BKE_dsort.h
 *  \ingroup bke
 */

/* Used by morph.c */
int BKE_dsort_get_bm_elem_index(void *elem, const char etype);
void BKE_dsort_get_bm_elem_center(void *elem, const char etype, float center[3]);
int BKE_dsort_get_bm_elems_length(struct BMesh *bm, const char etype);

/* Used by morph.c and Sort Modifier */
void BKE_dsort_set_elems_order(struct ModifierData *md, struct BMesh *bm, struct Object *ob,
								struct DSortSettings *settings,
								int **p_verts_order, int **p_edges_order, int **p_faces_order);

void BKE_dsort_free_elems_order(int **p_verts_order, int **p_edges_order, int **p_faces_order);
void BKE_dsort_free_data(struct DSortSettings *settings, 
							int **p_verts_order, int **p_edges_order, int **p_faces_order, 
							short *is_sorted, short *initiate_sort);

char BKE_dsort_bm(struct ModifierData *md, struct BMesh *bm, struct Object *ob, struct DSortSettings *settings,
					int **p_verts_order, int **p_edges_order, int **p_faces_order, int **p_loops_order,
					int *p_verts_length, int *p_edges_length, int *p_faces_length, int *p_loops_length,
					short *is_sorted, short *initiate_sort, short auto_refresh);

void BKE_copy_dsort_settings(struct DSortSettings *dss, struct DSortSettings *tdss);

#endif
