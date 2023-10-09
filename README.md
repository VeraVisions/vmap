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
* gcc-c++
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