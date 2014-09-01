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

#include "DNA_dsort_types.h"
#include "BKE_dsort.h"

static void SortModifier_initDSortSettings(DSortSettings *dss)
{
	dss->coords = NULL;
	dss->vgroup = 0;

	dss->coords_num = 0;

	/* general settings */
	dss->sort_type = DSORT_TYPE_AXIS;
	dss->reverse = false;

	/* for DSORT_TYPE_AXIS */
	dss->axis = DSORT_AXIS_X;

	/* for DSORT_TYPE_RANDOMIZE */
	dss->random_seed = 0;

	/* for DSORT_TYPE_SELECTED */
	dss->use_original_mesh = false;
	dss->connected_first = false;

	dss->sort_elems = DSORT_ELEMS_VERTS | DSORT_ELEMS_FACES;
}

static void initData(ModifierData *md)
{
	SortModifierData *smd = (SortModifierData *) md;

	smd->settings = MEM_callocN(sizeof(DSortSettings), "SortModifier settings");

	SortModifier_initDSortSettings(smd->settings);

	smd->faces_order = NULL;
	smd->edges_order = NULL;
	smd->verts_order = NULL;

	smd->faces_length = 0;
	smd->edges_length = 0;
	smd->verts_length = 0;

	smd->is_sorted = false; /* for UI */
	smd->initiate_sort = true;

	smd->auto_refresh = false;
}

static void SortModifier_freeData(SortModifierData *smd)
{
	/* Freeing dsort data */
	BKE_dsort_free_data(smd->settings, &smd->verts_order, &smd->edges_order, &smd->faces_order, &smd->is_sorted, &smd->initiate_sort);
}

static void freeData(ModifierData *md)
{
	SortModifierData *smd = (SortModifierData *) md;
	SortModifier_freeData(smd);	
	/* Freeing dsort settings */
	MEM_freeN(smd->settings);
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SortModifierData *smd = (SortModifierData *) md;
	SortModifierData *tsmd = (SortModifierData *) target;

	BKE_copy_dsort_settings(smd->settings, tsmd->settings);

	tsmd->verts_order = NULL;
	tsmd->verts_length = 0;

	tsmd->edges_order = NULL;
	tsmd->edges_length = 0;

	tsmd->faces_order = NULL;
	tsmd->faces_length = 0;

	/* Inititate sort */
	tsmd->is_sorted = false;
	tsmd->initiate_sort = true;
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	SortModifierData *smd = (SortModifierData *)md;

	if (smd->is_sorted || smd->initiate_sort)
		return false;

	if (smd->settings->sort_type == DSORT_TYPE_WEIGHTS)
		return !(bool)smd->settings->vgroup;

	return false;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	SortModifierData *smd = (SortModifierData *) md;
	CustomDataMask dataMask;

	dataMask = 0;

	if (smd->settings->vgroup)
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static DerivedMesh *SortModifier_do(ModifierData *md, Object *ob, DerivedMesh *dm)
{
	SortModifierData *smd;
	BMesh *bm;
	DerivedMesh *result;

	smd = (SortModifierData *)md;

	bm = DM_to_bmesh(dm, false);

	BKE_dsort_bm(md, bm, smd->settings,
					&smd->verts_order, &smd->edges_order, &smd->faces_order, NULL,
					&smd->verts_length, &smd->edges_length, &smd->faces_length, NULL,
					&smd->is_sorted, &smd->initiate_sort, smd->auto_refresh);

	if (!smd->is_sorted) {
		BM_mesh_free(bm);
		return dm;
	}

	smd->ui_info = MOD_SORT_NONE;

	if (smd->is_sorted) {
		if (smd->verts_order)
			smd->ui_info |= MOD_SORT_VERTS;
		if (smd->edges_order)
			smd->ui_info |= MOD_SORT_EDGES;
		if (smd->verts_order)
			smd->ui_info |= MOD_SORT_FACES;
	}

	result = CDDM_from_bmesh(bm, false);
	BM_mesh_free(bm);
	return result;
}

static DerivedMesh *applyModifier(ModifierData *md, struct Object *ob,
									DerivedMesh *derivedData, ModifierApplyFlag UNUSED(flag))
{
	return SortModifier_do(md, ob, derivedData);
}

static DerivedMesh *applyModifierEm(ModifierData *md, struct Object *ob,
									struct BMEditMesh *UNUSED(editData), struct DerivedMesh *derivedData,
									ModifierApplyFlag UNUSED(flag))
{
	return SortModifier_do(md, ob, derivedData);
}

ModifierTypeInfo modifierType_Sort = {
	/* name */				"Sort",
	/* structName */		"SortModifierData",
	/* structSize */		sizeof(SortModifierData),
	/* type */				eModifierTypeType_Nonconstructive,
	/* flags */				eModifierTypeFlag_AcceptsMesh | 
							eModifierTypeFlag_SupportsEditmode,

	/* copyData */			copyData,
	/* deformVerts */		NULL,
	/* deformMatrices */	NULL,
	/* deformVertsEM */		NULL,
	/* deformMatricesEM */	NULL,
	/* applyModifier */		applyModifier,
	/* applyModifierEM */	applyModifierEm,
	/* initData */			initData,
	/* requiredDataMask */	requiredDataMask,
	/* freeData */			freeData,
	/* isDisabled */		isDisabled,
	/* updateDepgraph */	NULL,
	/* dependsOnTime */		NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */	NULL,
	/* foreachIDLink */		NULL,
	/* foreachTexLink */	NULL,
};
