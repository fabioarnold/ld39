#!/bin/bash

# detect platform
OS_NAME="$(uname)" # {Darwin, Linux}
MACHINE_NAME="$(uname -m)" # {x86_64, armv7l}
PLATFORM="unknown"
# TODO: find more reliable way to determine if it's a Pandora
if [[ $OS_NAME == "Linux" && $MACHINE_NAME == "armv7l" ]]; then
	PLATFORM="OpenPandora"
else
	PLATFORM=$OS_NAME
fi

TARGET="ludumdare"

if [[ $1 = "clean" ]]; then
	rm -r build
	exit 0
fi

CWARN="-Wall -Wextra -Wattributes -Wbuiltin-macro-redefined -Wcast-align -Wconversion -Wdiv-by-zero -Wdouble-promotion -Wenum-compare -Wfloat-equal -Winit-self -Wint-to-pointer-cast -Wmissing-braces -Wmissing-field-initializers       -Woverflow -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wtype-limits -Wuninitialized -Wwrite-strings -Wno-unused-parameter -Wno-unused-variable -Wno-multichar -Wdeclaration-after-statement -Wimplicit-int -Wpointer-sign -Wpointer-to-int-cast"
CXXWARN="-Wall -Wextra -Wattributes -Wbuiltin-macro-redefined -Wcast-align -Wconversion -Wdiv-by-zero -Wdouble-promotion -Wenum-compare -Wfloat-equal -Winit-self -Wint-to-pointer-cast -Wmissing-braces -Wmissing-field-initializers       -Woverflow -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wtype-limits -Wuninitialized -Wwrite-strings -Wno-unused-parameter -Wno-unused-variable -Wno-multichar -Wc++0x-compat -Wsign-promo"

CFLAGS="$CFLAGS -std=c++11"
DEBUG_FLAGS="$CXXWARN -O0 -g -DDEBUG"
RELEASE_FLAGS="-Os"
if [[ $1 = "release" ]]; then
	CFLAGS="$CFLAGS $RELEASE_FLAGS"
else
	CFLAGS="$CFLAGS $DEBUG_FLAGS"
fi
LDFLAGS="-Lbuild"

# gamelib
INCLUDE_DIRS="-Ilib/gamelib/src"

# zlib
CFLAGS="$CFLAGS -DUSE_ZLIB"
LIB_Z="-lz"

# SDL2
LIB_SDL2="`pkg-config --libs sdl2`"

# OpenGL and Windowing stuff
LIB_OPENGL="-lGL"

# fontstash
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/fontstash"
LIB_FONTSTASH="-lfontstash"
if [ ! -f build/libfontstash.a ]; then
	echo "building fontstash..."
	./build_fontstash.sh
	if [ $? = 1 ]; then
		exit 1
	fi
fi

# dear imgui
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/imgui -Ilib/gamelib/third_party/imgui"
LIB_IMGUI="-lImGui"
if [ ! -f build/libImGui.a ]; then
	echo "building dear imgui..."
	./build_imgui.sh
	if [ $? = 1 ]; then
		exit 1
	fi
fi

# platform specific changes
if [[ $PLATFORM == "Darwin" ]]; then
	LIB_OPENGL="-framework OpenGL -framework Cocoa"
elif [[ $PLATFORM == "Linux" ]]; then
	CFLAGS="$CFLAGS -DUSE_GLEW"
	LIB_OPENGL="$LIB_OPENGL -lGLEW"
elif [[ $PLATFORM == "OpenPandora" ]]; then
	CFLAGS="$CFLAGS `pkg-config --cflags glesv1_cm glesv2` -DUSE_OPENGLES"
	LIB_OPENGL="`pkg-config --libs glesv2`"
	source pandora_setup.sh
fi

# final compiler flags
CFLAGS="$CFLAGS `pkg-config --cflags sdl2` $INCLUDE_DIRS"
LDFLAGS="$LDFLAGS $LIB_Z $LIB_SDL2 $LIB_OPENGL $LIB_FONTSTASH $LIB_IMGUI"

mkdir -p build
if [[ $PLATFORM != "OpenPandora" ]]; then
	python compile_assets.py assets build/data desktop
fi
echo "compiling..."
c++ $CFLAGS src/main_sdl2.cpp $LDFLAGS -o build/$TARGET
EXIT_STATUS=$?
if [[ $EXIT_STATUS == 0 ]]; then
	if [[ $1 == "run" ]]; then
		cd build
		./$TARGET
		cd ..
	fi
else
	exit $EXIT_STATUS
fi
