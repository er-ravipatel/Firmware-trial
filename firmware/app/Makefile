#
# Makefile — Lumen Frame firmware app (out-of-tree Circle build).
#
# Circle lives under firmware/vendor/circle. Build the Circle libraries first:
#   (cd ../vendor/circle && ./configure -r 3 -p aarch64-linux-gnu- --qemu -f && ./makeall -j)
# then here:
#   make            -> produces kernel8.img
#

CIRCLEHOME = ../vendor/circle

# Our shared, freestanding source modules live under firmware/src; stb + qrcodegen under vendor/.
# -I ../app lets shared modules (e.g. the web server) reach app-layer headers like Config.h.
EXTRAINCLUDE = -I ../src -I ../vendor/stb -I ../vendor/qrcodegen -I ../app

OBJS = main.o kernel.o Config.o \
       ../src/plugins/PhotoFramePlugin.o \
       ../src/content/JpegDecoder.o \
       ../src/content/ExifReader.o \
       ../src/content/stb_image_impl.o \
       ../src/net/dhcpd.o \
       ../src/net/dnsd.o \
       ../src/net/webserver.o \
       ../src/util/libc_compat.o \
       ../vendor/qrcodegen/qrcodegen.o

LIBS = $(CIRCLEHOME)/addon/wlan/libwlan.a \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/net/libnet.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
