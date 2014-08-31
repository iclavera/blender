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

struct BMesh;
struct FloatBMElem;

int BKE_dsort_compare_floatbmvert(const void *p1, const void *p2);
int BKE_dsort_get_bm_elem_index(void *elem, const char etype);
void BKE_dsort_get_bm_elem_center(void *elem, const char etype, float center[3]);
char BKE_dsort_bm_elem_flag_test(void *elem, const char etype, const char flag);
void *BKE_dsort_bm_elem_at_index(struct BMesh *bm, const char etype, const int index);

void BKE_dsort_create_order_single_not_connected_first(int *elems_order, int length, const char etype, struct FloatBMElem *distances);
void BKE_dsort_create_order_single_connected_first(int *elems_order, int length, const char etype, int itype, int iitype, struct FloatBMElem *distances);
void BKE_dsort_create_order_multiple_not_connected_first(int *elems_order, int length, const char etype, struct FloatBMElem **distances, int coords_num);
void BKE_dsort_create_order_multiple_connected_first(int *elems_order, int length, const char etype, int itype, int iitype, struct FloatBMElem **distances, int coords_num);

struct FloatBMElem *BKE_dsort_elems_from_axis(struct BMesh *bm, int length, const char etype, int mitype, int axis);
struct FloatBMElem **BKE_dsort_elems_from_coords(struct BMesh *bm, int length, const char etype, int mitype, float (*coords)[3], int *coords_num);
struct FloatBMElem **BKE_dsort_elems_from_selected(struct BMesh *bm, struct Object *ob, int length, const char etype, int mitype, float (**coords)[3], int *coords_num, short use_original_mesh);
struct FloatBMElem **BKE_dsort_elems_from_cursor(struct BMesh *bm, int length, const char etype, int mitype, float (**coords)[3], struct ModifierData *md);
struct FloatBMElem *BKE_dsort_elems_from_weights(struct BMesh *bm, int length, const char etype, int mitype, int itype, struct Object *ob, char vgroup[]);
void BKE_set_centers_array_from_dm(struct DerivedMesh *dm, const char etype, float (**elems_centers)[3], int *elems_centers_length);
struct FloatBMElem *BKE_dsort_elems_from_object(struct BMesh *bm, int length, const char etype, int mitype, struct DerivedMesh *target_dm, short use_random, int seed);
struct FloatBMElem *BKE_dsort_elems_randomize(struct BMesh *bm, int length, int mitype, int random_seed);
struct FloatBMElem *BKE_dsort_elems_none(struct BMesh *bm, int length, int mitype);

void BKE_dsort_elems(struct ModifierData *md, struct Object *ob, struct BMesh *bm, int sort_type,
					short use_original_mesh, short connected_first, float (**coords)[3], int *coords_num,
					int axis, char vgroup[], struct Object *target_object, short use_random, int random_seed,
					int **elems_order, int length,
					const char etype, int mitype, int itype, int iitype);

int BKE_dsort_get_new_index(int *elems_order, int length, int old_index);
void BKE_dsort_elems_from_elems(struct BMesh *bm, int **elems_order, int length, int *elems_from_order, int from_length,
								const char etype, const char eftype, int itype);

void BKE_dsort_edges_from_verts(struct BMesh *bm, int **edges_order, int *verts_order);
void BKE_dsort_faces_from_verts(struct BMesh *bm, int **faces_order, int *verts_order);
void BKE_dsort_faces_from_edges(struct BMesh *bm, int **faces_order, int *edges_order);

void dsort_from_bm(struct ModifierData *md, struct Object *ob, struct BMesh *bm, int sort_type,
			short use_original_mesh, short connected_first, float (**coords)[3], int *coords_num,
			int axis, char vgroup[], struct Object *target_object, short use_random, int random_seed,
			int **verts_order, int **edges_order, int **faces_order, int **loops_order,
			short sort_verts, short sort_edges, short sort_faces, short sort_loops);


#endif
