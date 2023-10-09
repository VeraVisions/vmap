# vmap Makefile 

# ws libs vmap uses
LIBOBJS=../libs/libddslib.a \
	../libs/libetclib.a \
	../libs/libfilematch.a \
	../libs/libl_net.a \
	../libs/libmathlib.a \
	../libs/libpicomodel.a

GLIB_CFLAGS=$(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS=$(shell pkg-config --libs glib-2.0)

XML_CFLAGS=$(shell pkg-config --cflags libxml-2.0)
XML_LDFLAGS=$(shell pkg-config --libs libxml-2.0)

MINIZIP_CFLAGS=$(shell pkg-config --cflags minizip)
MINIZIP_LDFLAGS=$(shell pkg-config --libs minizip)

JPEG_CFLAGS=$(shell pkg-config --cflags libjpeg)
JPEG_LDFLAGS=$(shell pkg-config --libs libjpeg)

PNG_CFLAGS=$(shell pkg-config --cflags libpng)
PNG_LDFLAGS=$(shell pkg-config --libs libpng)

VMAP_CFLAGS=$(CFLAGS) $(GLIB_CFLAGS) $(XML_CFLAGS) $(MINIZIP_CFLAGS) $(JPEG_CFLAGS) $(PNG_CFLAGS) -I../include  -I../common -I../libs
VMAP_LDFLAGS=$(LDFLAGS) -lm -lpthread -L../lib $(GLIB_LDFLAGS) $(XML_LDFLAGS) $(MINIZIP_LDFLAGS) $(JPEG_LDFLAGS) $(PNG_LDFLAGS)

DO_CC=$(CC) $(VMAP_CFLAGS) -o $@ -c $<

.c.o:
	$(DO_CC)

VMAP_OBJS = \
	../common/cmdlib.o \
	../common/imagelib.o \
	../common/inout.o \
	../common/jpeg.o \
	../common/md4.o \
	../common/mutex.o \
	../common/polylib.o \
	../common/scriplib.o \
	../common/matlib.o \
	../common/threads.o \
	../common/vfs.o \
	brush.o \
	brush_primit.o \
	bsp.o \
	bsp_analyze.o \
	bsp_info.o \
	bsp_scale.o \
	bspfile_abstract.o \
	bspfile_ibsp.o \
	bspfile_rbsp.o \
	convert_ase.o \
	convert_bsp.o \
	convert_map.o \
	convert_obj.o \
	decals.o \
	exportents.o \
	facebsp.o \
	fixaas.o \
	fog.o \
	help.o \
	image.o \
	leakfile.o \
	light.o \
	light_bounce.o \
	light_trace.o \
	light_ydnar.o \
	lightmaps_ydnar.o \
	main.o \
	map.o \
	mesh.o \
	model.o \
	patch.o \
	path_init.o \
	portals.o \
	prtfile.o \
	shaders.o \
	surface.o \
	surface_extra.o \
	surface_foliage.o \
	surface_fur.o \
	surface_meta.o \
	tjunction.o \
	tree.o \
	vis.o \
	visflow.o \
	writebsp.o

# binary target
../vmap: $(VMAP_OBJS)
	$(CXX) -o $@ $(VMAP_OBJS) $(LIBOBJS) $(VMAP_LDFLAGS)

clean:
	-rm -f ./../common/*.o
	-rm -f ./*.o
	-rm -f ../vmap

# object files
../common/cmdlib.o: ../common/cmdlib.c ../common/cmdlib.h
../common/imagelib.o: ../common/imagelib.c ../common/imagelib.h
../common/inout.o: ../common/inout.c ../common/inout.h
../common/jpeg.o: ../common/jpeg.c
../common/md4.o: ../common/md4.c ../common/md4.h
../common/mutex.o: ../common/mutex.c ../common/mutex.h
../common/polylib.o: ../common/polylib.c ../common/polylib.h
../common/scriplib.o: ../common/scriplib.c ../common/scriplib.h
../common/matlib.o: ../common/matlib.c ../common/matlib.h
../common/threads.o: ../common/threads.c
../common/vfs.o: ../common/vfs.c ../common/vfs.h
brush.o: brush.c 
brush_primit.o: brush_primit.c
bsp.o: bsp.c
bsp_analyze.o: bsp_analyze.c
bsp_info.o: bsp_info.c
bsp_scale.o: bsp_scale.c
bspfile_abstract.o: bspfile_abstract.c
bspfile_ibsp.o: bspfile_ibsp.c
bspfile_rbsp.o: bspfile_rbsp.c
convert_ase.o: convert_ase.c
convert_bsp.o: convert_bsp.c
convert_map.o: convert_map.c
convert_obj.o: convert_obj.c
decals.o: decals.c
exportents.o: exportents.c
facebsp.o: facebsp.c
fixaas.o: fixaas.c
fog.o: fog.c
help.o: help.c
image.o: image.c
leakfile.o: leakfile.c
light.o: light.c
light_bounce.o: light_bounce.c
light_trace.o: light_trace.c
light_ydnar.o: light_ydnar.c
lightmaps_ydnar.o: lightmaps_ydnar.c
main.o: main.c
map.o: map.c
mesh.o: mesh.c
model.o: model.c
patch.o: patch.c
path_init.o: path_init.c
portals.o: portals.c
prtfile.o: prtfile.c
shaders.o: shaders.c
surface.o: surface.c
surface_extra.o: surface_extra.c
surface_foliage.o: surface_foliage.c
surface_fur.o: surface_fur.c
surface_meta.o: surface_meta.c
tjunction.o: tjunction.c
tree.o: tree.c
vis.o: vis.c
visflow.o: visflow.c
writebsp.o: writebsp.c
