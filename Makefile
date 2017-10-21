#/************************************************************************************
#***
#***	Copyright 2017 Dell(dellrunning@gmail.com), All Rights Reserved.
#***
#***	File Author: Dell, Sun Feb  5 20:51:04 CST 2017
#***
#************************************************************************************/
#

TORCH7_INSTALL_DIR=$(shell dirname ${DYLD_LIBRARY_PATH})

LIB_NAME := libvision

INCS	:= \
	-Iinclude \
	-I$(TORCH7_INSTALL_DIR)/include \
	-I/usr/local/include

	# -I$(BUILD_DIR)/include
SOURCE :=  \
	vision.c \
	limage.c \
	lvideo.c \
	source/color.c \
	source/frame.c \
	source/hough.c \
	source/matrix.c \
	source/shape.c \
	source/text.c \
	source/vector.c \
	source/common.c  \
	source/image.c \
	source/motion.c \
	source/sift.c \
	source/texture.c \
	source/video.c \
	source/blend.c \
	source/camera.c \
	source/filter.c \
	source/seam.c \
	source/histogram.c \
	source/retinex.c \
	source/sview.c \
	source/plane.c \
	source/phash.c

#	source/genes.c 

DEFINES := 
CFLAGS := -O2 -fPIC -Wall -Wextra
LDFLAGS := -fPIC \
	-ljpeg -lpng
 


#****************************************************************************
# Makefile code common to all platforms
#****************************************************************************
CFLAGS   := ${CFLAGS} ${DEFINES}
CXXFLAGS := ${CXXFLAGS} ${DEFINES}
OBJECTS := $(addsuffix .o,$(basename ${SOURCE}))

#****************************************************************************
# Compile block
#****************************************************************************
all: sharelib
# sharelib
# staticlib

sharelib: $(OBJECTS)
	$(LD) $(LDFLAGS) -shared -soname $(LIB_NAME).so -o $(LIB_NAME).so $(OBJECTS)
	# cp $(LIB_NAME).so ${INSTALL_DIR}/lib
	# mv $(LIB_NAME).so ../lib


staticlib:$(OBJECTS)
	$(AR) $(ARFLAGS) $(LIB_NAME).a $(OBJECTS)
	# cp $(LIB_NAME).a ${INSTALL_DIR}/lib
	# mv $(LIB_NAME).a ../lib


#****************************************************************************
# Depend block
#****************************************************************************
depend:

#****************************************************************************
# common rules
#****************************************************************************
%.o : %.cpp
	${CXX} ${CXXFLAGS} ${INCS} -c $< -o $@

%.o : %.c
	${CC} ${CFLAGS} ${INCS} -c $< -o $@


clean:
	rm -rf *.a *.so *.o $(OBJECTS)

install:
	cp ${LIB_NAME}.so $(TORCH7_INSTALL_DIR)/lib

