#!/bin/bash

TARGET="ludumdare39.html"
BUILD_DIR=embuild

if [[ $1 = "clean" ]]; then
	rm -r $BUILD_DIR
	exit 0
fi

CWARN="-Wall -Wextra -Wattributes -Wbuiltin-macro-redefined -Wcast-align -Wconversion -Wdiv-by-zero -Wdouble-promotion -Wenum-compare -Wfloat-equal -Winit-self -Wint-to-pointer-cast -Wmissing-braces -Wmissing-field-initializers       -Woverflow -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wtype-limits -Wuninitialized -Wwrite-strings -Wno-unused-parameter -Wno-unused-variable -Wno-multichar -Wdeclaration-after-statement -Wimplicit-int -Wpointer-sign -Wpointer-to-int-cast"
CXXWARN="-Wall -Wextra -Wattributes -Wbuiltin-macro-redefined -Wcast-align -Wconversion -Wdiv-by-zero -Wdouble-promotion -Wenum-compare -Wfloat-equal -Winit-self -Wint-to-pointer-cast -Wmissing-braces -Wmissing-field-initializers       -Woverflow -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wsign-compare -Wtype-limits -Wuninitialized -Wwrite-strings -Wno-unused-parameter -Wno-unused-variable -Wno-multichar -Wc++0x-compat -Wsign-promo"

CFLAGS="$CFLAGS -std=c++11"
DEBUG_FLAGS="$CXXWARN -O0 -g -DDEBUG"
RELEASE_FLAGS="-O2"

CFLAGS="$CFLAGS $RELEASE_FLAGS"

# gamelib
INCLUDE_DIRS="-Ilib/gamelib/src"

# fontstash
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/fontstash"
LIB_FONTSTASH="lib/gamelib/third_party/fontstash/fontstash.cpp"

# dear imgui
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/imgui -Ilib/gamelib/third_party/imgui"
LIB_IMGUI="lib/gamelib/third_party/imgui/imgui_sdl2_gl2_ub.cpp"

# final compiler flags
CFLAGS="$CFLAGS $INCLUDE_DIRS"
LDFLAGS="$LDFLAGS $LIB_FONTSTASH $LIB_IMGUI"

EMSCRIPTEN_FLAGS="-s USE_SDL=2 -s TOTAL_MEMORY=268435456"

mkdir -p $BUILD_DIR
python compile_assets.py assets $BUILD_DIR/data desktop
echo "compiling..."
em++ $EMSCRIPTEN_FLAGS $CFLAGS src/main_sdl2.cpp $LDFLAGS -o $BUILD_DIR/$TARGET --preload-file $BUILD_DIR/data@data
EXIT_STATUS=$?
if [[ $EXIT_STATUS == 0 ]]; then
	if [[ $1 == "run" ]]; then
		cd $BUILD_DIR
		xdg-open $TARGET
		cd ..
	fi
else
	exit $EXIT_STATUS
fi
