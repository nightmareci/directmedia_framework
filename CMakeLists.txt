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

configure_file("${SRC}/src/framework/app_const.c.in" "${BIN}/src/framework/app_const.c" @ONLY)
add_executable("${EXE}"
	"${SRC}/src/framework/app.c"
	"${BIN}/src/framework/app_const.c"
	"${SRC}/src/framework/frames.c"
	"${SRC}/src/framework/dict.c"
	"${SRC}/src/framework/file.c"
	"${SRC}/src/framework/file_blob.c"
	"${SRC}/src/framework/file_chunk.c"
	"${SRC}/src/framework/file_font.c"
	"${SRC}/src/framework/file_music.c"
	"${SRC}/src/framework/file_surface.c"
	"${SRC}/src/framework/font.c"
	"${SRC}/src/framework/glad.c"
	"${SRC}/src/framework/ini.c"
	"${SRC}/src/framework/linear.c"
	"${SRC}/src/framework/main.c"
	"${SRC}/src/framework/nanotime.c"
	"${SRC}/src/framework/opengl_util.c"
	"${SRC}/src/framework/print.c"
	"${SRC}/src/framework/queue.c"
	"${SRC}/src/framework/render.c"
	"${SRC}/src/framework/string_util.c"
	"${SRC}/src/framework/app.h"
	"${SRC}/src/framework/frames.h"
	"${SRC}/src/framework/defs.h"
	"${SRC}/src/framework/dict.h"
	"${SRC}/src/framework/file_blob.h"
	"${SRC}/src/framework/file_chunk.h"
	"${SRC}/src/framework/file_font.h"
	"${SRC}/src/framework/file.h"
	"${SRC}/src/framework/file_music.h"
	"${SRC}/src/framework/file_surface.h"
	"${SRC}/src/framework/file_type_manager.h"
	"${SRC}/src/framework/font.h"
	"${SRC}/src/framework/glad.h"
	"${SRC}/src/framework/ini.h"
	"${SRC}/src/framework/khrplatform.h"
	"${SRC}/src/framework/linear.h"
	"${SRC}/src/framework/nanotime.h"
	"${SRC}/src/framework/opengl_util.h"
	"${SRC}/src/framework/print.h"
	"${SRC}/src/framework/queue.h"
	"${SRC}/src/framework/render.h"
	"${SRC}/src/framework/string_util.h"

	"${SRC}/src/framework/lua/lapi.c"
	"${SRC}/src/framework/lua/lauxlib.c"
	"${SRC}/src/framework/lua/lbaselib.c"
	"${SRC}/src/framework/lua/lcode.c"
	"${SRC}/src/framework/lua/lcorolib.c"
	"${SRC}/src/framework/lua/lctype.c"
	"${SRC}/src/framework/lua/ldblib.c"
	"${SRC}/src/framework/lua/ldebug.c"
	"${SRC}/src/framework/lua/ldo.c"
	"${SRC}/src/framework/lua/ldump.c"
	"${SRC}/src/framework/lua/lfunc.c"
	"${SRC}/src/framework/lua/lgc.c"
	"${SRC}/src/framework/lua/linit.c"
	"${SRC}/src/framework/lua/liolib.c"
	"${SRC}/src/framework/lua/llex.c"
	"${SRC}/src/framework/lua/lmathlib.c"
	"${SRC}/src/framework/lua/lmem.c"
	"${SRC}/src/framework/lua/loadlib.c"
	"${SRC}/src/framework/lua/lobject.c"
	"${SRC}/src/framework/lua/lopcodes.c"
	"${SRC}/src/framework/lua/loslib.c"
	"${SRC}/src/framework/lua/lparser.c"
	"${SRC}/src/framework/lua/lstate.c"
	"${SRC}/src/framework/lua/lstring.c"
	"${SRC}/src/framework/lua/lstrlib.c"
	"${SRC}/src/framework/lua/ltable.c"
	"${SRC}/src/framework/lua/ltablib.c"
	"${SRC}/src/framework/lua/ltm.c"
	"${SRC}/src/framework/lua/lundump.c"
	"${SRC}/src/framework/lua/lutf8lib.c"
	"${SRC}/src/framework/lua/lvm.c"
	"${SRC}/src/framework/lua/lzio.c"
	"${SRC}/src/framework/lua/lapi.h"
	"${SRC}/src/framework/lua/lauxlib.h"
	"${SRC}/src/framework/lua/lcode.h"
	"${SRC}/src/framework/lua/lctype.h"
	"${SRC}/src/framework/lua/ldebug.h"
	"${SRC}/src/framework/lua/ldo.h"
	"${SRC}/src/framework/lua/lfunc.h"
	"${SRC}/src/framework/lua/lgc.h"
	"${SRC}/src/framework/lua/ljumptab.h"
	"${SRC}/src/framework/lua/llex.h"
	"${SRC}/src/framework/lua/llimits.h"
	"${SRC}/src/framework/lua/lmem.h"
	"${SRC}/src/framework/lua/lobject.h"
	"${SRC}/src/framework/lua/lopcodes.h"
	"${SRC}/src/framework/lua/lopnames.h"
	"${SRC}/src/framework/lua/lparser.h"
	"${SRC}/src/framework/lua/lprefix.h"
	"${SRC}/src/framework/lua/lstate.h"
	"${SRC}/src/framework/lua/lstring.h"
	"${SRC}/src/framework/lua/ltable.h"
	"${SRC}/src/framework/lua/ltm.h"
	"${SRC}/src/framework/lua/luaconf.h"
	"${SRC}/src/framework/lua/lua.h"
	"${SRC}/src/framework/lua/lualib.h"
	"${SRC}/src/framework/lua/lundump.h"
	"${SRC}/src/framework/lua/lvm.h"
	"${SRC}/src/framework/lua/lzio.h"

	"${SRC}/src/game/game.c"
	"${SRC}/src/game/game.h"
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

	# With Lua, compile-time warnings show up if POSIX features aren't used on POSIX platforms.
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
