# MIT License
# 
# Copyright (c) 2023 Brandon McGriff <nightmareci@gmail.com>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.16)
project(skill-clear
	VERSION 0.0.0
	LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}")
set(BIN "${CMAKE_CURRENT_BINARY_DIR}")

include("${SRC}/src/cmake/InstallRuntimeDependencies.cmake")

set(ORG "nightmareci")
set(EXE "${CMAKE_PROJECT_NAME}")
set(NAME "Skill Clear")

option(SHOW_RELEASE_VERSION "If TRUE, the build is a finalized release version, and the game will show the release version. If FALSE, the text \"Prerelease <UTC build timestamp>\" will be shown instead")

string(TIMESTAMP CONFIGURE_TIME UTC)

if(SHOW_RELEASE_VERSION)
	set(VERSION "v${CMAKE_PROJECT_VERSION}")
else()
	set(VERSION "Prerelease ${CONFIGURE_TIME}")
endif()

set(BUILD_TARGET "${CMAKE_SYSTEM_NAME}" CACHE STRING "The build target. By default, the current platform is the build target")
message(STATUS "Build target: ${BUILD_TARGET}")

configure_file("${SRC}/src/main/app_const.c.in" "${BIN}/src/main/app_const.c" @ONLY)
add_executable("${EXE}"
	# Sources for the game follow. All game sources should be under the
	# src/game directory, not in any other directories in src/.

	"${SRC}/src/game/game.c"

	"${SRC}/src/game/game.h"


	# Sources for the engine follow.


	"${SRC}/src/main/app.h"

	"${SRC}/src/main/app.c"
	"${BIN}/src/main/app_const.c"
	"${SRC}/src/main/main.c"


	"${SRC}/src/render/frames.h"
	"${SRC}/src/render/print.h"
	"${SRC}/src/render/render.h"

	"${SRC}/src/render/frames.c"
	"${SRC}/src/render/print.c"
	"${SRC}/src/render/render.c"


	"${SRC}/src/util/defs.h"
	"${SRC}/src/util/dict.h"
	"${SRC}/src/util/font.h"
	"${SRC}/src/util/ini.h"
	"${SRC}/src/util/linear.c"
	"${SRC}/src/util/nanotime.h"
	"${SRC}/src/util/queue.h"
	"${SRC}/src/util/string_util.h"

	"${SRC}/src/util/dict.c"
	"${SRC}/src/util/font.c"
	"${SRC}/src/util/ini.c"
	"${SRC}/src/util/linear.h"
	"${SRC}/src/util/nanotime.c"
	"${SRC}/src/util/queue.c"
	"${SRC}/src/util/string_util.c"


	"${SRC}/src/file/file.h"
	"${SRC}/src/file/file_blob.h"
	"${SRC}/src/file/file_chunk.h"
	"${SRC}/src/file/file_font.h"
	"${SRC}/src/file/file_music.h"
	"${SRC}/src/file/file_surface.h"
	"${SRC}/src/file/file_type_manager.h"

	"${SRC}/src/file/file.c"
	"${SRC}/src/file/file_blob.c"
	"${SRC}/src/file/file_chunk.c"
	"${SRC}/src/file/file_font.c"
	"${SRC}/src/file/file_music.c"
	"${SRC}/src/file/file_surface.c"


	# Non-opengl sources should only ever include opengl.h; the other
	# sources are just internal implementation details to how OpenGL is
	# made available to the code. Different platforms could use different
	# internal means to provide OpenGL, with the opengl.h header providing
	# a standard way to access OpenGL on all platforms.


	"${SRC}/src/opengl/glad.h"
	"${SRC}/src/opengl/khrplatform.h"
	"${SRC}/src/opengl/opengl.h"

	"${SRC}/src/opengl/glad.c"
	"${SRC}/src/opengl/opengl.c"


	# A specific version of Lua is bundled directly into the repository, to
	# ensure it's the exact version the code expects. As of last editing
	# this file, the CMake FindLua module doesn't provide any means to
	# request a specific version of Lua to use.


	"${SRC}/src/lua/lapi.h"
	"${SRC}/src/lua/lauxlib.h"
	"${SRC}/src/lua/lcode.h"
	"${SRC}/src/lua/lctype.h"
	"${SRC}/src/lua/ldebug.h"
	"${SRC}/src/lua/ldo.h"
	"${SRC}/src/lua/lfunc.h"
	"${SRC}/src/lua/lgc.h"
	"${SRC}/src/lua/ljumptab.h"
	"${SRC}/src/lua/llex.h"
	"${SRC}/src/lua/llimits.h"
	"${SRC}/src/lua/lmem.h"
	"${SRC}/src/lua/lobject.h"
	"${SRC}/src/lua/lopcodes.h"
	"${SRC}/src/lua/lopnames.h"
	"${SRC}/src/lua/lparser.h"
	"${SRC}/src/lua/lprefix.h"
	"${SRC}/src/lua/lstate.h"
	"${SRC}/src/lua/lstring.h"
	"${SRC}/src/lua/ltable.h"
	"${SRC}/src/lua/ltm.h"
	"${SRC}/src/lua/luaconf.h"
	"${SRC}/src/lua/lua.h"
	"${SRC}/src/lua/lualib.h"
	"${SRC}/src/lua/lundump.h"
	"${SRC}/src/lua/lvm.h"
	"${SRC}/src/lua/lzio.h"

	"${SRC}/src/lua/lapi.c"
	"${SRC}/src/lua/lauxlib.c"
	"${SRC}/src/lua/lbaselib.c"
	"${SRC}/src/lua/lcode.c"
	"${SRC}/src/lua/lcorolib.c"
	"${SRC}/src/lua/lctype.c"
	"${SRC}/src/lua/ldblib.c"
	"${SRC}/src/lua/ldebug.c"
	"${SRC}/src/lua/ldo.c"
	"${SRC}/src/lua/ldump.c"
	"${SRC}/src/lua/lfunc.c"
	"${SRC}/src/lua/lgc.c"
	"${SRC}/src/lua/linit.c"
	"${SRC}/src/lua/liolib.c"
	"${SRC}/src/lua/llex.c"
	"${SRC}/src/lua/lmathlib.c"
	"${SRC}/src/lua/lmem.c"
	"${SRC}/src/lua/loadlib.c"
	"${SRC}/src/lua/lobject.c"
	"${SRC}/src/lua/lopcodes.c"
	"${SRC}/src/lua/loslib.c"
	"${SRC}/src/lua/lparser.c"
	"${SRC}/src/lua/lstate.c"
	"${SRC}/src/lua/lstring.c"
	"${SRC}/src/lua/lstrlib.c"
	"${SRC}/src/lua/ltable.c"
	"${SRC}/src/lua/ltablib.c"
	"${SRC}/src/lua/ltm.c"
	"${SRC}/src/lua/lundump.c"
	"${SRC}/src/lua/lutf8lib.c"
	"${SRC}/src/lua/lvm.c"
	"${SRC}/src/lua/lzio.c"
)

target_include_directories("${EXE}" PRIVATE "${SRC}/src" "${BIN}/src")

# TODO: Add these as Git submodules and include them in the build, but still
# have the option to use system-installed libraries.
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)

target_link_libraries("${EXE}"
	PRIVATE
		SDL2::SDL2
		SDL2_image::SDL2_image
		SDL2_mixer::SDL2_mixer
)

# SDL_main isn't used on all platforms.
if(TARGET SDL2::SDL2main)
	target_link_libraries("${EXE}"
		PRIVATE
			SDL2::SDL2main
	)
endif()

if(UNIX)
	find_library(MATH_LIBRARY m)
	if(NOT MATH_LIBRARY STREQUAL MATH_LIBRARY-NOTFOUND)
		target_link_libraries("${EXE}"
			PRIVATE
				${MATH_LIBRARY}
		)
	endif()

	# With Lua, compile-time warnings show up if POSIX features aren't used
	# on POSIX platforms.
	add_compile_definitions(LUA_USE_POSIX)
	add_compile_definitions(_POSIX_VERSION=199309L)
endif()

option(BUNDLE_LIBRARIES "Bundle the libraries needed by the game's executable, for distributable releases")

if(BUILD_TARGET STREQUAL Windows AND MSVC AND NOT MINGW)
	add_compile_definitions(BUILD_TARGET_WINDOWS_MSVC)

	option(HIDE_WINDOWS_CONSOLE "Hide the Windows console")

	target_link_options("${EXE}"
		PRIVATE
			$<$<BOOL:${HIDE_WINDOWS_CONSOLE}>:
				/ENTRY:mainCRTStartup
				/SUBSYSTEM:WINDOWS
			>
	)

	install(TARGETS "${EXE}" DESTINATION ".")
	if(BUNDLE_LIBRARIES)
		install_runtime_dependencies("${EXE}")
	endif()

	install(FILES "${SRC}/copyright.txt" DESTINATION ".")

	install(FILES "${SRC}/asset/font.fnt" DESTINATION "asset")
	install(FILES "${SRC}/asset/font_0.png" DESTINATION "asset")
elseif(BUILD_TARGET STREQUAL Darwin)
	# TODO: Support installation for macOS.
elseif(BUILD_TARGET STREQUAL Linux)
	# TODO: Support installation for Linux.
endif()

set(CPACK_PACKAGE_NAME "${NAME}")
set(CPACK_PACKAGE_VENDOR "${ORG}")
if(VCPKG_TARGET_TRIPLET)
	set(CPACK_PACKAGE_FILE_NAME "${NAME} ${VCPKG_TARGET_TRIPLET}")
elseif("${BUILD_TARGET}" STREQUAL Darwin)
	set(CPACK_PACKAGE_FILE_NAME "${NAME} macOS")
elseif(APPLE OR NOT CMAKE_SYSTEM_PROCESSOR)
	set(CPACK_PACKAGE_FILE_NAME "${NAME} ${BUILD_TARGET}")
else()
	set(CPACK_PACKAGE_FILE_NAME "${NAME} ${BUILD_TARGET} ${CMAKE_SYSTEM_PROCESSOR}")
endif()
include(CPack)
