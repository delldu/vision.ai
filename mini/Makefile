#/************************************************************************************
#***
#***	Copyright 2020 Dell(18588220928@163.com), All Rights Reserved.
#***
#***	File Author: Dell, Sun Feb  5 20:51:04 CST 2020
#***
#************************************************************************************/
#

LIB_NAME := libimage
INSTALL_DIR := /usr/local

INCS	:= 

SOURCE :=  \
	image.c

DEFINES := 
CFLAGS := -O2 -fPIC -Wall -Wextra
LDFLAGS := -fPIC -ljpeg -lpng
 

#****************************************************************************
# Makefile code common to all platforms
#****************************************************************************
CFLAGS   := ${CFLAGS} ${DEFINES}
CXXFLAGS := ${CXXFLAGS} ${DEFINES}
OBJECTS := $(addsuffix .o,$(basename ${SOURCE}))

#****************************************************************************
# Compile block
#****************************************************************************
all: staticlib
# sharelib

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

install: staticlib
	sudo mkdir -p ${INSTALL_DIR}/include
	sudo cp image.h ${INSTALL_DIR}/include

	sudo mkdir -p ${INSTALL_DIR}/lib
	# sudo cp ${LIB_NAME}.so ${INSTALL_DIR}/lib
	sudo cp ${LIB_NAME}.a ${INSTALL_DIR}/lib


