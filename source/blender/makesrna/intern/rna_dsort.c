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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_dsort.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <limits.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_dsort_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "BKE_dsort.h"
#include "BKE_modifier.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"

static void rna_DSort_vgroup_set(PointerRNA *ptr, const char *value)
{
	DSortSettings *dss = (DSortSettings *)ptr->data;

	Object *ob = dss->object;
	if (ob) {
		bDeformGroup *dg = defgroup_find_name(ob, value);
		if (dg) {
			BLI_strncpy(dss->vgroup, value, sizeof(dss->vgroup)); /* no need for BLI_strncpy_utf8, since this matches an existing group */
			return;
		}
	}

	dss->vgroup[0] = '\0';
}

static void rna_DSort_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Object *ob = (Object *)ptr->id.data;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

#else

static void rna_def_dsort_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_sort_type_items[] = {
		{DSORT_TYPE_AXIS, "AXIS", 0, "Axis", "Sort on axis"},
		{DSORT_TYPE_SELECTED, "SELECTED", 0, "Selected", "Sort from selected"},
		{DSORT_TYPE_CURSOR, "CURSOR", 0, "Cursor", "Sort from cursor"},
		{DSORT_TYPE_WEIGHTS, "WEIGHTS", 0, "Weights", "Sort from weights"},
		{DSORT_TYPE_RANDOM, "RANDOMIZE", 0, "Randomize", "Randomize order"},
		{DSORT_TYPE_NONE, "NONE", 0, "None", "No sorting"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_axis_items[] = {
		{DSORT_AXIS_X, "X", 0, "X", "Sort on objects X axis"},
		{DSORT_AXIS_Y, "Y", 0, "Y", "Sort on objects Y axis"},
		{DSORT_AXIS_Z, "Z", 0, "Z", "Sort on objects Z axis"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "DSortSettings", NULL);
	RNA_def_struct_ui_text(srna, "Dynamic Sort Settings", "Dynamic sort settings.");
	RNA_def_struct_sdna(srna, "DSortSettings");
	RNA_def_struct_path_func(srna, "rna_DSortSettings_path");

	prop = RNA_def_property(srna, "vgroup", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Name of Vertex Group to sort from");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DSort_vgroup_set");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "sort_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_sort_type_items);
	RNA_def_property_ui_text(prop, "Sort Type", "Sort Type");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "reverse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "reverse", 1);
	RNA_def_property_ui_text(prop, "Reverse", "Reverse sort order.");
	RNA_def_property_update(prop, 0, "rna_SortModifier_free_update");

	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Sort on axis");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "use_original_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_original_mesh", 1);
	RNA_def_property_ui_text(prop, "Use Original Mesh", "Use selected elements from original mesh.");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "connected_first", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "connected_first", 1);
	RNA_def_property_ui_text(prop, "Connected First", "Connected First");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "random_seed", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "random_seed");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Random Seed", "Seed of the random generator");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "sort_verts", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sort_elems", DSORT_ELEMS_VERTS);
	RNA_def_property_ui_text(prop, "Sort Verts", "Sort Verts");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "sort_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sort_elems", DSORT_ELEMS_EDGES);
	RNA_def_property_ui_text(prop, "Sort Edges", "Sort Edges");
	RNA_def_property_update(prop, 0, "rna_DSort_update");

	prop = RNA_def_property(srna, "sort_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sort_elems", DSORT_ELEMS_FACES);
	RNA_def_property_ui_text(prop, "Sort Faces", "Sort Faces");
	RNA_def_property_update(prop, 0, "rna_DSort_update");
}

#endif
