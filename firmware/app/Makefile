#
# Makefile — Lumen Frame firmware app (out-of-tree Circle build).
#
# Circle lives under firmware/vendor/circle. Build the Circle libraries first:
#   (cd ../vendor/circle && ./configure -r 3 -p aarch64-linux-gnu- --qemu -f && ./makeall -j)
# then here:
#   make            -> produces kernel8.img
#

CIRCLEHOME = ../vendor/circle

OBJS = main.o kernel.o

LIBS = $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
