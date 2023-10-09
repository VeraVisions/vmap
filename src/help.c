/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
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

   -------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "vmap.h"



struct HelpOption
{
	const char* name;
	const char* description;
};

void HelpOptions(const char* group_name, int indentation, int width, struct HelpOption* options, int count)
{
	indentation *= 2;
	char* indent = malloc(indentation+1);
	memset(indent, ' ', indentation);
	indent[indentation] = 0;
	printf("%s%s:\n", indent, group_name);
	indentation += 2;
	indent = realloc(indent, indentation+1);
	memset(indent, ' ', indentation);
	indent[indentation] = 0;

	int i;
	for ( i = 0; i < count; i++ )
	{
		int printed = printf("%s%-24s  ", indent, options[i].name);
		int descsz = strlen(options[i].description);
		int j = 0;
		while ( j < descsz && descsz-j > width - printed )
		{
			if ( j != 0 )
				printf("%s%26c",indent,' ');
			int fragment = width - printed;
			while ( fragment > 0 && options[i].description[j+fragment-1] != ' ')
				fragment--;
			j += fwrite(options[i].description+j, sizeof(char), fragment, stdout);
			putchar('\n');
			printed = indentation+26;
		}
		if ( j == 0 )
		{
			printf("%s\n",options[i].description+j);
		}
		else if ( j < descsz )
		{
			printf("%s%26c%s\n",indent,' ',options[i].description+j);
		}
	}

	putchar('\n');

	free(indent);
}

void HelpBsp()
{
	struct HelpOption bsp[] = {
		{"-bsp <filename.map>", "Switch that enters this stage"},
		{"-altsplit", "Alternate BSP tree splitting weights (should give more fps)"},
		{"-bspfile <filename.bsp>", "BSP file to write"},
		{"-celshader <shadername>", "Sets a global cel shader name"},
		{"-custinfoparms", "Read scripts/custinfoparms.txt"},
		{"-debuginset", "Push all triangle vertexes towards the triangle center"},
		{"-debugportals", "Make BSP portals visible in the map"},
		{"-debugsurfaces", "Color the vertexes according to the index of the surface"},
		{"-deep", "Use detail brushes in the BSP tree, but at lowest priority (should give more fps)"},
		{"-de <F>", "Distance epsilon for plane snapping etc."},
		{"-fakemap", "Write fakemap.map containing all world brushes"},
		{"-flat", "Enable flat shading (good for combining with -celshader)"},
		{"-fulldetail", "Treat detail brushes as structural ones"},
		{"-leaktest", "Continue even if a leak was found"},
		{"-linfile <filename.lin>", "Line file to write"},
		{"-meta", "Combine adjacent triangles of the same texture to surfaces (ALWAYS USE THIS)"},
		{"-minsamplesize <N>", "Sets minimum lightmap resolution in luxels/qu"},
		{"-mi <N>", "Sets the maximum number of indexes per surface"},
		{"-mv <N>", "Sets the maximum number of vertices of a lightmapped surface"},
		{"-ne <F>", "Normal epsilon for plane snapping etc."},
		{"-nocurves", "Turn off support for patches"},
		{"-nodetail", "Leave out detail brushes"},
		{"-nofog", "Turn off support for fog volumes"},
		{"-nohint", "Turn off support for hint brushes"},
		{"-nosubdivide", "Turn off support for `q3map_tessSize` (breaks water vertex deforms)"},
		{"-notjunc", "Do not fix T-junctions (causes cracks between triangles, do not use)"},
		{"-nowater", "Turn off support for water, slime or lava (Stef, this is for you)"},
		{"-np <A>", "Force all surfaces to be nonplanar with a given shade angle"},
		{"-onlyents", "Only update entities in the BSP"},
		{"-patchmeta", "Turn patches into triangle meshes for display"},
		{"-prtfile <filename.prt>", "Portal file to write"},
		{"-rename", "Append suffix to miscmodel shaders (needed for SoF2)"},
		{"-samplesize <N>", "Sets default lightmap resolution in luxels/qu"},
		{"-skyfix", "Turn sky box into six surfaces to work around ATI problems"},
		{"-snap <N>", "Snap brush bevel planes to the given number of units"},
		{"-srffile <filename.srf>", "Surface file to write"},
		{"-tempname <filename.map>", "Read the MAP file from the given file name"},
		{"-texrange <N>", "Limit per-surface texture range to the given number of units, and subdivide surfaces like with `q3map_tessSize` if this is not met"},
		{"-tmpout", "Write the BSP file to /tmp"},
		{"-verboseentities", "Enable `-v` only for map entities, not for the world"},
	};
	HelpOptions("BSP Stage", 0, 80, bsp, sizeof(bsp)/sizeof(struct HelpOption));
}

void HelpVis()
{
	struct HelpOption vis[] = {
		{"-vis <filename.map>", "Switch that enters this stage"},
		{"-fast", "Very fast and crude vis calculation"},
		{"-hint", "Merge all but hint portals"},
		{"-mergeportals", "The less crude half of `-merge`, makes vis sometimes much faster but doesn't hurt fps usually"},
		{"-merge", "Faster but still okay vis calculation"},
		{"-nopassage", "Just use PortalFlow vis (usually less fps)"},
		{"-nosort", "Do not sort the portals before calculating vis (usually slower)"},
		{"-passageOnly", "Just use PassageFlow vis (usually less fps)"},
		{"-prtfile <filename.prt>", "Portal file to read"},
		{"-saveprt", "Keep the Portal file after running vis (so you can run vis again)"},
		{"-tmpin", "Use /tmp folder for input"},
		{"-tmpout", "Use /tmp folder for output"},
	};
	HelpOptions("VIS Stage", 0, 80, vis, sizeof(vis)/sizeof(struct HelpOption));
}

void HelpLight()
{
	struct HelpOption light[] = {
		{"-light <filename.map>", "Switch that enters this stage"},
		{"-approx <N>", "Vertex light approximation tolerance (never use in conjunction with deluxemapping)"},
		{"-areascale <F, `-area` F>", "Scaling factor for area lights (surfacelight)"},
		{"-border", "Add a red border to lightmaps for debugging"},
		{"-bouncegrid", "Also compute radiosity on the light grid"},
		{"-bounceonly", "Only compute radiosity"},
		{"-bouncescale <F>", "Scaling factor for radiosity"},
		{"-bounce <N>", "Number of bounces for radiosity"},
		{"-bspfile <filename.bsp>", "BSP file to write"},
		{"-cheapgrid", "Use `-cheap` style lighting for radiosity"},
		{"-cheap", "Abort vertex light calculations when white is reached"},
		{"-compensate <F>", "Lightmap compensate (darkening factor applied after everything else)"},
		{"-custinfoparms", "Read scripts/custinfoparms.txt"},
		{"-dark", "Darken lightmap seams"},
		{"-debugaxis", "Color the lightmaps according to the lightmap axis"},
		{"-debugcluster", "Color the lightmaps according to the index of the cluster"},
		{"-debugdeluxe", "Show deluxemaps on the lightmap"},
		{"-debugnormals", "Color the lightmaps according to the direction of the surface normal"},
		{"-debugorigin", "Color the lightmaps according to the origin of the luxels"},
		{"-debugsurfaces, -debugsurface", "Color the lightmaps according to the index of the surface"},
		{"-debugunused", "This option does nothing"},
		{"-debug", "Mark the lightmaps according to the cluster: unmapped clusters get yellow, occluded ones get pink, flooded ones get blue overlay color, otherwise red"},
		{"-deluxemode 0", "Use modelspace deluxemaps (DarkPlaces)"},
		{"-deluxemode 1", "Use tangentspace deluxemaps"},
		{"-deluxe, -deluxemap", "Enable deluxemapping (light direction maps)"},
		{"-dirtdebug, -debugdirt", "Store the dirtmaps as lightmaps for debugging"},
		{"-dirtdepth", "Dirtmapping depth"},
		{"-dirtgain", "Dirtmapping exponent"},
		{"-dirtmode 0", "Ordered direction dirtmapping"},
		{"-dirtmode 1", "Randomized direction dirtmapping"},
		{"-dirtscale", "Dirtmapping scaling factor"},
		{"-dirty", "Enable dirtmapping"},
		{"-dump", "Dump radiosity from `-bounce` into numbered MAP file prefabs"},
		{"-export", "Export lightmaps when compile finished (like `-export` mode)"},
		{"-exposure <F>", "Lightmap exposure to better support overbright spots"},
		{"-external", "Force external lightmaps even if at size of internal lightmaps"},
		{"-extravisnudge", "Broken feature to nudge the luxel origin to a better vis cluster"},
		{"-fastallocate", "Use `-fastallocate` to trade lightmap size against allocation time (useful with hi res lightmaps on large maps: reduce allocation time from days to minutes for only some extra bytes)"},
		{"-slowbounce", "Use the slower method for calculating light spread"},
		{"-slowgrid", "Uses slower method for calculating light spread"},
		{"-slow", "Disable fast envelope/distance calculation for lights"},
		{"-faster", "Use a faster falloff curve for lighting; also implies `-fast`"},
		{"-filter", "Lightmap filtering"},
		{"-floodlight", "Enable floodlight (zero-effort somewhat decent lighting)"},
		{"-gamma <F>", "Lightmap gamma"},
		{"-gridambientscale <F>", "Scaling factor for the light grid ambient components only"},
		{"-gridscale <F>", "Scaling factor for the light grid only"},
		{"-keeplights", "Keep light entities in the BSP file after compile"},
		{"-lightmapdir <directory>", "Directory to store external lightmaps (default: same as map name without extension)"},
		{"-lightmapsearchblocksize <N>", "Restrict lightmap search to block size <N>"},
		{"-lightmapsearchpower <N>", "Optimize for lightmap merge power <N>"},
		{"-lightmapsize <N>", "Size of lightmaps to generate (must be a power of two)"},
		{"-lightsubdiv <N>", "Size of light emitting shader subdivision"},
		{"-lomem", "Low memory but slower lighting mode"},
		{"-lowquality", "Low quality floodlight (appears to currently break floodlight)"},
		{"-minsamplesize <N>", "Sets minimum lightmap resolution in luxels/qu"},
		{"-nocollapse", "Do not collapse identical lightmaps"},
		{"-nodeluxe, -nodeluxemap", "Disable deluxemapping"},
		{"-nogrid", "Disable grid light calculation (makes all entities fullbright)"},
		{"-nolightmapsearch", "Do not optimize lightmap packing for GPU memory usage (as doing so costs fps)"},
		{"-normalmap", "Color the lightmaps according to the direction of the surface normal (TODO is this identical to `-debugnormals`?)"},
		{"-nostyle, -nostyles", "Disable support for light styles"},
		{"-nosurf", "Disable tracing against surfaces (only uses BSP nodes then)"},
		{"-notrace", "Disable shadow occlusion"},
		{"-novertex", "Disable vertex lighting"},
		{"-patchshadows", "Cast shadows from patches"},
		{"-pointscale <F, `-point` F>", "Scaling factor for point lights (light entities)"},
		{"-samplescale <F>", "Scales all lightmap resolutions"},
		{"-samplesize <N>", "Sets default lightmap resolution in luxels/qu"},
		{"-samples <N>", "Adaptive supersampling quality"},
		{"-scale <F>", "Scaling factor for all light types"},
		{"-shadeangle <A>", "Angle for phong shading"},
		{"-shade", "Enable phong shading at default shade angle"},
		{"-skyscale <F, `-sky` F>", "Scaling factor for sky and sun light"},
		{"-srffile <filename.srf>", "Surface file to read"},
		{"-style, -styles", "Enable support for light styles"},
		{"-sunonly", "Only compute sun light"},
		{"-super <N, `-supersample` N>", "Ordered grid supersampling quality"},
		{"-thresh <F>", "Triangle subdivision threshold"},
		{"-trianglecheck", "Broken check that should ensure luxels apply to the right triangle"},
		{"-trisoup", "Convert brush faces to triangle soup"},
	};

	HelpOptions("Light Stage", 0, 80, light, sizeof(light)/sizeof(struct HelpOption));
}

void HelpAnalyze()
{
	struct HelpOption analyze[] = {
		{"-analyze <filename.bsp>", "Switch that enters this mode"},
		{"-lumpswap", "Swap byte order in the lumps"},
	};

	HelpOptions("Analyzing BSP-like file structure", 0, 80, analyze, sizeof(analyze)/sizeof(struct HelpOption));
}

void HelpScale()
{
	struct HelpOption scale[] = {
		{"-scale <S filename.bsp>", "Scale uniformly"},
		{"-scale <SX SY SZ filename.bsp>", "Scale non-uniformly"},
		{"-scale -tex <S filename.bsp>", "Scale uniformly without texture lock"},
		{"-scale -tex <SX SY SZ filename.bsp>", "Scale non-uniformly without texture lock"},
	};
	HelpOptions("Scaling", 0, 80, scale, sizeof(scale)/sizeof(struct HelpOption));
}

void HelpConvert()
{
	struct HelpOption convert[] = {
		{"-convert <filename.bsp>", "Switch that enters this mode"},
		{"-de <number>", "Distance epsilon for the conversion"},
		{"-format <converter>", "Select the converter (available: map, ase, or game names)"},
		{"-ne <F>", "Normal epsilon for the conversion"},
		{"-shadersasbitmap", "(only for ase) use the shader names as \\*BITMAP key so they work as prefabs"},
	};

	HelpOptions("Converting & Decompiling", 0, 80, convert, sizeof(convert)/sizeof(struct HelpOption));
}

void HelpExport()
{
	struct HelpOption exportl[] = {
		{"-export <filename.bsp>", "Copies lightmaps from the BSP to `filename/lightmap_0000.tga` ff"}
	};

	HelpOptions("Exporting lightmaps", 0, 80, exportl, sizeof(exportl)/sizeof(struct HelpOption));
}

void HelpExportEnts()
{
	struct HelpOption exportents[] = {
		{"-exportents <filename.bsp>", "Exports the entities to a text file (.ent)"},
	};
	HelpOptions("ExportEnts Stage", 0, 80, exportents, sizeof(exportents)/sizeof(struct HelpOption));
}

void HelpFixaas()
{
	struct HelpOption fixaas[] = {
		{"-fixaas <filename.bsp>", "Switch that enters this mode"},
	};

	HelpOptions("Fixing AAS checksum", 0, 80, fixaas, sizeof(fixaas)/sizeof(struct HelpOption));
}

void HelpInfo()
{
	struct HelpOption info[] = {
		{"-info <filename.bsp>", "Switch that enters this mode"},
	};

	HelpOptions("Get info about BSP file", 0, 80, info, sizeof(info)/sizeof(struct HelpOption));
}

void HelpImport()
{
	struct HelpOption import[] = {
		{"-import <filename.bsp>", "Copies lightmaps from `filename/lightmap_0000.tga` ff into the BSP"},
	};

	HelpOptions("Importing lightmaps", 0, 80, import, sizeof(import)/sizeof(struct HelpOption));
}

void HelpCommon()
{
	struct HelpOption common[] = {
		{"-game <gamename>", "Sets a different game directory name (can be used more than once)"},
		{"-connect <address>", "Talk to a WorldSpawn instance using a specific XML based protocol"},
		{"-force", "Allow reading some broken/unsupported BSP files e.g. when decompiling, may also crash"},
		{"-fs_basepath <path>", "Sets the given path as main directory of the game (can be used more than once to look in multiple paths)"},
		{"-fs_homebase <dir>", "Specifies where the user home directory name is on Linux (default for Q3A: .q3a)"},
		{"-fs_homepath <path>", "Sets the given path as home directory name"},
		{"-fs_nobasepath", "Do not load base paths in VFS, imply -fs_nomagicpath"},
		{"-fs_nomagicpath", "Do not try to guess base path magically"},
		{"-fs_nohomepath", "Do not load home path in VFS"},
		{"-fs_pakpath <path>", "Specify a package directory (can be used more than once to look in multiple paths)"},
		{"-subdivisions <F>", "multiplier for patch subdivisions quality"},
		{"-threads <N>", "number of threads to use"},
		{"-v", "Verbose mode"}
	};

	HelpOptions("Common Options", 0, 80, common, sizeof(common)/sizeof(struct HelpOption));

}

void HelpMain(const char* arg)
{
	printf("Usage: vmap [stage] [common options...] [stage options...] [stage source file]\n");
	printf("       vmap -help [stage]\n\n");

	HelpCommon();

	struct HelpOption stages[] = {
		{"-bsp", "BSP Stage"},
		{"-vis", "VIS Stage"},
		{"-light", "Light Stage"},
		{"-analyze", "Analyzing BSP-like file structure"},
		{"-scale", "Scaling"},
		{"-convert", "Converting & Decompiling"},
		{"-export", "Exporting lightmaps"},
		{"-exportents", "Exporting entities"},
		{"-fixaas", "Fixing AAS checksum"},
		{"-info", "Get info about BSP file"},
		{"-import", "Importing lightmaps"},
	};
	void (*help_funcs[])() = {
		HelpBsp,
		HelpVis,
		HelpLight,
		HelpAnalyze,
		HelpScale,
		HelpConvert,
		HelpExport,
		HelpExportEnts,
		HelpFixaas,
		HelpInfo,
		HelpImport,
	};

	if ( arg && strlen(arg) > 0 )
	{
		if ( arg[0] == '-' )
			arg++;

		unsigned i;
		for ( i = 0; i < sizeof(stages)/sizeof(struct HelpOption); i++ )
			if ( strcmp(arg, stages[i].name+1) == 0 )
			{
				help_funcs[i]();
				return;
			}
	}

	HelpOptions("Stages", 0, 80, stages, sizeof(stages)/sizeof(struct HelpOption));
}
