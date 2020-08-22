#/************************************************************************************
#***
#***	Copyright 2017 Dell(18588220928@163.com), All Rights Reserved.
#***
#***	File Author: Dell, Sun Feb  5 20:51:04 CST 2017
#***
#************************************************************************************/
#

LIB_NAME := libvision
INSTALL_DIR := /tmp/local/
#/usr/local/

INCS	:= \
	-Iinclude \
	-I/usr/local/include

SOURCE :=  \
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
	source/texture.c \
	source/video.c \
	source/blend.c \
	source/camera.c \
	source/filter.c \
	source/seam.c \
	source/histogram.c \
	source/retinex.c \
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
all: sharelib staticlib

sharelib: $(OBJECTS)
	$(LD) $(LDFLAGS) -shared -soname $(LIB_NAME).so -o $(LIB_NAME).so $(OBJECTS)


staticlib:$(OBJECTS)
	$(AR) $(ARFLAGS) $(LIB_NAME).a $(OBJECTS)


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
	sudo mkdir -p ${INSTALL_DIR}/include/vision
	sudo cp include/*.h ${INSTALL_DIR}/include/vision 
	sudo cp ${LIB_NAME}.so ${INSTALL_DIR}/lib
	sudo cp ${LIB_NAME}.a ${INSTALL_DIR}/lib


