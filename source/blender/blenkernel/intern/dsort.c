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

/** \file blender/blenkernel/intern/sort.c
 *  \ingroup bke
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_dsort_types.h"

#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"

#include "BLI_string.h"

#define DSORT_MAX_COORDS 10
#define DSORT_FLT_MAX 3.402823466e+38F


typedef struct FloatBMElem {
	float fl;
	void *el;
}FloatBMElem;


int dsort_compare_float_bm_elem(const void *p1, const void *p2)
{
	FloatBMElem fi1 = *(FloatBMElem *)p1;
	FloatBMElem fi2 = *(FloatBMElem *)p2;

	if (fi1.fl < fi2.fl)
		return -1;
	else if (fi1.fl > fi2.fl)
		return 1;

	return 0;
}

int BKE_dsort_get_bm_elem_index(void *elem, const char etype)
{
	switch (etype) {
		case BM_VERT:
			return ((BMVert *)elem)->head.index;
		case BM_EDGE:
			return ((BMEdge *)elem)->head.index;
		case BM_FACE:
			return ((BMFace *)elem)->head.index;
		default:
			return -1;
	}
}

void BKE_dsort_get_bm_elem_center(void *elem, const char etype, float center[3])
{
	switch (etype) {
		case BM_VERT:
			copy_v3_v3(center, ((BMVert *)elem)->co);
			break;
		case BM_EDGE:
			add_v3_v3v3(center, ((BMEdge *)elem)->v1->co, ((BMEdge *)elem)->v2->co);
			mul_v3_fl(center, 0.5f);
			break;
		case BM_FACE:
			BM_face_calc_center_mean((BMFace *)elem, center);
			break;
		default:
			zero_v3(center);
			break;
	}
}

int BKE_dsort_get_bm_elems_length(BMesh *bm, const char etype)
{
	switch (etype) {
		case BM_VERT:
			return bm->totvert;
		case BM_EDGE:
			return bm->totedge;
		case BM_FACE:
			return bm->totedge;
		default:
			return -1;
	}
}

char dsort_bm_elem_flag_test(void *elem, const char etype, const char flag)
{
	switch (etype) {
		case BM_VERT:
			return ((BMVert *)elem)->head.hflag & flag;
		case BM_EDGE:
			return ((BMEdge *)elem)->head.hflag & flag;
		case BM_FACE:
			return ((BMFace *)elem)->head.hflag & flag;
		default:
			return 0;
	}
}

void *dsort_bm_elem_at_index(BMesh *bm, const char etype, const int index)
{
	switch (etype) {
		case BM_VERT:
			return (void *)BM_vert_at_index(bm, index);
		case BM_EDGE:
			return (void *)BM_edge_at_index(bm, index);
		case BM_FACE:
			return (void *)BM_face_at_index(bm, index);
		default:
			return NULL;
	}
}

void dsort_create_order_single_not_connected_first(int *elems_order, int length, const char etype, FloatBMElem *distances)
{
	int i;
	for(i = 0; i < length; i++)
		elems_order[BKE_dsort_get_bm_elem_index(distances[i].el, etype)] = i;
}

void dsort_create_order_single_connected_first(int *elems_order, int length, const char etype, int itype, int iitype, FloatBMElem *distances)
{
	int i, j, k, j_start, old_i, found, connection_level;
	/* connection level */
	int *connected;
	char *used;
	void *elem, *ielem, *iielem;
	int *q_distances;
	BMIter iter, iiter;

	connected = MEM_callocN(sizeof(int) * length, "dsort_create_order_single_connected_first connected");
	used = MEM_callocN(sizeof(char) * length, "dsort_create_order_single_connected_first used");

	// For faster lookups.
	q_distances = MEM_callocN(sizeof(int) * length, "dsort_create_order_single_connected_first q_distances");
	for(i=0; i<length; i++)
		q_distances[i] = BKE_dsort_get_bm_elem_index(distances[i].el, etype);


	j_start = 1;
	connection_level = 1;

	elems_order[q_distances[0]] = 0;
	elem = distances[0].el;


	BM_ITER_ELEM(ielem, &iter, elem, itype) {
		BM_ITER_ELEM(iielem, &iiter, ielem, iitype) {
			if (iielem != elem) {
				j = BKE_dsort_get_bm_elem_index(iielem, etype);
				if (!connected[j])
					connected[j] = connection_level;
			}
		}
	}

	for(i=1; i<length; i++) {

		while(used[q_distances[j_start]])
			j_start++;

		found = -1;

		for(k=0; k<2; k++) {

			for(j=j_start; j<length; j++) {
				old_i = q_distances[j];
				if (connected[old_i] == connection_level && !used[old_i]) {
					found = j;
					break;
				}
			}

			if (found != -1)
				break;

			if (k == 0)
				connection_level++;

		}


		if (found == -1) {
			old_i = q_distances[j_start];
			found = j_start;
		}

		elem = distances[found].el;
		BM_ITER_ELEM(ielem, &iter, elem, itype) {
			BM_ITER_ELEM(iielem, &iiter, ielem, iitype) {
				if (iielem != elem) {
					j = BKE_dsort_get_bm_elem_index(iielem, etype);
					if (!connected[j])
						connected[j] = connection_level+1;
				}
			}
		}

		used[old_i] = true;
		elems_order[old_i] = i;

	}

	MEM_freeN(connected);
	MEM_freeN(used);
	MEM_freeN(q_distances);

}

void dsort_create_order_multiple_not_connected_first(int *elems_order, int length, const char etype, FloatBMElem **distances, int distances_num)
{

	int i, i_index, old_i;
	int indexes[DSORT_MAX_COORDS];
	char *used = (char *)MEM_callocN(sizeof(char) * length, "dsort_create_order_multiple_not_connected used");

	for(i = 0; i < distances_num; i++)
		indexes[i] = 0;

	i_index = 0;
	for(i=0; i<length; i++) {
		old_i = BKE_dsort_get_bm_elem_index(distances[i_index][indexes[i_index]].el, etype);
		while (used[old_i]) {
			indexes[i_index]++; //Think it should work without errors.
			old_i = BKE_dsort_get_bm_elem_index(distances[i_index][indexes[i_index]].el, etype);
		}
		elems_order[old_i] = i;
		used[old_i] = true;
		indexes[i_index]++;
		i_index++;
		if (i_index >= distances_num)
			i_index = 0;
	}

	MEM_freeN(used);

}

void dsort_create_order_multiple_connected_first(int *elems_order, int length, const char etype, int itype, int iitype, FloatBMElem **distances, int distances_num)
{

	int i, j, k, n, old_i, found;
	/* connection level */
	int *j_start, *connection_level;
	int **connected;
	char *used;
	void *elem, *ielem, *iielem;
	int **q_distances;
	BMIter iter, iiter;

	connection_level = MEM_callocN(sizeof(int) * distances_num, "dsort_create_order_multiple_connected connection_level");
	j_start = MEM_callocN(sizeof(int) * distances_num, "dsort_create_order_multiple_connected j_start");

	connected = MEM_callocN(sizeof(int *) * distances_num, "dsort_create_order_multiple_connected connected");
	for(n = 0; n < distances_num; n++)
		connected[n] = MEM_callocN(sizeof(int) * length, "dsort_create_order_multiple_connected connected[i]");

	used = MEM_callocN(sizeof(char) * length, "dsort_create_order_multiple_connected used");

	// For faster lookups.
	q_distances = MEM_callocN(sizeof(int *) * distances_num, "dsort_create_order_multiple_connected q_distances");
	for(n=0; n < distances_num; n++) {
		q_distances[n] = MEM_callocN(sizeof(int) * length, "dsort_create_order_multiple_connected q_distances[i]");

		for(i=0; i<length; i++)
			q_distances[n][i] = BKE_dsort_get_bm_elem_index(distances[n][i].el, etype);
	}

	for(n=0; n < distances_num; n++) {
		j_start[n] = 1;
		connection_level[n] = 1;

		elems_order[q_distances[n][0]] = 0;
		elem = distances[n][0].el;


		BM_ITER_ELEM(ielem, &iter, elem, itype) {
			BM_ITER_ELEM(iielem, &iiter, ielem, iitype) {
				if (iielem != elem) {
					j = BKE_dsort_get_bm_elem_index(iielem, etype);
					if (!connected[n][j])
						connected[n][j] = connection_level[n];
				}
			}
		}
	}


	n = 0;

	for(i=1; i<length; i++) {

		while(used[q_distances[n][j_start[n]]])
			j_start[n]++;

		found = -1;

		for(k=0; k<2; k++) {


			for(j=j_start[n]; j<length; j++) {
				old_i = q_distances[n][j];
				if (connected[n][old_i] == connection_level[n] && !used[old_i]) {
					found = j;
					break;
				}
			}

			if (found != -1)
				break;

			if (k == 0)
				connection_level[n]++;
		}


		if (found == -1) {
			old_i = q_distances[n][j_start[n]];
			found = j_start[n];
		}

		elem = distances[n][found].el;
		BM_ITER_ELEM(ielem, &iter, elem, itype) {
			BM_ITER_ELEM(iielem, &iiter, ielem, iitype) {
				if (iielem != elem) {
					j = BKE_dsort_get_bm_elem_index(iielem, etype);
					if (!connected[n][j])
						connected[n][j] = connection_level[n]+1;
				}
			}
		}

		used[old_i] = true;
		elems_order[old_i] = i;

		n++;
		if (n >= distances_num)
			n = 0;

	}

	for(n = 0; n < distances_num; n++) {
		MEM_freeN(connected[n]);
		MEM_freeN(q_distances[n]);
	}

	MEM_freeN(j_start);
	MEM_freeN(connection_level);
	MEM_freeN(connected);
	MEM_freeN(used);
	MEM_freeN(q_distances);

}

FloatBMElem *dsort_elems_from_axis(BMesh *bm, int length, const char etype, int mitype, int axis)
{

	int i;
	void *elem;
	BMIter iter;
	FloatBMElem *distances;
	float center[3];

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "dsort_order_from_dm distances");

	if (axis == DSORT_AXIS_X) {
		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i].fl = center[0];
			distances[i].el = elem;
		}
	}
	else if (axis == DSORT_AXIS_Y) {
		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i].fl = center[1];
			distances[i].el = elem;
		}
	}
	else if (axis == DSORT_AXIS_Z) {
		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i].fl = center[2];
			distances[i].el = elem;
		}
	}

	qsort(distances, length, sizeof(FloatBMElem), dsort_compare_float_bm_elem);

	return distances;

}

FloatBMElem **dsort_elems_from_coords(BMesh *bm, int length, const char etype, int mitype, float (*coords)[3], int coords_num)
{

	int i, j;
	void *elem;
	BMIter iter;
	FloatBMElem **distances;
	float center[3];

	distances = MEM_mallocN(sizeof(FloatBMElem *) * coords_num, "dsort_elems_from_coords distances");

	for(i=0; i<coords_num; i++) {

		distances[i] = MEM_mallocN(sizeof(FloatBMElem) * length, "dsort_elems_from_coords distances[i]");

		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, j) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i][j].fl = len_squared_v3v3(coords[i], center);
			distances[i][j].el = elem;
		}

		qsort(distances[i], length, sizeof(FloatBMElem), dsort_compare_float_bm_elem);
	}

	return distances;

}

FloatBMElem **dsort_elems_from_selected(BMesh *bm, Object *ob, int length, const char etype, int mitype, float (**coords)[3], int *coords_num, short use_original_mesh)
{

	void *elem;
	BMIter iter;
	int temp_etype, temp_mitype, temp_coords_num;
	BMesh *temp_bm;

	*coords = MEM_mallocN(sizeof(float)*3, "dsort_elems_from_selected coords");
	temp_coords_num = 1;
	*coords_num = 0;

	temp_etype = etype;
	temp_mitype = mitype;

	if (use_original_mesh){
		temp_bm = BM_mesh_create(&bm_mesh_allocsize_default);
		BM_mesh_bm_from_me(temp_bm,(Mesh *)ob->data, false, 0, false);
	} else
		temp_bm = bm;

	while (*coords_num == 0 && temp_bm) {

		BM_ITER_MESH(elem, &iter, temp_bm, temp_mitype) {

			if (dsort_bm_elem_flag_test(elem, temp_etype, BM_ELEM_SELECT)) {

				if (*coords_num >= temp_coords_num) {

					temp_coords_num++;
					*coords = MEM_reallocN(*coords, sizeof(float)*3*temp_coords_num);

				}

				BKE_dsort_get_bm_elem_center(elem, temp_etype, (*coords)[*coords_num]);

				*coords_num += 1;
				if (*coords_num >= DSORT_MAX_COORDS)
					break;
			}
		}

		if (temp_etype == BM_FACE) {
			temp_etype = BM_EDGE;
			temp_mitype = BM_EDGES_OF_MESH;
		}
		else if (temp_etype == BM_EDGE) {
			temp_etype = BM_VERT;
			temp_mitype = BM_VERTS_OF_MESH;
		}
		else if (!use_original_mesh && *coords_num == 0) {
			/* Nothing selected? Give it a try on not modified Mesh */
			temp_bm = BM_mesh_create(&bm_mesh_allocsize_default);
			BM_mesh_bm_from_me(temp_bm,(Mesh *)ob->data, false, 0, false);
			use_original_mesh = true;
			temp_etype = etype;
			temp_mitype = mitype;
		} else
			break;

	}

	if (use_original_mesh)
		BM_mesh_free(temp_bm);

	if (*coords_num == 0) {

		*coords[0][0] = 0.0f;
		*coords[0][1] = 0.0f;
		*coords[0][2] = 0.0f;
		*coords_num += 1;
	}


	return dsort_elems_from_coords(bm, length, etype, mitype, *coords, *coords_num);

}

FloatBMElem **dsort_elems_from_cursor(BMesh *bm, int length, const char etype, int mitype, float (**coords)[3], ModifierData *md)
{
	*coords = MEM_mallocN(sizeof(float)*3, "dsort_elems_from_cursor coords");

	copy_v3_v3((*coords)[0], md->scene->cursor);

	return dsort_elems_from_coords(bm, length, etype, mitype, *coords, 1);
}

FloatBMElem *dsort_elems_from_weights(BMesh *bm, int length, const char etype, int mitype, int itype, Object *ob, char vgroup[])
{
	void *elem;
	BMVert *vert;
	BMIter iter, iiter;
	MDeformVert *dvert;
	float weight;
	int defgrp_index;
	int i, count;
	FloatBMElem *distances;

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "dsort_order_from_weights distances");

	defgrp_index = defgroup_name_index(ob, vgroup);

	BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
		if (etype == BM_VERT) {
			vert = (BMVert *)elem;
			dvert = CustomData_bmesh_get(&bm->vdata, vert->head.data, CD_MDEFORMVERT);
			weight = defvert_find_weight(dvert, defgrp_index);
		} else {
			count = 0;
			weight = 0.0;
			BM_ITER_ELEM(vert, &iiter, elem, itype) {
				dvert = CustomData_bmesh_get(&bm->vdata, vert->head.data, CD_MDEFORMVERT);
				weight += defvert_find_weight(dvert, defgrp_index);
				count++;
			}
			weight = weight / (float)count;
		}

		distances[i].el = elem;
		distances[i].fl = -weight;
	}

	qsort(distances, length, sizeof(FloatBMElem), dsort_compare_float_bm_elem);

	printf("After weights sort");

	return distances;
}

FloatBMElem *dsort_elems_randomize(BMesh *bm, int length, int mitype, int random_seed)
{
	int i, j;
	BMIter iter;
	FloatBMElem *distances;
	void *temp_elem;

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "dsort_order_from_dm distances");

	BM_ITER_MESH_INDEX(temp_elem, &iter, bm, mitype, i) {
		distances[i].el = temp_elem;
	}

	srand(random_seed);
	for (i = 0; i < length - 1; i++) {
		j = i + (rand() % (length - i));
		temp_elem = distances[j].el;
		distances[j].el = distances[i].el;
		distances[i].el = temp_elem;
	}

	return distances;
}

FloatBMElem *dsort_elems_none(BMesh *bm, int length, int mitype)
{
	int i;
	BMIter iter;
	void *elem;
	FloatBMElem *distances;

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "dsort_elems_none distances");

	BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
		distances[i].el = elem;
	}

	return distances;
}

void dsort_elems(ModifierData *md,BMesh *bm, DSortSettings *settings,
				int **p_elems_order, int length,
				const char etype, int mitype, int itype, int iitype)
{

	int i, distances_num;
	FloatBMElem **m_distances = NULL;
	FloatBMElem *s_distances = NULL;

	*p_elems_order = MEM_mallocN(sizeof(int) * length, "dsort_from_bm verts_order");

	if (settings->sort_type == DSORT_TYPE_AXIS) {
		s_distances = dsort_elems_from_axis(bm, length, etype, mitype, settings->axis);
		distances_num = 1;
	} else if (settings->sort_type == DSORT_TYPE_SELECTED) {
		if (settings->coords != NULL)
			m_distances = dsort_elems_from_coords(bm, length, etype, mitype, settings->coords, settings->coords_num);
		else
			m_distances = dsort_elems_from_selected(bm, settings->object, length, etype, mitype, &settings->coords, &settings->coords_num, settings->use_original_mesh);
		distances_num = settings->coords_num;
	} else if (settings->sort_type == DSORT_TYPE_CURSOR) {
		if (settings->coords != NULL)
			m_distances = dsort_elems_from_coords(bm, length, etype, mitype, settings->coords, settings->coords_num);
		else {
			settings->coords_num = 1;
			m_distances = dsort_elems_from_cursor(bm, length, etype, mitype, &settings->coords, md);
		}
		distances_num = settings->coords_num;
	} else if (settings->sort_type == DSORT_TYPE_WEIGHTS) {
		s_distances = dsort_elems_from_weights(bm, length, etype, mitype, itype, settings->object, settings->vgroup);
		distances_num = 1;
	} else if (settings->sort_type == DSORT_TYPE_RANDOM) {
		s_distances = dsort_elems_randomize(bm, length, mitype, settings->random_seed);
		distances_num = 1;
	} else {
		s_distances = dsort_elems_none(bm, length, mitype);
		distances_num = 1;
	}


	if (distances_num <= 1 && m_distances) {
		s_distances = m_distances[0];
		MEM_freeN(m_distances);
	}

	if (distances_num <= 1) {
		if (!settings->connected_first)
			dsort_create_order_single_not_connected_first(*p_elems_order, length, etype, s_distances);
		else
			dsort_create_order_single_connected_first(*p_elems_order, length, etype, itype, iitype, s_distances);

		MEM_freeN(s_distances);
	} else {
		if (!settings->connected_first)
			dsort_create_order_multiple_not_connected_first(*p_elems_order, length, etype, m_distances, distances_num);
		else
			dsort_create_order_multiple_connected_first(*p_elems_order, length, etype, itype, iitype, m_distances, distances_num);

		for(i = 0; i < distances_num; i++)
			MEM_freeN(((FloatBMElem **)m_distances)[i]);

		MEM_freeN(m_distances);
	}

}

int dsort_get_new_index(int *elems_order, int length, int old_index)
{
	int i;

	for(i = 0; i < length; i++)
		if (elems_order[i] == old_index)
			return i;

	return -1;
}

void dsort_elems_from_elems(BMesh *bm, int **p_elems_order, int length, int *elems_from_order, int from_length,
								const char etype, const char eftype, int itype)
{
	void *elem_from, *elem;
	BMIter iter;
	int i, order_i, elem_i, elem_from_i;
	char *used_elems;

	*p_elems_order = MEM_mallocN(sizeof(int) * length, "dsort_elems_from_elems elems_order");
	used_elems = MEM_callocN(sizeof(char) * length, "dsort_elems_from_elems used_elems");

	order_i = 0;
	for (i = 0; i < from_length; i++) {
		elem_from_i = dsort_get_new_index(elems_from_order, from_length, i);
		elem_from = dsort_bm_elem_at_index(bm, eftype, elem_from_i);
		BM_ITER_ELEM(elem, &iter, elem_from, itype) {
			elem_i = BKE_dsort_get_bm_elem_index(elem, etype);
			if (!used_elems[elem_i]) {
				(*p_elems_order)[elem_i] = order_i;
				order_i++;
				used_elems[elem_i] = true;
			}
		}
	}

	MEM_freeN(used_elems);

}

void dsort_edges_from_verts(BMesh *bm, int **edges_order, int *verts_order)
{
	dsort_elems_from_elems(bm, edges_order, bm->totedge, verts_order, bm->totvert,
								BM_EDGE, BM_VERT, BM_EDGES_OF_VERT);
}

void dsort_faces_from_verts(BMesh *bm, int **faces_order, int *verts_order)
{
	dsort_elems_from_elems(bm, faces_order, bm->totface, verts_order, bm->totvert,
								BM_FACE, BM_VERT, BM_FACES_OF_VERT);
}

void dsort_faces_from_edges(BMesh *bm, int **faces_order, int *edges_order)
{
	dsort_elems_from_elems(bm, faces_order, bm->totface, edges_order, bm->totedge,
								BM_FACE, BM_EDGE, BM_FACES_OF_EDGE);
}

void dsort_reverse_order(int *elems_order, int length)
{
	int i, i_left, i_right, temp_length;
	int temp;

	temp_length = (int)length / 2;
	for(i = 0; i < temp_length; i++) {
		i_left = dsort_get_new_index(elems_order, length, i);
		i_right = dsort_get_new_index(elems_order, length, length - 1 - i);
		temp = elems_order[i_left];
		elems_order[i_left] = elems_order[i_right];
		elems_order[i_right] = temp;
	}
}

void BKE_dsort_set_elems_order(ModifierData *md, BMesh *bm, DSortSettings *settings,
					int **p_verts_order, int **p_edges_order, int **p_faces_order)
{
	if (settings->sort_elems & DSORT_ELEMS_VERTS) {
		dsort_elems(md, bm, settings,
					p_verts_order, bm->totvert,
					BM_VERT, BM_VERTS_OF_MESH, BM_EDGES_OF_VERT, BM_VERTS_OF_EDGE);
	}


	if (settings->sort_elems & DSORT_ELEMS_EDGES) {
		if (*p_verts_order)
			dsort_edges_from_verts(bm, p_edges_order, *p_verts_order);
		else {
			dsort_elems(md, bm, settings,
						p_edges_order, bm->totedge,
						BM_EDGE, BM_EDGES_OF_MESH, BM_VERTS_OF_EDGE, BM_EDGES_OF_VERT);
		}
	}


	if (settings->sort_elems & DSORT_ELEMS_FACES) {
		if (*p_verts_order)
			dsort_faces_from_verts(bm, p_faces_order, *p_verts_order);
		else if (*p_edges_order)
			dsort_edges_from_verts(bm, p_faces_order, *p_edges_order);
		else {
			dsort_elems(md, bm, settings,
						p_faces_order, bm->totface,
						BM_FACE, BM_FACES_OF_MESH, BM_VERTS_OF_FACE, BM_FACES_OF_VERT);
		}
	}

	/* reverse order if necessary */
	if (settings->reverse) {
		if (settings->sort_elems & DSORT_ELEMS_VERTS)
			dsort_reverse_order(*p_verts_order, bm->totvert);
		if (settings->sort_elems & DSORT_ELEMS_EDGES)
			dsort_reverse_order(*p_edges_order, bm->totedge);
		if (settings->sort_elems & DSORT_ELEMS_FACES)
			dsort_reverse_order(*p_faces_order, bm->totface);
	}

}

void BKE_dsort_free_elems_order(int **p_verts_order, int **p_edges_order, int **p_faces_order)
{
	int *verts_order = p_verts_order ? *p_verts_order : NULL;
	int *edges_order = p_edges_order ? *p_edges_order : NULL;
	int *faces_order = p_faces_order ? *p_faces_order : NULL;

	if (verts_order) {
		MEM_freeN(verts_order);
		verts_order = NULL;
	}

	if (edges_order) {
		MEM_freeN(edges_order);
		edges_order = NULL;
	}

	if (faces_order) {
		MEM_freeN(faces_order);
		faces_order = NULL;
	}
}

void BKE_dsort_free_data(DSortSettings *settings,
						 int **p_verts_order, int **p_edges_order, int **p_faces_order,
						 short *is_sorted, short *initiate_sort)
{
	BKE_dsort_free_elems_order(p_verts_order, p_edges_order, p_faces_order);

	if (settings->coords) {
		MEM_freeN(settings->coords);
		settings->coords = NULL;
	}

	*is_sorted = false;
	*initiate_sort = false;
}

char BKE_dsort_bm(ModifierData *md, BMesh *bm, DSortSettings *settings,
			int **p_verts_order, int **p_edges_order, int **p_faces_order, int **p_loops_order,
			int *p_verts_length, int *p_edges_length, int *p_faces_length, int *p_loops_length,
			short *is_sorted, short *initiate_sort, short auto_refresh)
{
	char order_changed = false;

	int *verts_order = p_verts_order ? *p_verts_order : NULL;
	int *edges_order = p_edges_order ? *p_edges_order : NULL;
	int *faces_order = p_faces_order ? *p_faces_order : NULL;

	int verts_length = p_verts_length ? *p_verts_length : 0;
	int edges_length = p_edges_length ? *p_edges_length : 0;
	int faces_length = p_faces_length ? *p_faces_length : 0;

	if (!(*initiate_sort)) {
		if (!(*is_sorted))
			return false;
		else {
		/* if mesh has changed free elems order */	
			if (((settings->sort_elems & DSORT_ELEMS_VERTS) && !(verts_order)) ||
				((settings->sort_elems & DSORT_ELEMS_EDGES) && !(edges_order)) ||
				((settings->sort_elems & DSORT_ELEMS_FACES) && !(faces_order)) ||
				(verts_order && verts_length != bm->totvert) ||
				(edges_order && edges_length != bm->totedge) ||
				(faces_order && faces_length != bm->totface)) {
					BKE_dsort_free_elems_order(p_verts_order, p_edges_order, p_faces_order);

					*is_sorted = false;
					if (auto_refresh)
						*initiate_sort = true;
					else
						return true;
			}
		}
	}

	/* Get new sort order. */
	if (*initiate_sort && (settings->sort_elems)) {
		BKE_dsort_set_elems_order((ModifierData *)md, bm, settings,
						p_verts_order, p_edges_order, p_faces_order);

		if (verts_order) {
			*p_verts_length = bm->totvert;
			*is_sorted = true;
		}

		if (edges_order) {
			*p_edges_length = bm->totedge;
			*is_sorted = true;
		}

		if (faces_order) {
			*p_faces_length = bm->totface;
			*is_sorted = true;
		}

		*initiate_sort = false;
		order_changed = true;
	}

	/* Remap mesh if it's needed. */
	if (settings->sort_type != DSORT_TYPE_NONE)
		BM_mesh_remap(bm, verts_order, edges_order, faces_order);

	return order_changed;
}

void BKE_copy_dsort_settings(DSortSettings *dss, DSortSettings *tdss)
{
	tdss->coords_num = dss->coords_num;

	if (dss->coords_num > 0) {
		tdss->coords = MEM_mallocN(sizeof(float)*3*dss->coords_num, "MOD_sort coords");
		memcpy(tdss->coords, dss->coords, sizeof(float)*3*dss->coords_num);
	} else
		tdss->coords = NULL;

	BLI_strncpy(tdss->vgroup, dss->vgroup, sizeof(tdss->vgroup));

	tdss->sort_type = dss->sort_type;
	tdss->axis = dss->axis;
	tdss->use_original_mesh = dss->use_original_mesh;

	tdss->random_seed = dss->random_seed;

	tdss->connected_first = dss->connected_first;
	tdss->sort_elems = dss->sort_elems;
}