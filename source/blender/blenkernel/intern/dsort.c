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

#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"

#define DSORT_MAX_COORDS 10
#define DSORT_FLT_MAX 3.402823466e+38F


typedef struct FloatBMElem {
	float fl;
	void *el;
}FloatBMElem;


int BKE_dsort_compare_float_bm_elem(const void *p1, const void *p2)
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

char BKE_dsort_bm_elem_flag_test(void *elem, const char etype, const char flag)
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

void *BKE_dsort_bm_elem_at_index(BMesh *bm, const char etype, const int index)
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

void BKE_dsort_create_order_single_not_connected_first(int *elems_order, int length, const char etype, FloatBMElem *distances)
{
	int i;
	for(i = 0; i < length; i++)
		elems_order[BKE_dsort_get_bm_elem_index(distances[i].el, etype)] = i;
}

void BKE_dsort_create_order_single_connected_first(int *elems_order, int length, const char etype, int itype, int iitype, FloatBMElem *distances)
{
	int i, j, k, j_start, old_i, found, connection_level;
	/* connection level */
	int *connected;
	char *used;
	void *elem, *ielem, *iielem;
	int *q_distances;
	BMIter iter, iiter;

	connected = MEM_callocN(sizeof(int) * length, "BKE_dsort_create_order_single_connected_first connected");
	used = MEM_callocN(sizeof(char) * length, "BKE_dsort_create_order_single_connected_first used");

	// For faster lookups.
	q_distances = MEM_callocN(sizeof(int) * length, "BKE_dsort_create_order_single_connected_first q_distances");
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

void BKE_dsort_create_order_multiple_not_connected_first(int *elems_order, int length, const char etype, FloatBMElem **distances, int coords_num)
{

	int i, i_index, old_i;
	int indexes[DSORT_MAX_COORDS];
	char *used = (char *)MEM_callocN(sizeof(char) * length, "BKE_dsort_create_order_multiple_not_connected used");

	for(i=0; i<coords_num; i++)
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
		if (i_index >= coords_num)
			i_index = 0;
	}

	MEM_freeN(used);

}

void BKE_dsort_create_order_multiple_connected_first(int *elems_order, int length, const char etype, int itype, int iitype, FloatBMElem **distances, int coords_num)
{

	int i, j, k, n, old_i, found;
	/* connection level */
	int *j_start, *connection_level;
	int **connected;
	char *used;
	void *elem, *ielem, *iielem;
	int **q_distances;
	BMIter iter, iiter;

	connection_level = MEM_callocN(sizeof(int) * coords_num, "BKE_dsort_create_order_multiple_connected connection_level");
	j_start = MEM_callocN(sizeof(int) * coords_num, "BKE_dsort_create_order_multiple_connected j_start");

	connected = MEM_callocN(sizeof(int *) * coords_num, "BKE_dsort_create_order_multiple_connected connected");
	for(n=0; n<coords_num; n++)
		connected[n] = MEM_callocN(sizeof(int) * length, "BKE_dsort_create_order_multiple_connected connected[i]");

	used = MEM_callocN(sizeof(char) * length, "BKE_dsort_create_order_multiple_connected used");

	// For faster lookups.
	q_distances = MEM_callocN(sizeof(int *) * coords_num, "BKE_dsort_create_order_multiple_connected q_distances");
	for(n=0; n<coords_num; n++) {
		q_distances[n] = MEM_callocN(sizeof(int) * length, "BKE_dsort_create_order_multiple_connected q_distances[i]");

		for(i=0; i<length; i++)
			q_distances[n][i] = BKE_dsort_get_bm_elem_index(distances[n][i].el, etype);
	}

	for(n=0; n<coords_num; n++) {
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
		if (n >= coords_num)
			n = 0;

	}

	for(n=0; n<coords_num; n++) {
		MEM_freeN(connected[n]);
		MEM_freeN(q_distances[n]);
	}

	MEM_freeN(j_start);
	MEM_freeN(connection_level);
	MEM_freeN(connected);
	MEM_freeN(used);
	MEM_freeN(q_distances);

}

FloatBMElem *BKE_dsort_elems_from_axis(BMesh *bm, int length, const char etype, int mitype, int axis)
{

	int i;
	void *elem;
	BMIter iter;
	FloatBMElem *distances;
	float center[3];

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "BKE_dsort_order_from_dm distances");

	if (axis == MOD_SORT_AXIS_X) {
		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i].fl = center[0];
			distances[i].el = elem;
		}
	}
	else if (axis == MOD_SORT_AXIS_Y) {
		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i].fl = center[1];
			distances[i].el = elem;
		}
	}
	else if (axis == MOD_SORT_AXIS_Z) {
		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i].fl = center[2];
			distances[i].el = elem;
		}
	}

	qsort(distances, length, sizeof(FloatBMElem), BKE_dsort_compare_float_bm_elem);

	return distances;

}

FloatBMElem **BKE_dsort_elems_from_coords(BMesh *bm, int length, const char etype, int mitype, float (*coords)[3], int coords_num)
{

	int i, j;
	void *elem;
	BMIter iter;
	FloatBMElem **distances;
	float center[3];

	distances = MEM_mallocN(sizeof(FloatBMElem *) * coords_num, "BKE_dsort_elems_from_coords distances");

	for(i=0; i<coords_num; i++) {

		distances[i] = MEM_mallocN(sizeof(FloatBMElem) * length, "BKE_dsort_elems_from_coords distances[i]");

		BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, j) {
			BKE_dsort_get_bm_elem_center(elem, etype, center);
			distances[i][j].fl = len_squared_v3v3(coords[i], center);
			distances[i][j].el = elem;
		}

		qsort(distances[i], length, sizeof(FloatBMElem), BKE_dsort_compare_float_bm_elem);
	}

	return distances;

}

FloatBMElem **BKE_dsort_elems_from_selected(BMesh *bm, Object *ob, int length, const char etype, int mitype, float (**coords)[3], int *coords_num, short use_original_mesh)
{

	void *elem;
	BMIter iter;
	int temp_etype, temp_mitype, temp_coords_num;
	BMesh *temp_bm;

	*coords = MEM_mallocN(sizeof(float)*3, "BKE_dsort_elems_from_selected coords");
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

			if (BKE_dsort_bm_elem_flag_test(elem, temp_etype, BM_ELEM_SELECT)) {

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


	return BKE_dsort_elems_from_coords(bm, length, etype, mitype, *coords, *coords_num);

}

FloatBMElem **BKE_dsort_elems_from_cursor(BMesh *bm, int length, const char etype, int mitype, float (**coords)[3], ModifierData *md)
{
	*coords = MEM_mallocN(sizeof(float)*3, "BKE_dsort_elems_from_cursor coords");

	copy_v3_v3((*coords)[0], md->scene->cursor);

	return BKE_dsort_elems_from_coords(bm, length, etype, mitype, *coords, 1);
}

/* should */
FloatBMElem *BKE_dsort_elems_from_weights(BMesh *bm, int length, const char etype, int mitype, int itype, Object *ob, char vgroup[])
{
	void *elem;
	BMVert *vert;
	BMIter iter, iiter;
	MDeformVert *dvert;
	float weight;
	int defgrp_index;
	int i, count;
	FloatBMElem *distances;

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "BKE_dsort_order_from_weights distances");

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

	qsort(distances, length, sizeof(FloatBMElem), BKE_dsort_compare_float_bm_elem);

	printf("After weights sort");

	return distances;
}

void BKE_set_centers_array_from_dm(DerivedMesh *dm, const char etype, float (**elems_centers)[3],int *elems_centers_length) {

	int i;

	if (etype == BM_FACE) {
		MVert *vert_array;
		MLoop *loop_array;
		MPoly *poly;

		*elems_centers_length = dm->numPolyData;
		if (*elems_centers_length > 0) {
			vert_array = dm->getVertArray(dm);
			loop_array = dm->getLoopArray(dm);

			poly = dm->getPolyArray(dm);
			*elems_centers = MEM_mallocN(sizeof(float) * 3 * (*elems_centers_length), "BKE_get_centers_array_from_dm elems_centers");

			for (i = 0; i < *elems_centers_length; i++, poly++)
				BKE_mesh_calc_poly_center(poly, (loop_array + poly->loopstart), vert_array, (*elems_centers)[i]);

			return;
		}
	}

	if (etype == BM_EDGE) {
		*elems_centers_length = dm->numEdgeData;
		if (*elems_centers_length > 0) {
			return;
		}
	}

	if (etype == BM_VERT) {
		*elems_centers_length = dm->numVertData;

		return;
	}

	return;

}

/* need to be improved to work better with morph modifier */
FloatBMElem *BKE_dsort_elems_from_object(BMesh *bm, int length, const char etype, int mitype, DerivedMesh *target_dm, short use_random, int seed)
{

	int i, ordered_i, j, temp;
	void *elem, *closest_elem;
	float center[3];
	BMIter iter;
	FloatBMElem *distances;

	int *sort_order;
	int iteration;
	int target_i;
	char extra_i;
	float (*target_coords)[3];
	int target_coords_length;
	char *used_elems;
	int closest_elem_i;
	float current_dist, closest_dist;

	float tempco1[3], tempco2[3];

	/* calculating target coordinates */
	BKE_set_centers_array_from_dm(target_dm, etype, &target_coords, &target_coords_length);

	/* bool array determining if element is already assigned */
	used_elems = MEM_callocN(sizeof(char) * length, "BKE_dsort_elems_from_object used_elems");

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "BKE_dsort_order_from_dm distances");

	iteration = 0;

	while (iteration <= 1) {

		if (iteration == 0) {
			/* sort order */
			sort_order = MEM_mallocN(sizeof(int) * length, "BKE_dsort_elems_from_object sort_order");

			for (i = 0; i < length; i++)
				sort_order[i] = i;

			if (use_random) {

				printf("Randomizing sort order.\n");

				srand(seed);
				for (i = 0; i < length - 1; i++) {
					j = i + (rand() % (length - i));
					temp = sort_order[j];
					sort_order[j] = sort_order[i];
					sort_order[i] = temp;
				}
			}


		}

		for (i = 0; i < length; i++) {
			ordered_i = sort_order[i];

			target_i = (int)(((float)ordered_i / (float)length) * (float)target_coords_length);

			if ((int)(((float)(ordered_i - 1) / (float)length) * (float)target_coords_length) == target_i)
				extra_i = true;
			else
				extra_i = false;

			if (iteration == 0 && extra_i)
				continue;
			else if (iteration == 1 && !extra_i)
				continue;

			closest_elem = NULL;
			closest_elem_i = -1;
			closest_dist = DSORT_FLT_MAX;

			BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, j) {
				if (used_elems[j])
					continue;

				BKE_dsort_get_bm_elem_center(elem, etype, center);

				current_dist = len_squared_v3v3(center, target_coords[target_i]);

				if (current_dist < closest_dist) {
					closest_elem = elem;
					closest_elem_i = j;
					closest_dist = current_dist;
					copy_v3_v3(tempco1, center);
					copy_v3_v3(tempco2, target_coords[target_i]);
				}

			}

			/* distances[ordered_i].el = closest_elem; */
			distances[ordered_i].el = closest_elem;

			used_elems[closest_elem_i] = true;

		}

		iteration++;
	}

	MEM_freeN(sort_order);
	MEM_freeN(target_coords);
	MEM_freeN(used_elems);

	return distances;

}

FloatBMElem *BKE_dsort_elems_randomize(BMesh *bm, int length, int mitype, int random_seed)
{
	int i, j;
	BMIter iter;
	FloatBMElem *distances;
	void *temp_elem;
	int sort_order;

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "BKE_dsort_order_from_dm distances");

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

FloatBMElem *BKE_dsort_elems_none(BMesh *bm, int length, int mitype)
{
	int i;
	BMIter iter;
	void *elem;
	FloatBMElem *distances;

	distances = MEM_mallocN(sizeof(FloatBMElem) * length, "BKE_dsort_elems_none distances");

	BM_ITER_MESH_INDEX(elem, &iter, bm, mitype, i) {
		distances[i].el = elem;
	}

	return distances;
}

void BKE_dsort_elems(ModifierData *md, Object *ob, BMesh *bm, int sort_type,
				short use_original_mesh, short connected_first, float (**coords)[3], int *coords_num,
				int axis, char vgroup[], Object *target_object, short use_random, int random_seed,
				int **elems_order, int length,
				const char etype, int mitype, int itype, int iitype)
{

	int i;
	FloatBMElem **m_distances = NULL;
	FloatBMElem *s_distances = NULL;

	*elems_order = MEM_mallocN(sizeof(int) * length, "BKE_dsort_from_bm verts_order");

	if (sort_type == MOD_SORT_TYPE_AXIS) {
		*coords_num = 1;
		s_distances = BKE_dsort_elems_from_axis(bm, length, etype, mitype, axis);
	} else if (sort_type == MOD_SORT_TYPE_SELECTED) {
		if (*coords != NULL)
			m_distances = BKE_dsort_elems_from_coords(bm, length, etype, mitype, *coords, *coords_num);
		else
			m_distances = BKE_dsort_elems_from_selected(bm, ob, length, etype, mitype, coords, coords_num, use_original_mesh);
	} else if (sort_type == MOD_SORT_TYPE_CURSOR) {
		if (*coords != NULL)
			m_distances = BKE_dsort_elems_from_coords(bm, length, etype, mitype, *coords, *coords_num);
		else {
			*coords_num = 1;
			m_distances = BKE_dsort_elems_from_cursor(bm, length, etype, mitype, coords, md);
		}
	} else if (sort_type == MOD_SORT_TYPE_WEIGHTS) {
		*coords_num = 1;
		s_distances = BKE_dsort_elems_from_weights(bm, length, etype, mitype, itype, ob, vgroup);
	} else if (sort_type == MOD_SORT_TYPE_OBJECT) {
		DerivedMesh *target_dm;
		*coords_num = 1;
		target_dm = mesh_get_derived_final(md->scene, target_object, CD_MASK_MESH);
		s_distances = BKE_dsort_elems_from_object(bm, length, etype, mitype, target_dm, use_random, random_seed);
	} else if (sort_type == MOD_SORT_TYPE_RANDOM) {
		s_distances = BKE_dsort_elems_randomize(bm, length, mitype, random_seed);
	} else {
		s_distances = BKE_dsort_elems_none(bm, length, mitype);
	}


	if (*coords_num <= 1 && m_distances) {

		s_distances = m_distances[0];

		MEM_freeN(m_distances);

	}

	if (*coords_num <= 1) {

		if (!connected_first)
			BKE_dsort_create_order_single_not_connected_first(*elems_order, length, etype, s_distances);
		else
			BKE_dsort_create_order_single_connected_first(*elems_order, length, etype, itype, iitype, s_distances);

		MEM_freeN(s_distances);

	} else {

		if (!connected_first)
			BKE_dsort_create_order_multiple_not_connected_first(*elems_order, length, etype, m_distances, *coords_num);
		else
			BKE_dsort_create_order_multiple_connected_first(*elems_order, length, etype, itype, iitype, m_distances, *coords_num);

		for(i=0; i<*coords_num; i++)
			MEM_freeN(((FloatBMElem **)m_distances)[i]);

		MEM_freeN(m_distances);

	}

}

int BKE_dsort_get_new_index(int *elems_order, int length, int old_index)
{
	int i;

	for(i = 0; i < length; i++)
		if (elems_order[i] == old_index)
			return i;

	return -1;
}

void BKE_dsort_elems_from_elems(BMesh *bm, int **elems_order, int length, int *elems_from_order, int from_length,
								const char etype, const char eftype, int itype)
{
	void *elem_from, *elem;
	BMIter iter;
	int i, order_i, elem_i, elem_from_i;
	char *used_elems;

	*elems_order = MEM_mallocN(sizeof(int) * length, "BKE_dsort_elems_from_elems elems_order");
	used_elems = MEM_callocN(sizeof(char) * length, "BKE_dsort_elems_from_elems used_elems");

	order_i = 0;
	for (i = 0; i < from_length; i++) {
		elem_from_i = BKE_dsort_get_new_index(elems_from_order, from_length, i);
		elem_from = BKE_dsort_bm_elem_at_index(bm, eftype, elem_from_i);
		BM_ITER_ELEM(elem, &iter, elem_from, itype) {
			elem_i = BKE_dsort_get_bm_elem_index(elem, etype);
			if (!used_elems[elem_i]) {
				(*elems_order)[elem_i] = order_i;
				order_i++;
				used_elems[elem_i] = true;
			}
		}
	}

	MEM_freeN(used_elems);

}

void BKE_dsort_edges_from_verts(BMesh *bm, int **edges_order, int *verts_order)
{
	BKE_dsort_elems_from_elems(bm, edges_order, bm->totedge, verts_order, bm->totvert,
								BM_EDGE, BM_VERT, BM_EDGES_OF_VERT);
}

void BKE_dsort_faces_from_verts(BMesh *bm, int **faces_order, int *verts_order)
{
	BKE_dsort_elems_from_elems(bm, faces_order, bm->totface, verts_order, bm->totvert,
								BM_FACE, BM_VERT, BM_FACES_OF_VERT);
}

void BKE_dsort_faces_from_edges(BMesh *bm, int **faces_order, int *edges_order)
{
	BKE_dsort_elems_from_elems(bm, faces_order, bm->totface, edges_order, bm->totedge,
								BM_FACE, BM_EDGE, BM_FACES_OF_EDGE);
}

void dsort_from_bm(ModifierData *md, Object *ob, BMesh *bm, int sort_type,
					short use_original_mesh, short connected_first, float (**coords)[3], int *coords_num,
					int axis, char vgroup[], Object *target_object, short use_random, int random_seed,
					int **verts_order, int **edges_order, int **faces_order, int **loops_order,
					short sort_verts, short sort_edges, short sort_faces, short sort_loops)
{

	printf("Started sorting\n");

	if (sort_verts) {
		BKE_dsort_elems(md, ob, bm, sort_type,
						use_original_mesh, connected_first, coords, coords_num,
						axis, vgroup, target_object, use_random, random_seed,
						verts_order, bm->totvert,
						BM_VERT, BM_VERTS_OF_MESH, BM_EDGES_OF_VERT, BM_VERTS_OF_EDGE);
	}


	if (sort_edges) {
		if (sort_verts)
			BKE_dsort_edges_from_verts(bm, edges_order, *verts_order);
		else {
			BKE_dsort_elems(md, ob, bm, sort_type,
							use_original_mesh, connected_first, coords, coords_num,
							axis, vgroup, target_object, use_random, random_seed,
							edges_order, bm->totedge,
							BM_EDGE, BM_EDGES_OF_MESH, BM_VERTS_OF_EDGE, BM_EDGES_OF_VERT);
		}
	}


	if (sort_faces) {
		if (sort_verts)
			BKE_dsort_faces_from_verts(bm, faces_order, *verts_order);
		else if (sort_edges)
			BKE_dsort_edges_from_verts(bm, faces_order, *edges_order);
		else {
			BKE_dsort_elems(md, ob, bm, sort_type,
							use_original_mesh, connected_first, coords, coords_num,
							axis, vgroup, target_object, use_random, random_seed,
							faces_order, bm->totface,
							BM_FACE, BM_FACES_OF_MESH, BM_VERTS_OF_FACE, BM_FACES_OF_VERT);
		}
	}

}

