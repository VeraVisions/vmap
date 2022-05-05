/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "select.h"

#include <gtk/gtk.h>

#include "debugging/debugging.h"

#include "ientity.h"
#include "eclasslib.h"
#include "iselection.h"
#include "iundo.h"

#include <vector>

#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "shaderlib.h"
#include "scenelib.h"

#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include "brushmanip.h"
#include "brush.h"
#include "patchmanip.h"
#include "patchdialog.h"
#include "selection.h"
#include "texwindow.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "grid.h"
#include "map.h"
#include "entityinspector.h"


select_workzone_t g_select_workzone;


/**
   Loops over all selected brushes and stores their
   world AABBs in the specified array.
 */
class CollectSelectedBrushesBounds : public SelectionSystem::Visitor {
AABB *m_bounds;         // array of AABBs
Unsigned m_max;         // max AABB-elements in array
Unsigned &m_count;      // count of valid AABBs stored in array

public:
CollectSelectedBrushesBounds(AABB *bounds, Unsigned max, Unsigned &count)
	: m_bounds(bounds),
	m_max(max),
	m_count(count)
{
	m_count = 0;
}

void visit(scene::Instance &instance) const
{
	ASSERT_MESSAGE(m_count <= m_max, "Invalid m_count in CollectSelectedBrushesBounds");

	// stop if the array is already full
	if (m_count == m_max) {
		return;
	}

	Selectable *selectable = Instance_getSelectable(instance);
	if ((selectable != 0)
	    && instance.isSelected()) {
		// brushes only
		if (Instance_getBrush(instance) != 0) {
			m_bounds[m_count] = instance.worldAABB();
			++m_count;
		}
	}
}
};

/**
   Selects all objects that intersect one of the bounding AABBs.
   The exact intersection-method is specified through TSelectionPolicy
 */
template<class TSelectionPolicy>
class SelectByBounds : public scene::Graph::Walker {
AABB *m_aabbs;                 // selection aabbs
Unsigned m_count;              // number of aabbs in m_aabbs
TSelectionPolicy policy;       // type that contains a custom intersection method aabb<->aabb

public:
SelectByBounds(AABB *aabbs, Unsigned count)
	: m_aabbs(aabbs),
	m_count(count)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	Selectable *selectable = Instance_getSelectable(instance);

	// ignore worldspawn
	Entity *entity = Node_getEntity(path.top());
	if (entity) {
		if (string_equal(entity->getKeyValue("classname"), "worldspawn")) {
			return true;
		}
	}

	if ((path.size() > 1) &&
	    (!path.top().get().isRoot()) &&
	    (selectable != 0)
	    ) {
		for (Unsigned i = 0; i < m_count; ++i) {
			if (policy.Evaluate(m_aabbs[i], instance)) {
				selectable->setSelected(true);
			}
		}
	}

	return true;
}

/**
   Performs selection operation on the global scenegraph.
   If delete_bounds_src is true, then the objects which were
   used as source for the selection aabbs will be deleted.
 */
static void DoSelection(bool delete_bounds_src = true)
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::ePrimitive) {
		// we may not need all AABBs since not all selected objects have to be brushes
		const Unsigned max = (Unsigned) GlobalSelectionSystem().countSelected();
		AABB *aabbs = new AABB[max];

		Unsigned count;
		CollectSelectedBrushesBounds collector(aabbs, max, count);
		GlobalSelectionSystem().foreachSelected(collector);

		// nothing usable in selection
		if (!count) {
			delete[] aabbs;
			return;
		}

		// delete selected objects
		if (delete_bounds_src) { // see deleteSelection
			UndoableCommand undo("deleteSelected");
			Select_Delete();
		}

		// select objects with bounds
		GlobalSceneGraph().traverse(SelectByBounds<TSelectionPolicy>(aabbs, count));

		SceneChangeNotify();
		delete[] aabbs;
	}
}
};

/**
   SelectionPolicy for SelectByBounds
   Returns true if box and the AABB of instance intersect
 */
class SelectionPolicy_Touching {
public:
bool Evaluate(const AABB &box, scene::Instance &instance) const
{
	const AABB &other(instance.worldAABB());
	for (Unsigned i = 0; i < 3; ++i) {
		if (fabsf(box.origin[i] - other.origin[i]) > (box.extents[i] + other.extents[i])) {
			return false;
		}
	}
	return true;
}
};

/**
   SelectionPolicy for SelectByBounds
   Returns true if the AABB of instance is inside box
 */
class SelectionPolicy_Inside {
public:
bool Evaluate(const AABB &box, scene::Instance &instance) const
{
	const AABB &other(instance.worldAABB());
	for (Unsigned i = 0; i < 3; ++i) {
		if (fabsf(box.origin[i] - other.origin[i]) > (box.extents[i] - other.extents[i])) {
			return false;
		}
	}
	return true;
}
};

class DeleteSelected : public scene::Graph::Walker {
mutable bool m_remove;
mutable bool m_removedChild;
public:
DeleteSelected()
	: m_remove(false), m_removedChild(false)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	m_removedChild = false;

	Selectable *selectable = Instance_getSelectable(instance);
	if (selectable != 0
	    && selectable->isSelected()
	    && path.size() > 1
	    && !path.top().get().isRoot()) {
		m_remove = true;

		return false; // dont traverse into child elements
	}
	return true;
}

void post(const scene::Path &path, scene::Instance &instance) const
{

	if (m_removedChild) {
		m_removedChild = false;

		// delete empty entities
		Entity *entity = Node_getEntity(path.top());
		if (entity != 0
		    && path.top().get_pointer() != Map_FindWorldspawn(g_map)
		    && Node_getTraversable(path.top())->empty()) {
			Path_deleteTop(path);
		}
	}

	// node should be removed
	if (m_remove) {
		if (Node_isEntity(path.parent()) != 0) {
			m_removedChild = true;
		}

		m_remove = false;
		Path_deleteTop(path);
	}
}
};



class DeleteEmpty : public scene::Graph::Walker {
mutable bool m_remove;
public:
DeleteEmpty()
	: m_remove(false)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{

	return true;
}

void post(const scene::Path &path, scene::Instance &instance) const
{
	/* skip point ents, crash otherwise */
	Entity *entity = Node_getEntity(path.top());
	if (entity != 0 && entity->getEntityClass().fixedsize) {
		return;
	}

	// delete empty entities
	if (entity != 0
	    && path.top().get_pointer() != Map_FindWorldspawn(g_map)
	    && Node_getTraversable(path.top())->empty())
	{
		Path_deleteTop(path);
	}
	/*}*/

	/*// node should be removed
	   if (m_remove) {
	        if (Node_isEntity(path.parent()) != 0) {
	                m_removedChild = true;
	        }

	        m_remove = false;
	        Path_deleteTop(path);
	   }*/
}
};


void Scene_DeleteEmpty()
{
	GlobalSceneGraph().traverse(DeleteEmpty());
	SceneChangeNotify();
}

void Scene_DeleteSelected(scene::Graph &graph)
{
	graph.traverse(DeleteSelected());
	SceneChangeNotify();
}

void Select_Delete(void)
{
	Scene_DeleteSelected(GlobalSceneGraph());
}

class InvertSelectionWalker : public scene::Graph::Walker {
SelectionSystem::EMode m_mode;
mutable Selectable *m_selectable;
public:
InvertSelectionWalker(SelectionSystem::EMode mode)
	: m_mode(mode), m_selectable(0)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	Selectable *selectable = Instance_getSelectable(instance);
	if (selectable) {
		switch (m_mode) {
		case SelectionSystem::eEntity:
			if (Node_isEntity(path.top()) != 0) {
				m_selectable = path.top().get().visible() ? selectable : 0;
			}
			break;
		case SelectionSystem::ePrimitive:
			m_selectable = path.top().get().visible() ? selectable : 0;
			break;
		case SelectionSystem::eComponent:
			break;
		}
	}
	return true;
}

void post(const scene::Path &path, scene::Instance &instance) const
{
	if (m_selectable != 0) {
		m_selectable->setSelected(!m_selectable->isSelected());
		m_selectable = 0;
	}
}
};

void Scene_Invert_Selection(scene::Graph &graph)
{
	graph.traverse(InvertSelectionWalker(GlobalSelectionSystem().Mode()));
}

void Select_Invert()
{
	Scene_Invert_Selection(GlobalSceneGraph());
}

class ExpandSelectionToEntitiesWalker : public scene::Graph::Walker {
mutable std::size_t m_depth;
NodeSmartReference worldspawn;
public:
ExpandSelectionToEntitiesWalker() : m_depth(0), worldspawn(Map_FindOrInsertWorldspawn(g_map))
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	++m_depth;

	// ignore worldspawn
	NodeSmartReference me(path.top().get());
	if (me == worldspawn) {
		return false;
	}

	if (m_depth == 2) { // entity depth
		// traverse and select children if any one is selected
		if (instance.childSelected()) {
			Instance_setSelected(instance, true);
		}
		return Node_getEntity(path.top())->isContainer() && instance.isSelected();
	} else if (m_depth == 3) { // primitive depth
		Instance_setSelected(instance, true);
		return false;
	}
	return true;
}

void post(const scene::Path &path, scene::Instance &instance) const
{
	--m_depth;
}
};

void Scene_ExpandSelectionToEntities()
{
	GlobalSceneGraph().traverse(ExpandSelectionToEntitiesWalker());
}


namespace {
void Selection_UpdateWorkzone()
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		Select_GetBounds(g_select_workzone.d_work_min, g_select_workzone.d_work_max);
	}
}

typedef FreeCaller<void (), Selection_UpdateWorkzone> SelectionUpdateWorkzoneCaller;

IdleDraw g_idleWorkzone = IdleDraw(SelectionUpdateWorkzoneCaller());
}

const select_workzone_t &Select_getWorkZone()
{
	g_idleWorkzone.flush();
	return g_select_workzone;
}

void UpdateWorkzone_ForSelection()
{
	g_idleWorkzone.queueDraw();
}

// update the workzone to the current selection
void UpdateWorkzone_ForSelectionChanged(const Selectable &selectable)
{
	if (selectable.isSelected()) {
		UpdateWorkzone_ForSelection();
	}
}

void Select_SetShader(const char *shader)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetShader_Selected(GlobalSceneGraph(), shader);
		Scene_PatchSetShader_Selected(GlobalSceneGraph(), shader);
	}
	Scene_BrushSetShader_Component_Selected(GlobalSceneGraph(), shader);
}

void Select_SetTexdef(const TextureProjection &projection, bool ignorebasis)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetTexdef_Selected(GlobalSceneGraph(), projection, ignorebasis);
	}
	Scene_BrushSetTexdef_Component_Selected(GlobalSceneGraph(), projection, ignorebasis);
}

void Select_SetFlags(const ContentsFlagsValue &flags)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushSetFlags_Selected(GlobalSceneGraph(), flags);
	}
	Scene_BrushSetFlags_Component_Selected(GlobalSceneGraph(), flags);
}

void Select_GetBounds(Vector3 &mins, Vector3 &maxs)
{
	AABB bounds;
	Scene_BoundsSelected(GlobalSceneGraph(), bounds);
	maxs = vector3_added(bounds.origin, bounds.extents);
	mins = vector3_subtracted(bounds.origin, bounds.extents);
}

void Select_GetMid(Vector3 &mid)
{
	AABB bounds;
	Scene_BoundsSelected(GlobalSceneGraph(), bounds);
	mid = vector3_snapped(bounds.origin);
}


void Select_FlipAxis(int axis)
{
	Vector3 flip(1, 1, 1);
	flip[axis] = -1;
	GlobalSelectionSystem().scaleSelected(flip);
}


void Select_Scale(float x, float y, float z)
{
	GlobalSelectionSystem().scaleSelected(Vector3(x, y, z));
}

enum axis_t {
	eAxisX = 0,
	eAxisY = 1,
	eAxisZ = 2,
};

enum sign_t {
	eSignPositive = 1,
	eSignNegative = -1,
};

inline Matrix4 matrix4_rotation_for_axis90(axis_t axis, sign_t sign)
{
	switch (axis) {
	case eAxisX:
		if (sign == eSignPositive) {
			return matrix4_rotation_for_sincos_x(1, 0);
		} else {
			return matrix4_rotation_for_sincos_x(-1, 0);
		}
	case eAxisY:
		if (sign == eSignPositive) {
			return matrix4_rotation_for_sincos_y(1, 0);
		} else {
			return matrix4_rotation_for_sincos_y(-1, 0);
		}
	default: //case eAxisZ:
		if (sign == eSignPositive) {
			return matrix4_rotation_for_sincos_z(1, 0);
		} else {
			return matrix4_rotation_for_sincos_z(-1, 0);
		}
	}
}

inline void matrix4_rotate_by_axis90(Matrix4 &matrix, axis_t axis, sign_t sign)
{
	matrix4_multiply_by_matrix4(matrix, matrix4_rotation_for_axis90(axis, sign));
}

inline void matrix4_pivoted_rotate_by_axis90(Matrix4 &matrix, axis_t axis, sign_t sign, const Vector3 &pivotpoint)
{
	matrix4_translate_by_vec3(matrix, pivotpoint);
	matrix4_rotate_by_axis90(matrix, axis, sign);
	matrix4_translate_by_vec3(matrix, vector3_negated(pivotpoint));
}

inline Quaternion quaternion_for_axis90(axis_t axis, sign_t sign)
{
#if 1
	switch (axis) {
	case eAxisX:
		if (sign == eSignPositive) {
			return Quaternion(c_half_sqrt2f, 0, 0, c_half_sqrt2f);
		} else {
			return Quaternion(-c_half_sqrt2f, 0, 0, -c_half_sqrt2f);
		}
	case eAxisY:
		if (sign == eSignPositive) {
			return Quaternion(0, c_half_sqrt2f, 0, c_half_sqrt2f);
		} else {
			return Quaternion(0, -c_half_sqrt2f, 0, -c_half_sqrt2f);
		}
	default: //case eAxisZ:
		if (sign == eSignPositive) {
			return Quaternion(0, 0, c_half_sqrt2f, c_half_sqrt2f);
		} else {
			return Quaternion(0, 0, -c_half_sqrt2f, -c_half_sqrt2f);
		}
	}
#else
	quaternion_for_matrix4_rotation( matrix4_rotation_for_axis90( (axis_t)axis, ( deg > 0 ) ? eSignPositive : eSignNegative ) );
#endif
}

void Select_RotateAxis(int axis, float deg)
{
	if (fabs(deg) == 90.f) {
		GlobalSelectionSystem().rotateSelected(
			quaternion_for_axis90((axis_t) axis, (deg > 0) ? eSignPositive : eSignNegative));
	} else {
		switch (axis) {
		case 0:
			GlobalSelectionSystem().rotateSelected(
				quaternion_for_matrix4_rotation(matrix4_rotation_for_x_degrees(deg)));
			break;
		case 1:
			GlobalSelectionSystem().rotateSelected(
				quaternion_for_matrix4_rotation(matrix4_rotation_for_y_degrees(deg)));
			break;
		case 2:
			GlobalSelectionSystem().rotateSelected(
				quaternion_for_matrix4_rotation(matrix4_rotation_for_z_degrees(deg)));
			break;
		}
	}
}


void Select_ShiftTexture(float x, float y)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushShiftTexdef_Selected(GlobalSceneGraph(), x, y);
		Scene_PatchTranslateTexture_Selected(GlobalSceneGraph(), x, y);
	}
	//globalOutputStream() << "shift selected face textures: s=" << x << " t=" << y << '\n';
	Scene_BrushShiftTexdef_Component_Selected(GlobalSceneGraph(), x, y);
}

void Select_ScaleTexture(float x, float y)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushScaleTexdef_Selected(GlobalSceneGraph(), x, y);
		Scene_PatchScaleTexture_Selected(GlobalSceneGraph(), x, y);
	}
	Scene_BrushScaleTexdef_Component_Selected(GlobalSceneGraph(), x, y);
}

void Select_RotateTexture(float amt)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushRotateTexdef_Selected(GlobalSceneGraph(), amt);
		Scene_PatchRotateTexture_Selected(GlobalSceneGraph(), amt);
	}
	Scene_BrushRotateTexdef_Component_Selected(GlobalSceneGraph(), amt);
}

// TTimo modified to handle shader architecture:
// expects shader names at input, comparison relies on shader names .. texture names no longer relevant
void FindReplaceTextures(const char *pFind, const char *pReplace, bool bSelected)
{
	if (!texdef_name_valid(pFind)) {
		globalErrorStream() << "FindReplaceTextures: invalid texture name: '" << pFind << "', aborted\n";
		return;
	}
	if (!texdef_name_valid(pReplace)) {
		globalErrorStream() << "FindReplaceTextures: invalid texture name: '" << pReplace << "', aborted\n";
		return;
	}

	StringOutputStream command;
	command << "textureFindReplace -find " << pFind << " -replace " << pReplace;
	UndoableCommand undo(command.c_str());

	if (bSelected) {
		if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
			Scene_BrushFindReplaceShader_Selected(GlobalSceneGraph(), pFind, pReplace);
			Scene_PatchFindReplaceShader_Selected(GlobalSceneGraph(), pFind, pReplace);
		}
		Scene_BrushFindReplaceShader_Component_Selected(GlobalSceneGraph(), pFind, pReplace);
	} else {
		Scene_BrushFindReplaceShader(GlobalSceneGraph(), pFind, pReplace);
		Scene_PatchFindReplaceShader(GlobalSceneGraph(), pFind, pReplace);
	}
}

typedef std::vector<const char *> PropertyValues;

bool propertyvalues_contain(const PropertyValues &propertyvalues, const char *str)
{
	for (PropertyValues::const_iterator i = propertyvalues.begin(); i != propertyvalues.end(); ++i) {
		if (string_equal(str, *i)) {
			return true;
		}
	}
	return false;
}

class EntityFindByPropertyValueWalker : public scene::Graph::Walker {
const PropertyValues &m_propertyvalues;
const char *m_prop;
public:
EntityFindByPropertyValueWalker(const char *prop, const PropertyValues &propertyvalues)
	: m_propertyvalues(propertyvalues), m_prop(prop)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	Entity *entity = Node_getEntity(path.top());
	if (entity != 0
	    && propertyvalues_contain(m_propertyvalues, entity->getKeyValue(m_prop))) {
		Instance_getSelectable(instance)->setSelected(true);
	}
	return true;
}
};

void Scene_EntitySelectByPropertyValues(scene::Graph &graph, const char *prop, const PropertyValues &propertyvalues)
{
	graph.traverse(EntityFindByPropertyValueWalker(prop, propertyvalues));
}

class EntityGetSelectedPropertyValuesWalker : public scene::Graph::Walker {
PropertyValues &m_propertyvalues;
const char *m_prop;
public:
EntityGetSelectedPropertyValuesWalker(const char *prop, PropertyValues &propertyvalues)
	: m_propertyvalues(propertyvalues), m_prop(prop)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	Selectable *selectable = Instance_getSelectable(instance);
	if (selectable != 0
	    && selectable->isSelected()) {
		Entity *entity = Node_getEntity(path.top());
		if (entity != 0) {
			if (!propertyvalues_contain(m_propertyvalues, entity->getKeyValue(m_prop))) {
				m_propertyvalues.push_back(entity->getKeyValue(m_prop));
			}
		}
	}
	return true;
}
};

void Scene_EntityGetPropertyValues(scene::Graph &graph, const char *prop, PropertyValues &propertyvalues)
{
	graph.traverse(EntityGetSelectedPropertyValuesWalker(prop, propertyvalues));
}

void Select_AllOfType()
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		if (GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace) {
			GlobalSelectionSystem().setSelectedAllComponents(false);
			Scene_BrushSelectByShader_Component(GlobalSceneGraph(),
			                                    TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
		}
	} else {
		PropertyValues propertyvalues;
		const char *prop = EntityInspector_getCurrentKey();
		if (!prop || !*prop) {
			prop = "classname";
		}
		Scene_EntityGetPropertyValues(GlobalSceneGraph(), prop, propertyvalues);
		GlobalSelectionSystem().setSelectedAll(false);
		if (!propertyvalues.empty()) {
			Scene_EntitySelectByPropertyValues(GlobalSceneGraph(), prop, propertyvalues);
		} else {
			Scene_BrushSelectByShader(GlobalSceneGraph(), TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
			Scene_PatchSelectByShader(GlobalSceneGraph(), TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
		}
	}
}

void Select_AllOfModel()
{
	if (GlobalSelectionSystem().Mode() == SelectionSystem::eComponent) {
		if (GlobalSelectionSystem().ComponentMode() == SelectionSystem::eFace) {
			GlobalSelectionSystem().setSelectedAllComponents(false);
			Scene_BrushSelectByShader_Component(GlobalSceneGraph(),
			                                    TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
		}
	} else {
		PropertyValues propertyvalues;
		const char *prop = EntityInspector_getCurrentKey();
		if (!prop || !*prop) {
			prop = "model";
		}
		Scene_EntityGetPropertyValues(GlobalSceneGraph(), prop, propertyvalues);
		GlobalSelectionSystem().setSelectedAll(false);
		if (!propertyvalues.empty()) {
			Scene_EntitySelectByPropertyValues(GlobalSceneGraph(), prop, propertyvalues);
		}
	}
}

void Select_Inside(void)
{
	SelectByBounds<SelectionPolicy_Inside>::DoSelection();
}

void Select_Touching(void)
{
	SelectByBounds<SelectionPolicy_Touching>::DoSelection(false);
}

void Select_FitTexture(float horizontal, float vertical)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushFitTexture_Selected(GlobalSceneGraph(), horizontal, vertical);
	}
	Scene_BrushFitTexture_Component_Selected(GlobalSceneGraph(), horizontal, vertical);

	SceneChangeNotify();
}


void Select_AlignTexture(int alignment)
{
	if (GlobalSelectionSystem().Mode() != SelectionSystem::eComponent) {
		Scene_BrushAlignTexture_Selected(GlobalSceneGraph(), alignment);
	}
	Scene_BrushAlignTexture_Component_Selected(GlobalSceneGraph(), alignment);

	SceneChangeNotify();
}

inline void hide_node(scene::Node &node, bool hide)
{
	hide
    ? node.enable(scene::Node::eHidden)
    : node.disable(scene::Node::eHidden);
}

class HideSelectedWalker : public scene::Graph::Walker {
bool m_hide;
public:
HideSelectedWalker(bool hide)
	: m_hide(hide)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	Selectable *selectable = Instance_getSelectable(instance);
	if (selectable != 0
	    && selectable->isSelected()) {
		hide_node(path.top(), m_hide);
	}
	return true;
}
};

void Scene_Hide_Selected(bool hide)
{
	GlobalSceneGraph().traverse(HideSelectedWalker(hide));
}

void Select_Hide()
{
	Scene_Hide_Selected(true);
	SceneChangeNotify();
}

void QE_hiddenCountChanged();
void HideSelected()
{
	Select_Hide();
	GlobalSelectionSystem().setSelectedAll(false);
	QE_hiddenCountChanged();
}

void HideUnselected()
{
	Select_Invert();
	Select_Hide();
	GlobalSelectionSystem().setSelectedAll(false);
	QE_hiddenCountChanged();
}


class HideAllWalker : public scene::Graph::Walker {
bool m_hide;
public:
HideAllWalker(bool hide)
	: m_hide(hide)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	hide_node(path.top(), m_hide);
	return true;
}
};

void Scene_Hide_All(bool hide)
{
	GlobalSceneGraph().traverse(HideAllWalker(hide));
}

void Select_ShowAllHidden()
{
	Scene_Hide_All(false);
	SceneChangeNotify();
	QE_hiddenCountChanged();
}


void Selection_Flipx()
{
	UndoableCommand undo("mirrorSelected -axis x");
	Select_FlipAxis(0);
}

void Selection_Flipy()
{
	UndoableCommand undo("mirrorSelected -axis y");
	Select_FlipAxis(1);
}

void Selection_Flipz()
{
	UndoableCommand undo("mirrorSelected -axis z");
	Select_FlipAxis(2);
}

void Selection_Rotatex()
{
	UndoableCommand undo("rotateSelected -axis x -angle -90");
	Select_RotateAxis(0, -90);
}

void Selection_Rotatey()
{
	UndoableCommand undo("rotateSelected -axis y -angle 90");
	Select_RotateAxis(1, 90);
}

void Selection_Rotatez()
{
	UndoableCommand undo("rotateSelected -axis z -angle -90");
	Select_RotateAxis(2, -90);
}


void Nudge(int nDim, float fNudge)
{
	Vector3 translate(0, 0, 0);
	translate[nDim] = fNudge;

	GlobalSelectionSystem().translateSelected(translate);
}

void Selection_NudgeZ(float amount)
{
	StringOutputStream command;
	command << "nudgeSelected -axis z -amount " << amount;
	UndoableCommand undo(command.c_str());

	Nudge(2, amount);
}

void Selection_MoveDown()
{
	Selection_NudgeZ(-GetGridSize());
}

void Selection_MoveUp()
{
	Selection_NudgeZ(GetGridSize());
}

void SceneSelectionChange(const Selectable &selectable)
{
	SceneChangeNotify();
}

SignalHandlerId Selection_boundsChanged;

void Selection_construct()
{
	typedef FreeCaller<void (const Selectable &), SceneSelectionChange> SceneSelectionChangeCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(SceneSelectionChangeCaller());
	typedef FreeCaller<void (
				   const Selectable &), UpdateWorkzone_ForSelectionChanged> UpdateWorkzoneForSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(UpdateWorkzoneForSelectionChangedCaller());
	typedef FreeCaller<void (), UpdateWorkzone_ForSelection> UpdateWorkzoneForSelectionCaller;
	Selection_boundsChanged = GlobalSceneGraph().addBoundsChangedCallback(UpdateWorkzoneForSelectionCaller());
}

void Selection_destroy()
{
	GlobalSceneGraph().removeBoundsChangedCallback(Selection_boundsChanged);
}


#include "gtkdlgs.h"
#include <gdk/gdkkeysyms.h>


inline Quaternion quaternion_for_euler_xyz_degrees(const Vector3 &eulerXYZ)
{
#if 0
	return quaternion_for_matrix4_rotation( matrix4_rotation_for_euler_xyz_degrees( eulerXYZ ) );
#elif 0
	return quaternion_multiplied_by_quaternion(
		quaternion_multiplied_by_quaternion(
			quaternion_for_z( degrees_to_radians( eulerXYZ[2] ) ),
			quaternion_for_y( degrees_to_radians( eulerXYZ[1] ) )
			),
		quaternion_for_x( degrees_to_radians( eulerXYZ[0] ) )
		);
#elif 1
	double cx = cos(degrees_to_radians(eulerXYZ[0] * 0.5));
	double sx = sin(degrees_to_radians(eulerXYZ[0] * 0.5));
	double cy = cos(degrees_to_radians(eulerXYZ[1] * 0.5));
	double sy = sin(degrees_to_radians(eulerXYZ[1] * 0.5));
	double cz = cos(degrees_to_radians(eulerXYZ[2] * 0.5));
	double sz = sin(degrees_to_radians(eulerXYZ[2] * 0.5));

	return Quaternion(
		cz * cy * sx - sz * sy * cx,
		cz * sy * cx + sz * cy * sx,
		sz * cy * cx - cz * sy * sx,
		cz * cy * cx + sz * sy * sx
		);
#endif
}

struct RotateDialog {
	ui::SpinButton x{ui::null};
	ui::SpinButton y{ui::null};
	ui::SpinButton z{ui::null};
	ui::Window window{ui::null};
};

static gboolean rotatedlg_apply(ui::Widget widget, RotateDialog *rotateDialog)
{
	Vector3 eulerXYZ;

	eulerXYZ[0] = static_cast<float>( gtk_spin_button_get_value(rotateDialog->x));
	eulerXYZ[1] = static_cast<float>( gtk_spin_button_get_value(rotateDialog->y));
	eulerXYZ[2] = static_cast<float>( gtk_spin_button_get_value(rotateDialog->z));

	StringOutputStream command;
	command << "rotateSelectedEulerXYZ -x " << eulerXYZ[0] << " -y " << eulerXYZ[1] << " -z " << eulerXYZ[2];
	UndoableCommand undo(command.c_str());

	GlobalSelectionSystem().rotateSelected(quaternion_for_euler_xyz_degrees(eulerXYZ));
	return TRUE;
}

static gboolean rotatedlg_cancel(ui::Widget widget, RotateDialog *rotateDialog)
{
	rotateDialog->window.hide();

	gtk_spin_button_set_value(rotateDialog->x, 0.0f); // reset to 0 on close
	gtk_spin_button_set_value(rotateDialog->y, 0.0f);
	gtk_spin_button_set_value(rotateDialog->z, 0.0f);

	return TRUE;
}

static gboolean rotatedlg_ok(ui::Widget widget, RotateDialog *rotateDialog)
{
	rotatedlg_apply(widget, rotateDialog);
	rotateDialog->window.hide();
	return TRUE;
}

static gboolean rotatedlg_delete(ui::Widget widget, GdkEventAny *event, RotateDialog *rotateDialog)
{
	rotatedlg_cancel(widget, rotateDialog);
	return TRUE;
}

RotateDialog g_rotate_dialog;

void DoRotateDlg()
{
	if (!g_rotate_dialog.window) {
		g_rotate_dialog.window = MainFrame_getWindow().create_dialog_window("Arbitrary rotation",
		                                                                    G_CALLBACK(rotatedlg_delete),
		                                                                    &g_rotate_dialog);

		auto accel = ui::AccelGroup(ui::New);
		g_rotate_dialog.window.add_accel_group(accel);

		{
			auto hbox = create_dialog_hbox(4, 4);
			g_rotate_dialog.window.add(hbox);
			{
				auto table = create_dialog_table(3, 2, 4, 4);
				hbox.pack_start(table, TRUE, TRUE, 0);
				{
					ui::Widget label = ui::Label("  X  ");
					label.show();
					table.attach(label, {0, 1, 0, 1}, {0, 0});
				}
				{
					ui::Widget label = ui::Label("  Y  ");
					label.show();
					table.attach(label, {0, 1, 1, 2}, {0, 0});
				}
				{
					ui::Widget label = ui::Label("  Z  ");
					label.show();
					table.attach(label, {0, 1, 2, 3}, {0, 0});
				}
				{
					auto adj = ui::Adjustment(0, -359, 359, 1, 10, 0);
					auto spin = ui::SpinButton(adj, 1, 0);
					spin.show();
					table.attach(spin, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});
					spin.dimensions(64, -1);
					gtk_spin_button_set_wrap(spin, TRUE);

					gtk_widget_grab_focus(spin);

					g_rotate_dialog.x = spin;
				}
				{
					auto adj = ui::Adjustment(0, -359, 359, 1, 10, 0);
					auto spin = ui::SpinButton(adj, 1, 0);
					spin.show();
					table.attach(spin, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
					spin.dimensions(64, -1);
					gtk_spin_button_set_wrap(spin, TRUE);

					g_rotate_dialog.y = spin;
				}
				{
					auto adj = ui::Adjustment(0, -359, 359, 1, 10, 0);
					auto spin = ui::SpinButton(adj, 1, 0);
					spin.show();
					table.attach(spin, {1, 2, 2, 3}, {GTK_EXPAND | GTK_FILL, 0});
					spin.dimensions(64, -1);
					gtk_spin_button_set_wrap(spin, TRUE);

					g_rotate_dialog.z = spin;
				}
			}
			{
				auto vbox = create_dialog_vbox(4);
				hbox.pack_start(vbox, TRUE, TRUE, 0);
				{
					auto button = create_dialog_button("OK", G_CALLBACK(rotatedlg_ok), &g_rotate_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
					widget_make_default(button);
					gtk_widget_add_accelerator(button, "clicked", accel, GDK_KEY_Return, (GdkModifierType) 0,
					                           (GtkAccelFlags) 0);
				}
				{
					auto button = create_dialog_button("Cancel", G_CALLBACK(rotatedlg_cancel), &g_rotate_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
					gtk_widget_add_accelerator(button, "clicked", accel, GDK_KEY_Escape, (GdkModifierType) 0,
					                           (GtkAccelFlags) 0);
				}
				{
					auto button = create_dialog_button("Apply", G_CALLBACK(rotatedlg_apply), &g_rotate_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
				}
			}
		}
	}

	g_rotate_dialog.window.show();
}


struct ScaleDialog {
	ui::Entry x{ui::null};
	ui::Entry y{ui::null};
	ui::Entry z{ui::null};
	ui::Window window{ui::null};
};

static gboolean scaledlg_apply(ui::Widget widget, ScaleDialog *scaleDialog)
{
	float sx, sy, sz;

	sx = static_cast<float>( atof(gtk_entry_get_text(GTK_ENTRY(scaleDialog->x))));
	sy = static_cast<float>( atof(gtk_entry_get_text(GTK_ENTRY(scaleDialog->y))));
	sz = static_cast<float>( atof(gtk_entry_get_text(GTK_ENTRY(scaleDialog->z))));

	StringOutputStream command;
	command << "scaleSelected -x " << sx << " -y " << sy << " -z " << sz;
	UndoableCommand undo(command.c_str());

	Select_Scale(sx, sy, sz);

	return TRUE;
}

static gboolean scaledlg_cancel(ui::Widget widget, ScaleDialog *scaleDialog)
{
	scaleDialog->window.hide();

	scaleDialog->x.text("1.0");
	scaleDialog->y.text("1.0");
	scaleDialog->z.text("1.0");

	return TRUE;
}

static gboolean scaledlg_ok(ui::Widget widget, ScaleDialog *scaleDialog)
{
	scaledlg_apply(widget, scaleDialog);
	scaleDialog->window.hide();
	return TRUE;
}

static gboolean scaledlg_delete(ui::Widget widget, GdkEventAny *event, ScaleDialog *scaleDialog)
{
	scaledlg_cancel(widget, scaleDialog);
	return TRUE;
}

ScaleDialog g_scale_dialog;

void DoScaleDlg()
{
	if (!g_scale_dialog.window) {
		g_scale_dialog.window = MainFrame_getWindow().create_dialog_window("Arbitrary scale",
		                                                                   G_CALLBACK(scaledlg_delete),
		                                                                   &g_scale_dialog);

		auto accel = ui::AccelGroup(ui::New);
		g_scale_dialog.window.add_accel_group(accel);

		{
			auto hbox = create_dialog_hbox(4, 4);
			g_scale_dialog.window.add(hbox);
			{
				auto table = create_dialog_table(3, 2, 4, 4);
				hbox.pack_start(table, TRUE, TRUE, 0);
				{
					ui::Widget label = ui::Label("  X  ");
					label.show();
					table.attach(label, {0, 1, 0, 1}, {0, 0});
				}
				{
					ui::Widget label = ui::Label("  Y  ");
					label.show();
					table.attach(label, {0, 1, 1, 2}, {0, 0});
				}
				{
					ui::Widget label = ui::Label("  Z  ");
					label.show();
					table.attach(label, {0, 1, 2, 3}, {0, 0});
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.text("1.0");
					entry.show();
					table.attach(entry, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});

					g_scale_dialog.x = entry;
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.text("1.0");
					entry.show();
					table.attach(entry, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});

					g_scale_dialog.y = entry;
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.text("1.0");
					entry.show();
					table.attach(entry, {1, 2, 2, 3}, {GTK_EXPAND | GTK_FILL, 0});

					g_scale_dialog.z = entry;
				}
			}
			{
				auto vbox = create_dialog_vbox(4);
				hbox.pack_start(vbox, TRUE, TRUE, 0);
				{
					auto button = create_dialog_button("OK", G_CALLBACK(scaledlg_ok), &g_scale_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
					widget_make_default(button);
					gtk_widget_add_accelerator(button, "clicked", accel, GDK_KEY_Return, (GdkModifierType) 0,
					                           (GtkAccelFlags) 0);
				}
				{
					auto button = create_dialog_button("Cancel", G_CALLBACK(scaledlg_cancel), &g_scale_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
					gtk_widget_add_accelerator(button, "clicked", accel, GDK_KEY_Escape, (GdkModifierType) 0,
					                           (GtkAccelFlags) 0);
				}
				{
					auto button = create_dialog_button("Apply", G_CALLBACK(scaledlg_apply), &g_scale_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
				}
			}
		}
	}

	g_scale_dialog.window.show();
}

/* NEW: Artbirary Move */

struct MoveDialog {
	ui::Entry x{ui::null};
	ui::Entry y{ui::null};
	ui::Entry z{ui::null};
	ui::Window window{ui::null};
};

static gboolean movedlg_apply(ui::Widget widget, MoveDialog *moveDialog)
{
	float sx, sy, sz;

	sx = static_cast<float>( atof(gtk_entry_get_text(GTK_ENTRY(moveDialog->x))));
	sy = static_cast<float>( atof(gtk_entry_get_text(GTK_ENTRY(moveDialog->y))));
	sz = static_cast<float>( atof(gtk_entry_get_text(GTK_ENTRY(moveDialog->z))));

	StringOutputStream command;
	command << "nudgeSelected -axis x " << sx;
	command << "nudgeSelected -axis y " << sy;
	command << "nudgeSelected -axis z " << sz;
	UndoableCommand undo(command.c_str());

	Nudge(0, sx);
	Nudge(1, sy);
	Nudge(2, sz);
	return TRUE;
}

static gboolean movedlg_cancel(ui::Widget widget, MoveDialog *moveDialog)
{
	moveDialog->window.hide();

	moveDialog->x.text("0.0");
	moveDialog->y.text("0.0");
	moveDialog->z.text("0.0");

	return TRUE;
}

static gboolean movedlg_ok(ui::Widget widget, MoveDialog *moveDialog)
{
	movedlg_apply(widget, moveDialog);
	moveDialog->window.hide();
	return TRUE;
}

static gboolean movedlg_delete(ui::Widget widget, GdkEventAny *event, MoveDialog *moveDialog)
{
	movedlg_cancel(widget, moveDialog);
	return TRUE;
}

MoveDialog g_move_dialog;

void DoMoveDlg()
{
	if (!g_move_dialog.window) {
		g_move_dialog.window = MainFrame_getWindow().create_dialog_window("Arbitrary scale",
		                                                                   G_CALLBACK(movedlg_delete),
		                                                                   &g_move_dialog);

		auto accel = ui::AccelGroup(ui::New);
		g_move_dialog.window.add_accel_group(accel);

		{
			auto hbox = create_dialog_hbox(4, 4);
			g_move_dialog.window.add(hbox);
			{
				auto table = create_dialog_table(3, 2, 4, 4);
				hbox.pack_start(table, TRUE, TRUE, 0);
				{
					ui::Widget label = ui::Label("  X  ");
					label.show();
					table.attach(label, {0, 1, 0, 1}, {0, 0});
				}
				{
					ui::Widget label = ui::Label("  Y  ");
					label.show();
					table.attach(label, {0, 1, 1, 2}, {0, 0});
				}
				{
					ui::Widget label = ui::Label("  Z  ");
					label.show();
					table.attach(label, {0, 1, 2, 3}, {0, 0});
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.text("0.0");
					entry.show();
					table.attach(entry, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});

					g_move_dialog.x = entry;
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.text("0.0");
					entry.show();
					table.attach(entry, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});

					g_move_dialog.y = entry;
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.text("0.0");
					entry.show();
					table.attach(entry, {1, 2, 2, 3}, {GTK_EXPAND | GTK_FILL, 0});

					g_move_dialog.z = entry;
				}
			}
			{
				auto vbox = create_dialog_vbox(4);
				hbox.pack_start(vbox, TRUE, TRUE, 0);
				{
					auto button = create_dialog_button("OK", G_CALLBACK(movedlg_ok), &g_move_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
					widget_make_default(button);
					gtk_widget_add_accelerator(button, "clicked", accel, GDK_KEY_Return, (GdkModifierType) 0,
					                           (GtkAccelFlags) 0);
				}
				{
					auto button = create_dialog_button("Cancel", G_CALLBACK(movedlg_cancel), &g_move_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
					gtk_widget_add_accelerator(button, "clicked", accel, GDK_KEY_Escape, (GdkModifierType) 0,
					                           (GtkAccelFlags) 0);
				}
				{
					auto button = create_dialog_button("Apply", G_CALLBACK(movedlg_apply), &g_move_dialog);
					vbox.pack_start(button, FALSE, FALSE, 0);
				}
			}
		}
	}

	g_move_dialog.window.show();
}

