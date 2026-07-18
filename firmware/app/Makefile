#
# Makefile — Lumen Frame firmware app (out-of-tree Circle build).
#
# Circle lives under firmware/vendor/circle. Build the Circle libraries first:
#   (cd ../vendor/circle && ./configure -r 3 -p aarch64-linux-gnu- --qemu -f && ./makeall -j)
# then here:
#   make            -> produces kernel8.img
#

CIRCLEHOME = ../vendor/circle

# Our shared, freestanding source modules live under firmware/src.
EXTRAINCLUDE = -I ../src

OBJS = main.o kernel.o \
       ../src/plugins/PhotoFramePlugin.o \
       ../src/plugins/ClockPlugin.o

LIBS = $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
