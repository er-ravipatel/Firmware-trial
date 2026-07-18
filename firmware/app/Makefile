#
# Makefile — Lumen Frame firmware app (out-of-tree Circle build).
#
# Circle lives under firmware/vendor/circle. Build the Circle libraries first:
#   (cd ../vendor/circle && ./configure -r 3 -p aarch64-linux-gnu- --qemu -f && ./makeall -j)
# then here:
#   make            -> produces kernel8.img
#

CIRCLEHOME = ../vendor/circle

# Our shared, freestanding source modules live under firmware/src; stb under vendor/stb.
EXTRAINCLUDE = -I ../src -I ../vendor/stb

OBJS = main.o kernel.o \
       ../src/plugins/PhotoFramePlugin.o \
       ../src/plugins/ClockPlugin.o \
       ../src/content/JpegDecoder.o \
       ../src/content/stb_image_impl.o

LIBS = $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
