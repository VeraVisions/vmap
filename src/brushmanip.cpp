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

#include "brushmanip.h"


#include "gtkutil/widget.h"
#include "gtkutil/menu.h"
#include "gtkmisc.h"
#include "brushnode.h"
#include "map.h"
#include "texwindow.h"
#include "gtkdlgs.h"
#include "commands.h"
#include "mainframe.h"
#include "dialog.h"
#include "xywindow.h"
#include "preferences.h"

#include <list>
#include <gdk/gdkkeysyms.h>

void Brush_ConstructCuboid(Brush &brush, const AABB &bounds, const char *shader, const TextureProjection &projection)
{
	const unsigned char box[3][2] = {{0, 1},
		{2, 0},
		{1, 2}};
	Vector3 mins(vector3_subtracted(bounds.origin, bounds.extents));
	Vector3 maxs(vector3_added(bounds.origin, bounds.extents));

	brush.clear();
	brush.reserve(6);

	{
		for (int i = 0; i < 3; ++i) {
			Vector3 planepts1(maxs);
			Vector3 planepts2(maxs);
			planepts2[box[i][0]] = mins[box[i][0]];
			planepts1[box[i][1]] = mins[box[i][1]];

			brush.addPlane(maxs, planepts1, planepts2, shader, projection);
		}
	}
	{
		for (int i = 0; i < 3; ++i) {
			Vector3 planepts1(mins);
			Vector3 planepts2(mins);
			planepts1[box[i][0]] = maxs[box[i][0]];
			planepts2[box[i][1]] = maxs[box[i][1]];

			brush.addPlane(mins, planepts1, planepts2, shader, projection);
		}
	}
}

inline float max_extent(const Vector3 &extents)
{
	return std::max(std::max(extents[0], extents[1]), extents[2]);
}

inline float max_extent_2d(const Vector3 &extents, int axis)
{
	switch (axis) {
	case 0:
		return std::max(extents[1], extents[2]);
	case 1:
		return std::max(extents[0], extents[2]);
	default:
		return std::max(extents[0], extents[1]);
	}
}

const std::size_t c_brushPrism_minSides = 3;
const std::size_t c_brushPrism_maxSides = c_brush_maxFaces - 2;
const char *const c_brushPrism_name = "brushPrism";

void Brush_ConstructPrism(Brush &brush, const AABB &bounds, std::size_t sides, int axis, const char *shader,
                          const TextureProjection &projection)
{
	if (sides < c_brushPrism_minSides) {
		globalErrorStream() << c_brushPrism_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is "
		                    << Unsigned(c_brushPrism_minSides) << "\n";
		return;
	}
	if (sides > c_brushPrism_maxSides) {
		globalErrorStream() << c_brushPrism_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is "
		                    << Unsigned(c_brushPrism_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides + 2);

	Vector3 mins(vector3_subtracted(bounds.origin, bounds.extents));
	Vector3 maxs(vector3_added(bounds.origin, bounds.extents));

	float radius = max_extent_2d(bounds.extents, axis);
	const Vector3 &mid = bounds.origin;
	Vector3 planepts[3];

	planepts[2][(axis + 1) % 3] = mins[(axis + 1) % 3];
	planepts[2][(axis + 2) % 3] = mins[(axis + 2) % 3];
	planepts[2][axis] = maxs[axis];
	planepts[1][(axis + 1) % 3] = maxs[(axis + 1) % 3];
	planepts[1][(axis + 2) % 3] = mins[(axis + 2) % 3];
	planepts[1][axis] = maxs[axis];
	planepts[0][(axis + 1) % 3] = maxs[(axis + 1) % 3];
	planepts[0][(axis + 2) % 3] = maxs[(axis + 2) % 3];
	planepts[0][axis] = maxs[axis];

	brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

	planepts[0][(axis + 1) % 3] = mins[(axis + 1) % 3];
	planepts[0][(axis + 2) % 3] = mins[(axis + 2) % 3];
	planepts[0][axis] = mins[axis];
	planepts[1][(axis + 1) % 3] = maxs[(axis + 1) % 3];
	planepts[1][(axis + 2) % 3] = mins[(axis + 2) % 3];
	planepts[1][axis] = mins[axis];
	planepts[2][(axis + 1) % 3] = maxs[(axis + 1) % 3];
	planepts[2][(axis + 2) % 3] = maxs[(axis + 2) % 3];
	planepts[2][axis] = mins[axis];

	brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

	for (std::size_t i = 0; i < sides; ++i) {
		double sv = sin(i * 3.14159265 * 2 / sides);
		double cv = cos(i * 3.14159265 * 2 / sides);

		planepts[0][(axis + 1) % 3] = static_cast<float>( floor(mid[(axis + 1) % 3] + radius * cv + 0.5));
		planepts[0][(axis + 2) % 3] = static_cast<float>( floor(mid[(axis + 2) % 3] + radius * sv + 0.5));
		planepts[0][axis] = mins[axis];

		planepts[1][(axis + 1) % 3] = planepts[0][(axis + 1) % 3];
		planepts[1][(axis + 2) % 3] = planepts[0][(axis + 2) % 3];
		planepts[1][axis] = maxs[axis];

		planepts[2][(axis + 1) % 3] = static_cast<float>( floor(planepts[0][(axis + 1) % 3] - radius * sv + 0.5));
		planepts[2][(axis + 2) % 3] = static_cast<float>( floor(planepts[0][(axis + 2) % 3] + radius * cv + 0.5));
		planepts[2][axis] = maxs[axis];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
	}
}

const std::size_t c_brushCone_minSides = 3;
const std::size_t c_brushCone_maxSides = 32;
const char *const c_brushCone_name = "brushCone";

void Brush_ConstructCone(Brush &brush, const AABB &bounds, std::size_t sides, const char *shader,
                         const TextureProjection &projection)
{
	if (sides < c_brushCone_minSides) {
		globalErrorStream() << c_brushCone_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is "
		                    << Unsigned(c_brushCone_minSides) << "\n";
		return;
	}
	if (sides > c_brushCone_maxSides) {
		globalErrorStream() << c_brushCone_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is "
		                    << Unsigned(c_brushCone_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides + 1);

	Vector3 mins(vector3_subtracted(bounds.origin, bounds.extents));
	Vector3 maxs(vector3_added(bounds.origin, bounds.extents));

	float radius = max_extent(bounds.extents);
	const Vector3 &mid = bounds.origin;
	Vector3 planepts[3];

	planepts[0][0] = mins[0];
	planepts[0][1] = mins[1];
	planepts[0][2] = mins[2];
	planepts[1][0] = maxs[0];
	planepts[1][1] = mins[1];
	planepts[1][2] = mins[2];
	planepts[2][0] = maxs[0];
	planepts[2][1] = maxs[1];
	planepts[2][2] = mins[2];

	brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);

	for (std::size_t i = 0; i < sides; ++i) {
		double sv = sin(i * 3.14159265 * 2 / sides);
		double cv = cos(i * 3.14159265 * 2 / sides);

		planepts[0][0] = static_cast<float>( floor(mid[0] + radius * cv + 0.5));
		planepts[0][1] = static_cast<float>( floor(mid[1] + radius * sv + 0.5));
		planepts[0][2] = mins[2];

		planepts[1][0] = mid[0];
		planepts[1][1] = mid[1];
		planepts[1][2] = maxs[2];

		planepts[2][0] = static_cast<float>( floor(planepts[0][0] - radius * sv + 0.5));
		planepts[2][1] = static_cast<float>( floor(planepts[0][1] + radius * cv + 0.5));
		planepts[2][2] = maxs[2];

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
	}
}

const std::size_t c_brushSphere_minSides = 3;
const std::size_t c_brushSphere_maxSides = 31;
const char *const c_brushSphere_name = "brushSphere";

void Brush_ConstructSphere(Brush &brush, const AABB &bounds, std::size_t sides, const char *shader,
                           const TextureProjection &projection)
{
	if (sides < c_brushSphere_minSides) {
		globalErrorStream() << c_brushSphere_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is "
		                    << Unsigned(c_brushSphere_minSides) << "\n";
		return;
	}
	if (sides > c_brushSphere_maxSides) {
		globalErrorStream() << c_brushSphere_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is "
		                    << Unsigned(c_brushSphere_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides * sides);

	float radius = max_extent(bounds.extents);
	const Vector3 &mid = bounds.origin;
	Vector3 planepts[3];

	double dt = 2 * c_pi / sides;
	double dp = c_pi / sides;
	for (std::size_t i = 0; i < sides; i++) {
		for (std::size_t j = 0; j < sides - 1; j++) {
			double t = i * dt;
			double p = float(j * dp - c_pi / 2);

			planepts[0] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t, p), radius));
			planepts[1] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t, p + dp), radius));
			planepts[2] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t + dt, p + dp), radius));

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}

	{
		double p = (sides - 1) * dp - c_pi / 2;
		for (std::size_t i = 0; i < sides; i++) {
			double t = i * dt;

			planepts[0] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t, p), radius));
			planepts[1] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t + dt, p + dp), radius));
			planepts[2] = vector3_added(mid, vector3_scaled(vector3_for_spherical(t + dt, p), radius));

			brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
		}
	}
}

const std::size_t c_brushRock_minSides = 10;
const std::size_t c_brushRock_maxSides = 1000;
const char *const c_brushRock_name = "brushRock";

void Brush_ConstructRock(Brush &brush, const AABB &bounds, std::size_t sides, const char *shader,
                         const TextureProjection &projection)
{
	if (sides < c_brushRock_minSides) {
		globalErrorStream() << c_brushRock_name << ": sides " << Unsigned(sides) << ": too few sides, minimum is "
		                    << Unsigned(c_brushRock_minSides) << "\n";
		return;
	}
	if (sides > c_brushRock_maxSides) {
		globalErrorStream() << c_brushRock_name << ": sides " << Unsigned(sides) << ": too many sides, maximum is "
		                    << Unsigned(c_brushRock_maxSides) << "\n";
		return;
	}

	brush.clear();
	brush.reserve(sides * sides);

	float radius = max_extent(bounds.extents);
	const Vector3 &mid = bounds.origin;
	Vector3 planepts[3];

	for (std::size_t j = 0; j < sides; j++) {
		planepts[0][0] = rand() - (RAND_MAX / 2);
		planepts[0][1] = rand() - (RAND_MAX / 2);
		planepts[0][2] = rand() - (RAND_MAX / 2);
		vector3_normalise(planepts[0]);

		// find two vectors that are perpendicular to planepts[0]
		ComputeAxisBase(planepts[0], planepts[1], planepts[2]);

		planepts[0] = vector3_added(mid, vector3_scaled(planepts[0], radius));
		planepts[1] = vector3_added(planepts[0], vector3_scaled(planepts[1], radius));
		planepts[2] = vector3_added(planepts[0], vector3_scaled(planepts[2], radius));

#if 0
		// make sure the orientation is right
		if ( vector3_dot( vector3_subtracted( planepts[0], mid ), vector3_cross( vector3_subtracted( planepts[1], mid ), vector3_subtracted( planepts[2], mid ) ) ) > 0 ) {
			Vector3 h;
			h = planepts[1];
			planepts[1] = planepts[2];
			planepts[2] = h;
			globalOutputStream() << "flip\n";
		}
		else{
			globalOutputStream() << "no flip\n";
		}
#endif

		brush.addPlane(planepts[0], planepts[1], planepts[2], shader, projection);
	}
}

int GetViewAxis()
{
	switch (GlobalXYWnd_getCurrentViewType()) {
	case XY:
		return 2;
	case XZ:
		return 1;
	case YZ:
		return 0;
	}
	return 2;
}

void Brush_ConstructPrefab(Brush &brush, EBrushPrefab type, const AABB &bounds, std::size_t sides, const char *shader,
                           const TextureProjection &projection)
{
	switch (type) {
	case eBrushCuboid: {
		UndoableCommand undo("brushCuboid");

		Brush_ConstructCuboid(brush, bounds, shader, projection);
	}
	break;
	case eBrushPrism: {
		int axis = GetViewAxis();
		StringOutputStream command;
		command << c_brushPrism_name << " -sides " << Unsigned(sides) << " -axis " << axis;
		UndoableCommand undo(command.c_str());

		Brush_ConstructPrism(brush, bounds, sides, axis, shader, projection);
	}
	break;
	case eBrushCone: {
		StringOutputStream command;
		command << c_brushCone_name << " -sides " << Unsigned(sides);
		UndoableCommand undo(command.c_str());

		Brush_ConstructCone(brush, bounds, sides, shader, projection);
	}
	break;
	case eBrushSphere: {
		StringOutputStream command;
		command << c_brushSphere_name << " -sides " << Unsigned(sides);
		UndoableCommand undo(command.c_str());

		Brush_ConstructSphere(brush, bounds, sides, shader, projection);
	}
	break;
	case eBrushRock: {
		StringOutputStream command;
		command << c_brushRock_name << " -sides " << Unsigned(sides);
		UndoableCommand undo(command.c_str());

		Brush_ConstructRock(brush, bounds, sides, shader, projection);
	}
	break;
	}
}


void ConstructRegionBrushes(scene::Node *brushes[6], const Vector3 &region_mins, const Vector3 &region_maxs)
{
	{
		// set mins
		Vector3 mins(region_mins[0] - 32, region_mins[1] - 32, region_mins[2] - 32);

		// vary maxs
		for (std::size_t i = 0; i < 3; i++) {
			Vector3 maxs(region_maxs[0] + 32, region_maxs[1] + 32, region_maxs[2] + 32);
			maxs[i] = region_mins[i];
			Brush_ConstructCuboid(*Node_getBrush(*brushes[i]), aabb_for_minmax(mins, maxs), texdef_name_default(),
			                      TextureProjection());
		}
	}

	{
		// set maxs
		Vector3 maxs(region_maxs[0] + 32, region_maxs[1] + 32, region_maxs[2] + 32);

		// vary mins
		for (std::size_t i = 0; i < 3; i++) {
			Vector3 mins(region_mins[0] - 32, region_mins[1] - 32, region_mins[2] - 32);
			mins[i] = region_maxs[i];
			Brush_ConstructCuboid(*Node_getBrush(*brushes[i + 3]), aabb_for_minmax(mins, maxs), texdef_name_default(),
			                      TextureProjection());
		}
	}
}


void Scene_BrushSetTexdef_Selected(scene::Graph &graph, const TextureProjection &projection, bool ignorebasis)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.SetTexdef(projection, ignorebasis);
	});
	SceneChangeNotify();
}

void Scene_BrushSetTexdef_Component_Selected(scene::Graph &graph, const TextureProjection &projection, bool ignorebasis)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.SetTexdef(projection, ignorebasis);
	});
	SceneChangeNotify();
}


void Scene_BrushSetFlags_Selected(scene::Graph &graph, const ContentsFlagsValue &flags)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.SetFlags(flags);
	});
	SceneChangeNotify();
}

void Scene_BrushSetFlags_Component_Selected(scene::Graph &graph, const ContentsFlagsValue &flags)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.SetFlags(flags);
	});
	SceneChangeNotify();
}

void Scene_BrushShiftTexdef_Selected(scene::Graph &graph, float s, float t)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.ShiftTexdef(s, t);
	});
	SceneChangeNotify();
}

void Scene_BrushShiftTexdef_Component_Selected(scene::Graph &graph, float s, float t)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.ShiftTexdef(s, t);
	});
	SceneChangeNotify();
}

void Scene_BrushScaleTexdef_Selected(scene::Graph &graph, float s, float t)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.ScaleTexdef(s, t);
	});
	SceneChangeNotify();
}

void Scene_BrushScaleTexdef_Component_Selected(scene::Graph &graph, float s, float t)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.ScaleTexdef(s, t);
	});
	SceneChangeNotify();
}

void Scene_BrushRotateTexdef_Selected(scene::Graph &graph, float angle)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.RotateTexdef(angle);
	});
	SceneChangeNotify();
}

void Scene_BrushRotateTexdef_Component_Selected(scene::Graph &graph, float angle)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.RotateTexdef(angle);
	});
	SceneChangeNotify();
}


void Scene_BrushSetShader_Selected(scene::Graph &graph, const char *name)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.SetShader(name);
	});
	SceneChangeNotify();
}

void Scene_BrushSetShader_Component_Selected(scene::Graph &graph, const char *name)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.SetShader(name);
	});
	SceneChangeNotify();
}

void Scene_BrushSetDetail_Selected(scene::Graph &graph, bool detail)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.setDetail(detail);
	});
	SceneChangeNotify();
}

bool Face_FindReplaceShader(Face &face, const char *find, const char *replace)
{
	if (shader_equal(face.GetShader(), find)) {
		face.SetShader(replace);
		return true;
	}
	return false;
}

bool DoingSearch(const char *repl)
{
	return (repl == NULL || (strcmp("textures/", repl) == 0));
}

void Scene_BrushFindReplaceShader(scene::Graph &graph, const char *find, const char *replace)
{
	if (DoingSearch(replace)) {
		Scene_ForEachBrush_ForEachFaceInstance(graph, [&](FaceInstance &faceinst) {
			if (shader_equal(faceinst.getFace().GetShader(), find)) {
				faceinst.setSelected(SelectionSystem::eFace, true);
			}
		});
	} else {
		Scene_ForEachBrush_ForEachFace(graph, [&](Face &face) {
			Face_FindReplaceShader(face, find, replace);
		});
	}
}

void Scene_BrushFindReplaceShader_Selected(scene::Graph &graph, const char *find, const char *replace)
{
	if (DoingSearch(replace)) {
		Scene_ForEachSelectedBrush_ForEachFaceInstance(graph, [&](FaceInstance &faceinst) {
			if (shader_equal(faceinst.getFace().GetShader(), find)) {
				faceinst.setSelected(SelectionSystem::eFace, true);
			}
		});
	} else {
		Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
			Face_FindReplaceShader(face, find, replace);
		});
	}
}

// TODO: find for components
// d1223m: dont even know what they are...
void Scene_BrushFindReplaceShader_Component_Selected(scene::Graph &graph, const char *find, const char *replace)
{
	if (DoingSearch(replace)) {

	} else {
		Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
			Face_FindReplaceShader(face, find, replace);
		});
	}
}


void Scene_BrushFitTexture_Selected(scene::Graph &graph, float s_repeat, float t_repeat)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.FitTexture(s_repeat, t_repeat);
	});
	SceneChangeNotify();
}
void Scene_BrushFitTexture_Component_Selected(scene::Graph &graph, float s_repeat, float t_repeat)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.FitTexture(s_repeat, t_repeat);
	});
	SceneChangeNotify();
}

void Scene_BrushAlignTexture_Selected(scene::Graph &graph, int alignment)
{
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		face.AlignTexture(alignment);
	});
	SceneChangeNotify();
}
void Scene_BrushAlignTexture_Component_Selected(scene::Graph &graph,  int alignment)
{
	Scene_ForEachSelectedBrushFace(graph, [&](Face &face) {
		face.AlignTexture(alignment);
	});
	SceneChangeNotify();
}

TextureProjection g_defaultTextureProjection;

const TextureProjection &TextureTransform_getDefault()
{
	TexDef_Construct_Default(g_defaultTextureProjection);
	return g_defaultTextureProjection;
}

void Scene_BrushConstructPrefab(scene::Graph &graph, EBrushPrefab type, std::size_t sides, const char *shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path &path = GlobalSelectionSystem().ultimateSelected().path();

		Brush *brush = Node_getBrush(path.top());
		if (brush != 0) {
			AABB bounds = brush->localAABB(); // copy bounds because the brush will be modified
			Brush_ConstructPrefab(*brush, type, bounds, sides, shader, TextureTransform_getDefault());
			SceneChangeNotify();
		}
	}
}

void Scene_BrushResize_Selected(scene::Graph &graph, const AABB &bounds, const char *shader)
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path &path = GlobalSelectionSystem().ultimateSelected().path();

		Brush *brush = Node_getBrush(path.top());
		if (brush != 0) {
			Brush_ConstructCuboid(*brush, bounds, shader, TextureTransform_getDefault());
			SceneChangeNotify();
		}
	}
}

bool Brush_hasShader(const Brush &brush, const char *name)
{
	for (Brush::const_iterator i = brush.begin(); i != brush.end(); ++i) {
		if (shader_equal((*i)->GetShader(), name)) {
			return true;
		}
	}
	return false;
}

class BrushSelectByShaderWalker : public scene::Graph::Walker {
const char *m_name;
public:
BrushSelectByShaderWalker(const char *name)
	: m_name(name)
{
}

bool pre(const scene::Path &path, scene::Instance &instance) const
{
	if (path.top().get().visible()) {
		Brush *brush = Node_getBrush(path.top());
		if (brush != 0 && Brush_hasShader(*brush, m_name)) {
			Instance_getSelectable(instance)->setSelected(true);
		}
	}
	return true;
}
};

void Scene_BrushSelectByShader(scene::Graph &graph, const char *name)
{
	graph.traverse(BrushSelectByShaderWalker(name));
}

void Scene_BrushSelectByShader_Component(scene::Graph &graph, const char *name)
{
	Scene_ForEachSelectedBrush_ForEachFaceInstance(graph, [&](FaceInstance &face) {
		printf("checking %s = %s\n", face.getFace().GetShader(), name);
		if (shader_equal(face.getFace().GetShader(), name)) {
			face.setSelected(SelectionSystem::eFace, true);
		}
	});
}

void Scene_BrushGetTexdef_Selected(scene::Graph &graph, TextureProjection &projection)
{
	bool done = false;
	Scene_ForEachSelectedBrush_ForEachFace(graph, [&](Face &face) {
		if (!done) {
			done = true;
			face.GetTexdef(projection);
		}
	});
}

void Scene_BrushGetTexdef_Component_Selected(scene::Graph &graph, TextureProjection &projection)
{
#if 1
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance &faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetTexdef(projection);
	}
#else
	FaceGetTexdef visitor( projection );
	Scene_ForEachSelectedBrushFace( graph, visitor );
#endif
}

void Scene_BrushGetShaderSize_Component_Selected(scene::Graph &graph, size_t &width, size_t &height)
{
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance &faceInstance = g_SelectedFaceInstances.last();
		width = faceInstance.getFace().getShader().width();
		height = faceInstance.getFace().getShader().height();
	}
}


void Scene_BrushGetFlags_Selected(scene::Graph &graph, ContentsFlagsValue &flags)
{
#if 1
	if (GlobalSelectionSystem().countSelected() != 0) {
		BrushInstance *brush = Instance_getBrush(GlobalSelectionSystem().ultimateSelected());
		if (brush != 0) {
			bool done = false;
			Brush_forEachFace(*brush, [&](Face &face) {
				if (!done) {
					done = true;
					face.GetFlags(flags);
				}
			});
		}
	}
#else
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceGetFlags( flags ) );
#endif
}

void Scene_BrushGetFlags_Component_Selected(scene::Graph &graph, ContentsFlagsValue &flags)
{
#if 1
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance &faceInstance = g_SelectedFaceInstances.last();
		faceInstance.getFace().GetFlags(flags);
	}
#else
	Scene_ForEachSelectedBrushFace( graph, FaceGetFlags( flags ) );
#endif
}


void Scene_BrushGetShader_Selected(scene::Graph &graph, CopiedString &shader)
{
#if 1
	if (GlobalSelectionSystem().countSelected() != 0) {
		BrushInstance *brush = Instance_getBrush(GlobalSelectionSystem().ultimateSelected());
		if (brush != 0) {
			bool done = false;
			Brush_forEachFace(*brush, [&](Face &face) {
				if (!done) {
					done = true;
					shader = face.GetShader();
				}
			});
		}
	}
#else
	Scene_ForEachSelectedBrush_ForEachFace( graph, FaceGetShader( shader ) );
#endif
}

void Scene_BrushGetShader_Component_Selected(scene::Graph &graph, CopiedString &shader)
{
#if 1
	if (!g_SelectedFaceInstances.empty()) {
		FaceInstance &faceInstance = g_SelectedFaceInstances.last();
		shader = faceInstance.getFace().GetShader();
	}
#else
	FaceGetShader visitor( shader );
	Scene_ForEachSelectedBrushFace( graph, visitor );
#endif
}


class filter_face_shader : public FaceFilter {
const char *m_shader;
public:
filter_face_shader(const char *shader) : m_shader(shader)
{
}

bool filter(const Face &face) const
{
	return shader_equal(face.GetShader(), m_shader);
}
};

class filter_face_shader_prefix : public FaceFilter {
const char *m_prefix;
public:
filter_face_shader_prefix(const char *prefix) : m_prefix(prefix)
{
}

bool filter(const Face &face) const
{
	return shader_equal_n(face.GetShader(), m_prefix, strlen(m_prefix));
}
};

class filter_face_flags : public FaceFilter {
int m_flags;
public:
filter_face_flags(int flags) : m_flags(flags)
{
}

bool filter(const Face &face) const
{
	return (face.getShader().shaderFlags() & m_flags) != 0;
}
};

class filter_face_contents : public FaceFilter {
int m_contents;
public:
filter_face_contents(int contents) : m_contents(contents)
{
}

bool filter(const Face &face) const
{
	return (face.getShader().m_flags.m_contentFlags & m_contents) != 0;
}
};


class filter_brush_any_face : public BrushFilter {
FaceFilter *m_filter;
public:
filter_brush_any_face(FaceFilter *filter) : m_filter(filter)
{
}

bool filter(const Brush &brush) const
{
	bool filtered = false;
	Brush_forEachFace(brush, [&](Face &face) {
			if (m_filter->filter(face)) {
				filtered = true;
			}
		});
	return filtered;
}
};

class filter_brush_all_faces : public BrushFilter {
FaceFilter *m_filter;
public:
filter_brush_all_faces(FaceFilter *filter) : m_filter(filter)
{
}

bool filter(const Brush &brush) const
{
	bool filtered = true;
	Brush_forEachFace(brush, [&](Face &face) {
			if (!m_filter->filter(face)) {
				filtered = false;
			}
		});
	return filtered;
}
};


filter_face_flags g_filter_face_clip(QER_CLIP);
filter_brush_all_faces g_filter_brush_clip(&g_filter_face_clip);

filter_face_shader g_filter_face_clip_q2("textures/clip");
filter_brush_all_faces g_filter_brush_clip_q2(&g_filter_face_clip_q2);

filter_face_shader g_filter_face_weapclip("textures/common/weapclip");
filter_brush_all_faces g_filter_brush_weapclip(&g_filter_face_weapclip);

filter_face_shader g_filter_face_commonclip("textures/common/clip");
filter_brush_all_faces g_filter_brush_commonclip(&g_filter_face_commonclip);

filter_face_shader g_filter_face_fullclip("textures/common/fullclip");
filter_brush_all_faces g_filter_brush_fullclip(&g_filter_face_fullclip);

filter_face_shader g_filter_face_botclip("textures/common/botclip");
filter_brush_all_faces g_filter_brush_botclip(&g_filter_face_botclip);

filter_face_shader_prefix g_filter_face_caulk("textures/common/caulk");
filter_brush_all_faces g_filter_brush_caulk(&g_filter_face_caulk);

filter_face_shader_prefix g_filter_face_caulk_ja("textures/system/caulk");
filter_brush_all_faces g_filter_brush_caulk_ja(&g_filter_face_caulk_ja);

filter_face_shader_prefix g_filter_face_liquids("textures/liquids/");
filter_brush_any_face g_filter_brush_liquids(&g_filter_face_liquids);

filter_face_shader g_filter_face_hint("textures/common/hint");
filter_brush_any_face g_filter_brush_hint(&g_filter_face_hint);

filter_face_shader g_filter_face_hint_q2("textures/hint");
filter_brush_any_face g_filter_brush_hint_q2(&g_filter_face_hint_q2);

filter_face_shader g_filter_face_hint_ja("textures/system/hint");
filter_brush_any_face g_filter_brush_hint_ja(&g_filter_face_hint_ja);

filter_face_shader g_filter_face_areaportal("textures/common/areaportal");
filter_brush_all_faces g_filter_brush_areaportal(&g_filter_face_areaportal);

filter_face_shader g_filter_face_visportal("textures/editor/visportal");
filter_brush_any_face g_filter_brush_visportal(&g_filter_face_visportal);

filter_face_shader g_filter_face_clusterportal("textures/common/clusterportal");
filter_brush_all_faces g_filter_brush_clusterportal(&g_filter_face_clusterportal);

filter_face_shader g_filter_face_lightgrid("textures/common/lightgrid");
filter_brush_all_faces g_filter_brush_lightgrid(&g_filter_face_lightgrid);

filter_face_flags g_filter_face_translucent(QER_TRANS);
filter_brush_all_faces g_filter_brush_translucent(&g_filter_face_translucent);

filter_face_contents g_filter_face_detail(BRUSH_DETAIL_MASK);
filter_brush_all_faces g_filter_brush_detail(&g_filter_face_detail);

filter_face_shader_prefix g_filter_face_decals("textures/decals/");
filter_brush_any_face g_filter_brush_decals(&g_filter_face_decals);


void BrushFilters_construct()
{
	add_brush_filter(g_filter_brush_clip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_clip_q2, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_weapclip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_fullclip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_commonclip, EXCLUDE_CLIP);
	add_brush_filter(g_filter_brush_botclip, EXCLUDE_BOTCLIP);
	add_brush_filter(g_filter_brush_caulk, EXCLUDE_CAULK);
	add_brush_filter(g_filter_brush_caulk_ja, EXCLUDE_CAULK);
	add_face_filter(g_filter_face_caulk, EXCLUDE_CAULK);
	add_face_filter(g_filter_face_caulk_ja, EXCLUDE_CAULK);
	add_brush_filter(g_filter_brush_liquids, EXCLUDE_LIQUIDS);
	add_brush_filter(g_filter_brush_hint, EXCLUDE_HINTSSKIPS);
	add_brush_filter(g_filter_brush_hint_q2, EXCLUDE_HINTSSKIPS);
	add_brush_filter(g_filter_brush_hint_ja, EXCLUDE_HINTSSKIPS);
	add_brush_filter(g_filter_brush_clusterportal, EXCLUDE_CLUSTERPORTALS);
	add_brush_filter(g_filter_brush_visportal, EXCLUDE_VISPORTALS);
	add_brush_filter(g_filter_brush_areaportal, EXCLUDE_AREAPORTALS);
	add_brush_filter(g_filter_brush_translucent, EXCLUDE_TRANSLUCENT);
	add_brush_filter(g_filter_brush_detail, EXCLUDE_DETAILS);
	add_brush_filter(g_filter_brush_detail, EXCLUDE_STRUCTURAL, true);
	add_brush_filter(g_filter_brush_lightgrid, EXCLUDE_LIGHTGRID);
	add_brush_filter(g_filter_brush_decals, EXCLUDE_DECALS);
}

#if 0

void normalquantisation_draw(){
	glPointSize( 1 );
	glBegin( GL_POINTS );
	for ( std::size_t i = 0; i <= c_quantise_normal; ++i )
	{
		for ( std::size_t j = 0; j <= c_quantise_normal; ++j )
		{
			Normal3f vertex( normal3f_normalised( Normal3f(
								      static_cast<float>( c_quantise_normal - j - i ),
								      static_cast<float>( i ),
								      static_cast<float>( j )
								      ) ) );
			VectorScale( normal3f_to_array( vertex ), 64.f, normal3f_to_array( vertex ) );
			glVertex3fv( normal3f_to_array( vertex ) );
			vertex.x = -vertex.x;
			glVertex3fv( normal3f_to_array( vertex ) );
		}
	}
	glEnd();
}

class RenderableNormalQuantisation : public OpenGLRenderable
{
public:
void render( RenderStateFlags state ) const {
	normalquantisation_draw();
}
};

const float g_test_quantise_normal = 1.f / static_cast<float>( 1 << 3 );

class TestNormalQuantisation
{
void check_normal( const Normal3f& normal, const Normal3f& other ){
	spherical_t spherical = spherical_from_normal3f( normal );
	double longditude = RAD2DEG( spherical.longditude );
	double latitude = RAD2DEG( spherical.latitude );
	double x = cos( spherical.longditude ) * sin( spherical.latitude );
	double y = sin( spherical.longditude ) * sin( spherical.latitude );
	double z = cos( spherical.latitude );

	ASSERT_MESSAGE( normal3f_dot( normal, other ) > 0.99, "bleh" );
}

void test_normal( const Normal3f& normal ){
	Normal3f test = normal3f_from_spherical( spherical_from_normal3f( normal ) );
	check_normal( normal, test );

	EOctant octant = normal3f_classify_octant( normal );
	Normal3f folded = normal3f_fold_octant( normal, octant );
	ESextant sextant = normal3f_classify_sextant( folded );
	folded = normal3f_fold_sextant( folded, sextant );

	double scale = static_cast<float>( c_quantise_normal ) / ( folded.x + folded.y + folded.z );

	double zbits = folded.z * scale;
	double ybits = folded.y * scale;

	std::size_t zbits_q = static_cast<std::size_t>( zbits );
	std::size_t ybits_q = static_cast<std::size_t>( ybits );

	ASSERT_MESSAGE( zbits_q <= ( c_quantise_normal / 8 ) * 3, "bleh" );
	ASSERT_MESSAGE( ybits_q <= ( c_quantise_normal / 2 ), "bleh" );
	ASSERT_MESSAGE( zbits_q + ( ( c_quantise_normal / 2 ) - ybits_q ) <= ( c_quantise_normal / 2 ), "bleh" );

	std::size_t y_t = ( zbits_q < ( c_quantise_normal / 4 ) ) ? ybits_q : ( c_quantise_normal / 2 ) - ybits_q;
	std::size_t z_t = ( zbits_q < ( c_quantise_normal / 4 ) ) ? zbits_q : ( c_quantise_normal / 2 ) - zbits_q;
	std::size_t index = ( c_quantise_normal / 4 ) * y_t + z_t;
	ASSERT_MESSAGE( index <= ( c_quantise_normal / 4 ) * ( c_quantise_normal / 2 ), "bleh" );

	Normal3f tmp( c_quantise_normal - zbits_q - ybits_q, ybits_q, zbits_q );
	tmp = normal3f_normalised( tmp );

	Normal3f unfolded = normal3f_unfold_octant( normal3f_unfold_sextant( tmp, sextant ), octant );

	check_normal( normal, unfolded );

	double dot = normal3f_dot( normal, unfolded );
	float length = VectorLength( normal3f_to_array( unfolded ) );
	float inv_length = 1 / length;

	Normal3f quantised = normal3f_quantised( normal );
	check_normal( normal, quantised );
}
void test2( const Normal3f& normal, const Normal3f& other ){
	if ( normal3f_quantised( normal ) != normal3f_quantised( other ) ) {
		int bleh = 0;
	}
}

static Normal3f normalise( float x, float y, float z ){
	return normal3f_normalised( Normal3f( x, y, z ) );
}

float vec_rand(){
	return static_cast<float>( rand() - ( RAND_MAX / 2 ) );
}

Normal3f normal3f_rand(){
	return normalise( vec_rand(), vec_rand(), vec_rand() );
}

public:
TestNormalQuantisation(){
	for ( int i = 4096; i > 0; --i )
		test_normal( normal3f_rand() );

	test_normal( normalise( 1, 0, 0 ) );
	test_normal( normalise( 0, 1, 0 ) );
	test_normal( normalise( 0, 0, 1 ) );
	test_normal( normalise( 1, 1, 0 ) );
	test_normal( normalise( 1, 0, 1 ) );
	test_normal( normalise( 0, 1, 1 ) );

	test_normal( normalise( 10000, 10000, 10000 ) );
	test_normal( normalise( 10000, 10000, 10001 ) );
	test_normal( normalise( 10000, 10000, 10002 ) );
	test_normal( normalise( 10000, 10000, 10010 ) );
	test_normal( normalise( 10000, 10000, 10020 ) );
	test_normal( normalise( 10000, 10000, 10030 ) );
	test_normal( normalise( 10000, 10000, 10100 ) );
	test_normal( normalise( 10000, 10000, 10101 ) );
	test_normal( normalise( 10000, 10000, 10102 ) );
	test_normal( normalise( 10000, 10000, 10200 ) );
	test_normal( normalise( 10000, 10000, 10201 ) );
	test_normal( normalise( 10000, 10000, 10202 ) );
	test_normal( normalise( 10000, 10000, 10203 ) );
	test_normal( normalise( 10000, 10000, 10300 ) );


	test2( normalise( 10000, 10000, 10000 ), normalise( 10000, 10000, 10001 ) );
	test2( normalise( 10000, 10000, 10001 ), normalise( 10000, 10001, 10000 ) );
}
};

TestNormalQuantisation g_testNormalQuantisation;


#endif

#if 0
class TestSelectableObserver : public observer_template<const Selectable&>
{
public:
void notify( const Selectable& arguments ){
	bool bleh = arguments.isSelected();
}
};

inline void test_bleh(){
	TestSelectableObserver test;
	ObservableSelectableInstance< SingleObservable< SelectionChangeCallback > > bleh;
	bleh.attach( test );
	bleh.setSelected( true );
	bleh.detach( test );
}

class TestBleh
{
public:
TestBleh(){
	test_bleh();
}
};

const TestBleh testbleh;
#endif


#if 0
class TestRefcountedString
{
public:
TestRefcountedString(){
	{
		// copy construct
		SmartString string1( "string1" );
		SmartString string2( string1 );
		SmartString string3( string2 );
	}
	{
		// refcounted assignment
		SmartString string1( "string1" );
		SmartString string2( "string2" );
		string1 = string2;
	}
	{
		// copy assignment
		SmartString string1( "string1" );
		SmartString string2( "string2" );
		string1 = string2.c_str();
	}
	{
		// self-assignment
		SmartString string1( "string1" );
		string1 = string1;
	}
	{
		// self-assignment via another reference
		SmartString string1( "string1" );
		SmartString string2( string1 );
		string1 = string2;
	}
}
};

const TestRefcountedString g_testRefcountedString;

#endif

void Select_MakeDetail()
{
	UndoableCommand undo("brushSetDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), true);
}

void Select_MakeStructural()
{
	UndoableCommand undo("brushClearDetail");
	Scene_BrushSetDetail_Selected(GlobalSceneGraph(), false);
}

class BrushMakeSided {
std::size_t m_count;
public:
BrushMakeSided(std::size_t count)
	: m_count(count)
{
}

void set()
{
	Scene_BrushConstructPrefab(GlobalSceneGraph(), eBrushPrism, m_count,
	                           TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
}

typedef MemberCaller<BrushMakeSided, void (), &BrushMakeSided::set> SetCaller;
};


BrushMakeSided g_brushmakesided3(3);
BrushMakeSided g_brushmakesided4(4);
BrushMakeSided g_brushmakesided5(5);
BrushMakeSided g_brushmakesided6(6);
BrushMakeSided g_brushmakesided7(7);
BrushMakeSided g_brushmakesided8(8);
BrushMakeSided g_brushmakesided9(9);

inline int axis_for_viewtype(int viewtype)
{
	switch (viewtype) {
	case XY:
		return 2;
	case XZ:
		return 1;
	case YZ:
		return 0;
	}
	return 2;
}

class BrushPrefab {
EBrushPrefab m_type;
public:
BrushPrefab(EBrushPrefab type)
	: m_type(type)
{
}

void set()
{
	DoSides(m_type, axis_for_viewtype(GetViewAxis()));
}

typedef MemberCaller<BrushPrefab, void (), &BrushPrefab::set> SetCaller;
};

BrushPrefab g_brushprism(eBrushPrism);
BrushPrefab g_brushcone(eBrushCone);
BrushPrefab g_brushsphere(eBrushSphere);
BrushPrefab g_brushrock(eBrushRock);


void FlipClip();

void SplitClip();

void Clip();

void OnClipMode(bool enable);

bool ClipMode();


void ClipSelected()
{
	if (ClipMode()) {
		UndoableCommand undo("clipperClip");
		Clip();
	}
}

void SplitSelected()
{
	if (ClipMode()) {
		UndoableCommand undo("clipperSplit");
		SplitClip();
	}
}

void FlipClipper()
{
	FlipClip();
}


Callback<void()> g_texture_lock_status_changed;
ConstReferenceCaller<bool, void(const Callback<void(bool)> &), PropertyImpl<bool>::Export> g_texdef_movelock_caller(
	g_brush_texturelock_enabled);
ToggleItem g_texdef_movelock_item(g_texdef_movelock_caller);

void Texdef_ToggleMoveLock()
{
	g_brush_texturelock_enabled = !g_brush_texturelock_enabled;
	g_texdef_movelock_item.update();
	g_texture_lock_status_changed();
}


void Brush_registerCommands()
{
	GlobalToggles_insert("TogTexLock", makeCallbackF(Texdef_ToggleMoveLock),
	                     ToggleItem::AddCallbackCaller(g_texdef_movelock_item),
	                     Accelerator('T', (GdkModifierType) GDK_SHIFT_MASK));

	GlobalCommands_insert("BrushPrism", BrushPrefab::SetCaller(g_brushprism));
	GlobalCommands_insert("BrushCone", BrushPrefab::SetCaller(g_brushcone));
	GlobalCommands_insert("BrushSphere", BrushPrefab::SetCaller(g_brushsphere));
	GlobalCommands_insert("BrushRock", BrushPrefab::SetCaller(g_brushrock));

	GlobalCommands_insert("Brush3Sided", BrushMakeSided::SetCaller(g_brushmakesided3),
	                      Accelerator('3', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush4Sided", BrushMakeSided::SetCaller(g_brushmakesided4),
	                      Accelerator('4', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush5Sided", BrushMakeSided::SetCaller(g_brushmakesided5),
	                      Accelerator('5', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush6Sided", BrushMakeSided::SetCaller(g_brushmakesided6),
	                      Accelerator('6', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush7Sided", BrushMakeSided::SetCaller(g_brushmakesided7),
	                      Accelerator('7', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush8Sided", BrushMakeSided::SetCaller(g_brushmakesided8),
	                      Accelerator('8', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("Brush9Sided", BrushMakeSided::SetCaller(g_brushmakesided9),
	                      Accelerator('9', (GdkModifierType) GDK_CONTROL_MASK));

	GlobalCommands_insert("ClipSelected", makeCallbackF(ClipSelected), Accelerator(GDK_KEY_Return));
	GlobalCommands_insert("SplitSelected", makeCallbackF(SplitSelected),
	                      Accelerator(GDK_KEY_Return, (GdkModifierType) GDK_SHIFT_MASK));
	GlobalCommands_insert("FlipClip", makeCallbackF(FlipClipper),
	                      Accelerator(GDK_KEY_Return, (GdkModifierType) GDK_CONTROL_MASK));

	GlobalCommands_insert("MakeDetail", makeCallbackF(Select_MakeDetail),
	                      Accelerator('M', (GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("MakeStructural", makeCallbackF(Select_MakeStructural),
	                      Accelerator('S', (GdkModifierType) (GDK_SHIFT_MASK | GDK_CONTROL_MASK)));
}

void Brush_constructMenu(ui::Menu menu)
{
	create_menu_item_with_mnemonic(menu, "Prism...", "BrushPrism");
	create_menu_item_with_mnemonic(menu, "Cone...", "BrushCone");
	create_menu_item_with_mnemonic(menu, "Sphere...", "BrushSphere");
	create_menu_item_with_mnemonic(menu, "Rock...", "BrushRock");
	menu_separator(menu);
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic(menu, "CSG");
		/*if (g_Layout_enableOpenStepUX.m_value) {
		    menu_tearoff(menu_in_menu);
		   }*/
		create_menu_item_with_mnemonic(menu_in_menu, "Make _Hollow", "CSGMakeHollow");
		create_menu_item_with_mnemonic(menu_in_menu, "Make _Room", "CSGMakeRoom");
		create_menu_item_with_mnemonic(menu_in_menu, "CSG _Subtract", "CSGSubtract");
		create_menu_item_with_mnemonic(menu_in_menu, "CSG _Merge", "CSGMerge");
	}
	menu_separator(menu);
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic(menu, "Clipper");
		/*if (g_Layout_enableOpenStepUX.m_value) {
		    menu_tearoff(menu_in_menu);
		   }*/

		create_menu_item_with_mnemonic(menu_in_menu, "Clip selection", "ClipSelected");
		create_menu_item_with_mnemonic(menu_in_menu, "Split selection", "SplitSelected");
		create_menu_item_with_mnemonic(menu_in_menu, "Flip Clip orientation", "FlipClip");
	}
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, "Make detail", "MakeDetail");
	create_menu_item_with_mnemonic(menu, "Make structural", "MakeStructural");
	create_menu_item_with_mnemonic(menu, "Snap selection to _grid", "SnapToGrid");

	create_check_menu_item_with_mnemonic(menu, "Texture Lock", "TogTexLock");
	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, "Copy Face Texture", "FaceCopyTexture");
	create_menu_item_with_mnemonic(menu, "Paste Face Texture", "FacePasteTexture");

	command_connect_accelerator("Brush3Sided");
	command_connect_accelerator("Brush4Sided");
	command_connect_accelerator("Brush5Sided");
	command_connect_accelerator("Brush6Sided");
	command_connect_accelerator("Brush7Sided");
	command_connect_accelerator("Brush8Sided");
	command_connect_accelerator("Brush9Sided");
}
