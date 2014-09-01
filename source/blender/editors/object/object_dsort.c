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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_dsort.c
 *
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_dsort_types.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_deform.h"
#include "BKE_dsort.h"
#include "BKE_DerivedMesh.h"

#include "RNA_define.h"
#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "UI_resources.h"

#include "object_intern.h"

#include "bmesh.h"


static EnumPropertyItem *sort_mod_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	Object *ob = CTX_data_edit_object(C);
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item = NULL;
	ModifierData *md = NULL;
	int a, totitem = 0;

	if (!ob)
		return DummyRNA_NULL_items;

	for (a = 0, md = ob->modifiers.first; md; md = md->next, a++) {
		if (md->type == eModifierType_Sort) {
			tmp.value = a;
			tmp.icon = ICON_MOD_SORT;
			tmp.identifier = md->name;
			tmp.name = md->name;
			RNA_enum_item_add(&item, &totitem, &tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int object_dsort_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_SortModifier);
	int num = RNA_enum_get(op->ptr, "modifier");
	Object *ob = NULL;
	SortModifierData *smd = NULL;

	/* for sorting */
	Scene *scene = CTX_data_scene(C);
	BMesh *bm = NULL;
	DerivedMesh *dm = NULL;


	if (ptr.data) {     /* if modifier context is available, use that */
		ob = ptr.id.data;
		smd = ptr.data;
	} else {          /* use the provided property */
		ob = CTX_data_active_object(C);
		smd = (SortModifierData *)BLI_findlink(&ob->modifiers, num);
	}

	if (!ob || !smd) {
		BKE_report(op->reports, RPT_ERROR, "Could not find sort modifier");
		return OPERATOR_CANCELLED;
	}

	if (smd->is_sorted) {
		BKE_report(op->reports, RPT_ERROR, "Free sort first.");
		return OPERATOR_CANCELLED;
	}

	smd->initiate_sort = true;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_dsort(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Dynamic Sort";
	ot->description = "Dynamic Sort";
	ot->idname = "OBJECT_OT_dsort";

	ot->exec = object_dsort_exec;
	ot->poll = ED_operator_object_active_editable_mesh;

	ot->flag = /*OPTYPE_REGISTER |*/ OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to assign to");
	RNA_def_enum_funcs(prop, *sort_mod_itemf);
}

static int object_dsort_free_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_SortModifier);
	int num = RNA_enum_get(op->ptr, "modifier");
	Object *ob = NULL;
	SortModifierData *smd = NULL;

	if (ptr.data) {     /* if modifier context is available, use that */
		ob = ptr.id.data;
		smd = ptr.data;
	} else {          /* use the provided property */
		ob = CTX_data_active_object(C);
		smd = (SortModifierData *)BLI_findlink(&ob->modifiers, num);
	}

	if (!ob || !smd) {
		BKE_report(op->reports, RPT_ERROR, "Could not find sort modifier");
		return OPERATOR_CANCELLED;
	}

	/* Freeing dsort data */
	BKE_dsort_free_data(smd->settings,
						&smd->verts_order, &smd->edges_order, &smd->faces_order,
						&smd->is_sorted, &smd->initiate_sort);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_dsort_free(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "DSort Free";
	ot->description = "Dynamic Sort Free";
	ot->idname = "OBJECT_OT_dsort_free";

	ot->exec = object_dsort_free_exec;
	ot->poll = ED_operator_object_active_editable_mesh;

	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->flag = /*OPTYPE_REGISTER |*/ OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to assign to");
	RNA_def_enum_funcs(prop, sort_mod_itemf);
}
