# vmap

A fork of q3map2, now available as a stand-alone compiler targetting [FTEQW](https://www.fteqw.org/)


## Compiler Changes
- Improved High-Dynamic-Range lightmaps
- Support for our patchDef2WS and patchDef3WS curved surfaces in the BSP compiler, allowing for 4-way texture blended patches.
- Reads individual material scripts (.mat) instead of large .shader files
- Surfaces are aware which env_cubemap ents they belong to
- light_surface entity support, so you don't have to write map specific materials to override texture light properties
- Handles Half-Life styled point lights, including zhlt_lightflags
- Handles Half-Life styled light_environment entities
- New material keys: vmap_lightLinear, vmap_lightLinearFade
- Support for target-less spotlights
- Explicit support for func_detail, func_detail_illusionary
- Support for misc_prefab (including other .map files)
- vmap_remapMaterial/q3map_remapShader can carry over surface flags now
- Support for entity key: _entsurfaceflags, so surfaces can override their surfaceflags
- Support for entity key: _entcontentflags, so brushes can override their contentflags

## Usage

BSP: `./vmap [options] dm_foobar`  
VIS:  `./vmap [options] -vis dm_foobar`  
LIGHT: `./vmap [options] -light dm_foobar`

You want to pass `-basedir /path/to/game/root` and `-game` arguments to specify where to read textures from. If you wanted to, for example, compile a map included within Nuclide's 'base' directory you'd end up with a command-line like this:

`./vmap -basedir "/home/user/nuclide-sdk/" -game "base" test_sun`

### Material definitions

As stated, we look alongside the textures for material definitions.
For example, if a .map file references `measure/floor` the compiler will look
at `textures/measure/floor.mat` within any `-game` directories for the exact material description.

The .mat file in question looks something like this:

```
{
	program lightmapped
	diffusemap "textures/measure/floor.tga"
}
```

The `program` line tells the engine which SPIR-V/GLSL/HLSL pixel shader program to use, the compiler ignores that. The `diffusemap` key is read in by the compiler to figure out which texture to load for its color information (used by the `-light` switch) and its size.

You can also use Q3A style .shader syntax in these, but set at least a `qer_editorImage` pointing to a valid texture for valid dimensions.

[There's material directives specific to vmap, which you can see here.](https://developer.vera-visions.com/d6/d06/mat_vmap.html). Support for the q3map2 equivalents (where applicable) has been preserved, so you won't need to migrate.

## Compiling
To compile on a standard GNU/Linux system:
`make`

On BSD you should probably use GNU make right now.
Clang should also be supported, pass `CC=clang` if you want to use it.

On NT you'll have to jump through a lot more hoops, here's the gist:

1. MSYS2: https://www.msys2.org/
2. in the msys2 shell, enter `pacman -S --needed base-devel git unzip mingw-w64-$(uname -m)-{toolchain,make,minizip-git}`
3. boot into the Mingw64 shell, don't use the stock MSYS2 shell
4. run make and it should build everything, in theory

**Please don't contact us about helping you build it on Windows. This is a development tool. This is provided AS-IS.**

## Dependencies
* GNU make
* gcc-core
* glib2-devel
* libxml2-devel
* libjpeg8-devel
* libpng-devel
* minizip-devel

## Support
**As mentioned before, if you need help with this: you're on your own.**

## Special Thanks

The original q3map/2 developers: 
- id Software
- Splash Damage
- ydnar
- GtkRadiant team and contributors
- NetRadiant team and contributors

vmap developers:
- Vera Visions, L.L.C.
- Spike
- Joshua Ashton
- Slartibarty