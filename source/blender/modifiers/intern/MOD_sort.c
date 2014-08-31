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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Jakub Zolcik
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_sort.c
 *  \ingroup modifiers
 */

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "depsgraph_private.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#include "BKE_dsort.h"


static void initData(ModifierData *md) {
	SortModifierData *smd = (SortModifierData *) md;

	smd->target_object = NULL;
	smd->coords = NULL;
	smd->vgroup[0] = '\0';

	smd->coords_num = 0;

	smd->faces = NULL;
	smd->faces_length = 0;
	smd->edges = NULL;
	smd->edges_length = 0;
	smd->verts = NULL;
	smd->verts_length = 0;

	smd->sort_type = MOD_SORT_TYPE_AXIS;
	smd->axis = MOD_SORT_AXIS_X;
	smd->use_original_mesh = false;

	smd->use_random = true;
	smd->random_seed = 0;

	smd->connected_first = false;
	smd->sort_verts = false;
	smd->sort_edges = false;
	smd->sort_faces = true;

	smd->auto_refresh = false;

	smd->is_sorted = false;

	smd->sort_initiated = true;
}

static void SortModifier_freeElemsOrder(SortModifierData *smd)
{
	if (smd->verts) {
		MEM_freeN(smd->verts);
		smd->verts = NULL;
		smd->verts_length = 0;
	}

	if (smd->edges) {
		MEM_freeN(smd->edges);
		smd->edges = NULL;
		smd->edges_length = 0;
	}

	if (smd->faces) {
		MEM_freeN(smd->faces);
		smd->faces = NULL;
		smd->faces_length = 0;
	}
}

static void SortModifier_freeData(SortModifierData *smd)
{

	SortModifier_freeElemsOrder(smd);

	if (smd->coords) {
		MEM_freeN(smd->coords);
		smd->coords = NULL;
		smd->coords_num = 0;
	}

	smd->is_sorted = false;
	smd->ui_info = MOD_SORT_NONE;

}

static void freeData(ModifierData *md) {

	SortModifierData *smd = (SortModifierData *) md;

	SortModifier_freeData(smd);

}

static void copyData(ModifierData *md, ModifierData *target) {

	SortModifierData *smd = (SortModifierData *) md;
	SortModifierData *tsmd = (SortModifierData *) target;
	int i;

	tsmd->target_object = smd->target_object;
	tsmd->coords = MEM_mallocN(sizeof(float)*3*smd->coords_num, "MOD_sort coords");
	for(i=0; i<smd->coords_num; i++)
		copy_v3_v3(tsmd->coords[i], smd->coords[i]);
	tsmd->coords_num = smd->coords_num;

	BLI_strncpy(tsmd->vgroup, smd->vgroup, sizeof(tsmd->vgroup));

	tsmd->sort_type = smd->sort_type;
	tsmd->axis = smd->axis;
	tsmd->use_original_mesh = smd->use_original_mesh;

	tsmd->use_random = smd->use_random;
	tsmd->random_seed = smd->random_seed;

	tsmd->connected_first = smd->connected_first;
	tsmd->sort_verts = smd->sort_verts;
	tsmd->sort_edges = smd->sort_edges;
	tsmd->sort_faces = smd->sort_faces;

	/* Inititate sort */
	tsmd->sort_initiated = true;

}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams)) {

	SortModifierData *smd = (SortModifierData *)md;

	if (smd->sort_type == MOD_SORT_TYPE_WEIGHTS)
			return !smd->vgroup[0];

	if (smd->sort_type == MOD_SORT_TYPE_OBJECT)
		return !smd->target_object;

	if (!smd->is_sorted && !smd->sort_initiated)
		return 1;

	return 0;

}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md) {
	SortModifierData *smd = (SortModifierData *) md;
	CustomDataMask dataMask;

	dataMask = 0;

	if (smd->vgroup[0])
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static DerivedMesh *SortModifier_do(ModifierData *md, Object *ob, DerivedMesh *dm)
{

	SortModifierData *smd;
	BMesh *bm;
	DerivedMesh *result;
	short auto_refresh;
	short initiate_sort;

	smd = (SortModifierData *)md;

	initiate_sort = smd->sort_initiated;

	auto_refresh = smd->auto_refresh;

	if (!initiate_sort) {
		if ((smd->verts && smd->verts_length != dm->numVertData) ||
			(smd->edges && smd->edges_length != dm->numEdgeData) ||
			(smd->faces && smd->faces_length != dm->numPolyData)) {

			SortModifier_freeElemsOrder(smd);

			if (auto_refresh)
				smd->sort_initiated = true;
			else {
				smd->ui_info = MOD_SORT_NONE;
				smd->is_sorted = false;
			}
		}
	}

	bm = NULL;

	if (initiate_sort && (smd->sort_verts || smd->sort_edges || smd->sort_faces)) {

		bm = DM_to_bmesh(dm, false);

		smd->ui_info = MOD_SORT_NONE;

		dsort_from_bm((ModifierData *)smd, ob, bm, smd->sort_type,
				smd->use_original_mesh, smd->connected_first, &smd->coords, &smd->coords_num,
				smd->axis, smd->vgroup, smd->target_object, smd->use_random, smd->random_seed,
				&smd->verts, &smd->edges, &smd->faces, NULL,
				smd->sort_verts, smd->sort_edges, smd->sort_faces, false);

		if (smd->verts) {
			smd->verts_length = bm->totvert;
			smd->ui_info |= MOD_SORT_VERTS;
		}

		if (smd->edges) {
			smd->edges_length = bm->totedge;
			smd->ui_info |= MOD_SORT_EDGES;
		}

		if (smd->faces) {
			smd->faces_length = bm->totface;
			smd->ui_info |= MOD_SORT_FACES;
		}

		smd->is_sorted = true;
		smd->sort_initiated = false;
	}

	if (!smd->is_sorted)
		return dm;

	if (!bm)
		bm = DM_to_bmesh(dm, false);

	BM_mesh_remap(bm, smd->verts, smd->edges, smd->faces);

	result = CDDM_from_bmesh(bm, false);
	BM_mesh_free(bm);

	return result;

}

static DerivedMesh *applyModifier(ModifierData *md, struct Object *ob,
		DerivedMesh *derivedData, int useRenderParams, int isFinalCalc) {

	return SortModifier_do(md, ob, derivedData);

}

static DerivedMesh *applyModifierEm(ModifierData *md, struct Object *ob,
		struct EditMesh *editData, struct DerivedMesh *derivedData) {

	return SortModifier_do(md, ob, derivedData);

}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk,
		void *userData) {

	SortModifierData *smd = (SortModifierData *) md;

	walk(userData, ob, &smd->target_object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
		struct Scene *UNUSED(scene), Object *UNUSED(ob), DagNode *obNode) {

	SortModifierData *smd = (SortModifierData *) md;

	if (smd->target_object)
		dag_add_relation(forest, dag_get_node(forest, smd->target_object), obNode,
				DAG_RL_DATA_DATA, "Sort Modifier");
}

ModifierTypeInfo modifierType_Sort = {
/* name */"Sort",
/* structName */"SortModifierData",
/* structSize */sizeof(SortModifierData),
/* type */eModifierTypeType_Nonconstructive,
/* flags */eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,

/* copyData */copyData,
/* deformVerts */NULL,
/* deformMatrices */NULL,
/* deformVertsEM */NULL,
/* deformMatricesEM */NULL,
/* applyModifier */applyModifier,
/* applyModifierEM */applyModifierEm,
/* initData */initData,
/* requiredDataMask */requiredDataMask,
/* freeData */freeData,
/* isDisabled */isDisabled,
/* updateDepgraph */updateDepgraph,
/* dependsOnTime */NULL,
/* dependsOnNormals */NULL,
/* foreachObjectLink */foreachObjectLink,
/* foreachIDLink */NULL,
/* foreachTexLink */NULL , };
