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

set(ORG "nightmareci")
set(EXE "${CMAKE_PROJECT_NAME}")
set(NAME "Skill Clear")

option(SHOW_RELEASE_VERSION "If enabled, the build is a finalized release version, and the game will show the release version. If disabled, the text \"Prerelease <UTC build timestamp>\" will be shown instead.")

if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE MATCHES Debug)
	set(IS_DEBUG TRUE)
else()
	set(IS_DEBUG FALSE)
endif()

option(MEM_DEBUG "If enabled, memory debugging will be included regardless of build type." ${IS_DEBUG})
if(MEM_DEBUG)
	add_compile_definitions(MEM_DEBUG)
endif()

option(RELEASE_DEBUG "If enabled, assertions via release_assert will be active regardless of build type." ${IS_DEBUG})
if(RELEASE_DEBUG)
	add_compile_definitions(RELEASE_DEBUG)
endif()

string(TIMESTAMP CONFIGURE_TIME UTC)

if(SHOW_RELEASE_VERSION)
	set(VERSION "v${CMAKE_PROJECT_VERSION}")
else()
	set(VERSION "Prerelease ${CONFIGURE_TIME}")
endif()

set(BUILD_TARGET "${CMAKE_SYSTEM_NAME}" CACHE STRING "The build target. By default, the current platform is the build target.")
message(STATUS "Build target: ${BUILD_TARGET}")

option(USE_VENDOR_LIBRARIES "Enable building and usage of the vendor-provided libraries instead of system-installed libraries." FALSE)

configure_file("${SRC}/src/main/private/app_const.c.in" "${BIN}/src/main/private/app_const.c" @ONLY)

# TODO: Refactor so headers are precompiled.
add_executable("${EXE}"
	# Sources for the game follow. All game sources should be under the
	# src/game directory, not in any other directories in src/.


	"${SRC}/src/game/game.h"

	"${SRC}/src/game/game.c"


	# Sources for the engine follow.


	"${SRC}/src/main/app.h"
	"${SRC}/src/main/main.h"

	"${SRC}/src/main/private/app_private.h"

	"${SRC}/src/main/private/app.c"
	"${BIN}/src/main/private/app_const.c"
	"${SRC}/src/main/private/main.c"


	"${SRC}/src/input/action.h"

	"${SRC}/src/input/private/action.c"


	"${SRC}/src/render/render.h"
	"${SRC}/src/render/render_types.h"

	# OpenGL-using sources should only ever include opengl.h; the
	# other sources are just internal implementation details to how OpenGL
	# is made available to the code. Different platforms could use
	# different internal means to provide OpenGL, with the opengl.h header
	# providing a standard way to access OpenGL on all platforms.

	"${SRC}/src/render/private/glad.h"
	"${SRC}/src/render/private/khrplatform.h"
	"${SRC}/src/render/private/opengl.h"

	"${SRC}/src/render/private/glad.c"
	"${SRC}/src/render/private/opengl.c"

	"${SRC}/src/render/private/frames.h"
	"${SRC}/src/render/private/layers.h"
	"${SRC}/src/render/private/print.h"
	"${SRC}/src/render/private/render_private.h"
	"${SRC}/src/render/private/sprites.h"

	"${SRC}/src/render/private/frames.c"
	"${SRC}/src/render/private/layers.c"
	"${SRC}/src/render/private/print.c"
	"${SRC}/src/render/private/render.c"
	"${SRC}/src/render/private/sprites.c"


	"${SRC}/src/audio/audio.h"

	"${SRC}/src/audio/private/audio_private.h"

	"${SRC}/src/audio/private/audio.c"


	"${SRC}/src/util/dict.h"
	"${SRC}/src/util/font.h"
	"${SRC}/src/util/ini.h"
	"${SRC}/src/util/log.h"
	"${SRC}/src/util/maths.h"
	"${SRC}/src/util/mem.h"
	"${SRC}/src/util/nanotime.h"
	"${SRC}/src/util/queue.h"
	"${SRC}/src/util/str.h"

	"${SRC}/src/util/private/conqueue.h"

	"${SRC}/src/util/private/conqueue.c"
	"${SRC}/src/util/private/dict.c"
	"${SRC}/src/util/private/font.c"
	"${SRC}/src/util/private/ini.c"
	"${SRC}/src/util/private/log.c"
	"${SRC}/src/util/private/maths.c"
	"${SRC}/src/util/private/mem.c"
	"${SRC}/src/util/private/nanotime.c"
	"${SRC}/src/util/private/queue.c"
	"${SRC}/src/util/private/str.c"


	"${SRC}/src/data/data.h"
	"${SRC}/src/data/data_font.h"
	"${SRC}/src/data/data_music.h"
	"${SRC}/src/data/data_raw.h"
	"${SRC}/src/data/data_sound.h"
	"${SRC}/src/data/data_texture.h"
	"${SRC}/src/data/data_types.h"

	"${SRC}/src/data/private/data_type_manager.h"

	"${SRC}/src/data/private/data.c"
	"${SRC}/src/data/private/data_font.c"
	"${SRC}/src/data/private/data_music.c"
	"${SRC}/src/data/private/data_raw.c"
	"${SRC}/src/data/private/data_sound.c"
	"${SRC}/src/data/private/data_texture.c"


	# A specific version of Lua as a Git submodule in the repository is
	# used, to ensure it's the exact version the code expects, because
	# updates to Lua can break compatibility; the code of this repository
	# will be adapted to be compatible with updated versions of Lua, to
	# ensure the code doesn't break. Thus, you *must* at least get the Lua
	# submodule with the repository's own content to build at all.  As of
	# last editing this file, the CMake FindLua module doesn't provide any
	# means to request a specific version of Lua to use.


	"${SRC}/lib/lua/lapi.h"
	"${SRC}/lib/lua/lauxlib.h"
	"${SRC}/lib/lua/lcode.h"
	"${SRC}/lib/lua/lctype.h"
	"${SRC}/lib/lua/ldebug.h"
	"${SRC}/lib/lua/ldo.h"
	"${SRC}/lib/lua/lfunc.h"
	"${SRC}/lib/lua/lgc.h"
	"${SRC}/lib/lua/ljumptab.h"
	"${SRC}/lib/lua/llex.h"
	"${SRC}/lib/lua/llimits.h"
	"${SRC}/lib/lua/lmem.h"
	"${SRC}/lib/lua/lobject.h"
	"${SRC}/lib/lua/lopcodes.h"
	"${SRC}/lib/lua/lopnames.h"
	"${SRC}/lib/lua/lparser.h"
	"${SRC}/lib/lua/lprefix.h"
	"${SRC}/lib/lua/lstate.h"
	"${SRC}/lib/lua/lstring.h"
	"${SRC}/lib/lua/ltable.h"
	"${SRC}/lib/lua/ltm.h"
	"${SRC}/lib/lua/luaconf.h"
	"${SRC}/lib/lua/lua.h"
	"${SRC}/lib/lua/lualib.h"
	"${SRC}/lib/lua/lundump.h"
	"${SRC}/lib/lua/lvm.h"
	"${SRC}/lib/lua/lzio.h"

	"${SRC}/lib/lua/lapi.c"
	"${SRC}/lib/lua/lauxlib.c"
	"${SRC}/lib/lua/lbaselib.c"
	"${SRC}/lib/lua/lcode.c"
	"${SRC}/lib/lua/lcorolib.c"
	"${SRC}/lib/lua/lctype.c"
	"${SRC}/lib/lua/ldblib.c"
	"${SRC}/lib/lua/ldebug.c"
	"${SRC}/lib/lua/ldo.c"
	"${SRC}/lib/lua/ldump.c"
	"${SRC}/lib/lua/lfunc.c"
	"${SRC}/lib/lua/lgc.c"
	"${SRC}/lib/lua/linit.c"
	"${SRC}/lib/lua/liolib.c"
	"${SRC}/lib/lua/llex.c"
	"${SRC}/lib/lua/lmathlib.c"
	"${SRC}/lib/lua/lmem.c"
	"${SRC}/lib/lua/loadlib.c"
	"${SRC}/lib/lua/lobject.c"
	"${SRC}/lib/lua/lopcodes.c"
	"${SRC}/lib/lua/loslib.c"
	"${SRC}/lib/lua/lparser.c"
	"${SRC}/lib/lua/lstate.c"
	"${SRC}/lib/lua/lstring.c"
	"${SRC}/lib/lua/lstrlib.c"
	"${SRC}/lib/lua/ltable.c"
	"${SRC}/lib/lua/ltablib.c"
	"${SRC}/lib/lua/ltm.c"
	"${SRC}/lib/lua/lundump.c"
	"${SRC}/lib/lua/lutf8lib.c"
	"${SRC}/lib/lua/lvm.c"
	"${SRC}/lib/lua/lzio.c"
)

target_include_directories("${EXE}" PRIVATE
	"${SRC}/src"
	"${BIN}/src"

	"${SRC}/lib"
)

# TODO: Add the libraries as Git submodules and include them in the build, but
# still have the option to use system-installed libraries.

option(USE_PKG_CONFIG "If TRUE, pkg-config will be used for finding packages, rather than CMake find_package." ${MINGW})

# Use POSIX support when available.
if(UNIX)
	find_library(MATH_LIBRARY m)
	if(NOT MATH_LIBRARY STREQUAL MATH_LIBRARY-NOTFOUND)
		target_link_libraries("${EXE}"
			PRIVATE
				${MATH_LIBRARY}
		)
	endif()

	add_compile_definitions(_POSIX_VERSION=199309L)
endif()

# Adapt Lua to work in this CMake build system project.
if (UNIX)
	# When compiling Lua, compile-time warnings show up if POSIX features aren't
	# used on POSIX platforms.
	add_compile_definitions(LUA_USE_POSIX)
endif()

# SDL and SDL companion libraries.
if(USE_VENDOR_LIBRARIES)
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(BUILD_SHARED_LIBS FALSE)

	set(SDL_SHARED FALSE)
	set(SDL_STATIC TRUE)
	set(SDL_TEST FALSE)
	set(SDL2_DISABLE_UNINSTALL ON)
	add_subdirectory("lib/SDL" "${BIN}/lib/SDL")

	set(SDL2IMAGE_VENDORED ON)
	set(SDL2IMAGE_DEPS_SHARED OFF)
	set(SDL2IMAGE_SAMPLES OFF)
	set(SDL2IMAGE_INSTALL OFF)
	set(SDL2IMAGE_TESTS OFF)
	add_subdirectory("lib/SDL_image" "${BIN}/lib/SDL_image")

	set(SDL2MIXER_VENDORED ON)
	set(SDL2MIXER_DEPS_SHARED OFF)
	set(SDL2MIXER_INSTALL OFF)
	set(SDL2MIXER_SAMPLES OFF)
	set(SDL2MIXER_VORBIS "VORBISFILE" CACHE STRING "Override")
	set(SDL2MIXER_VORBIS_VORBISFILE_SHARED OFF)
	set(SDL2MIXER_OPUS_SHARED OFF)
	add_subdirectory("lib/SDL_mixer" "${BIN}/lib/SDL_mixer")

	# SDL_main isn't used on all platforms.
	if(TARGET SDL2::SDL2main)
		target_link_libraries("${EXE}" PUBLIC SDL2::SDL2main)
	endif()
	target_link_libraries("${EXE}" PUBLIC
		SDL2::SDL2-static
		SDL2_image::SDL2_image-static
		SDL2_mixer::SDL2_mixer-static
	)
elseif(USE_PKG_CONFIG)
	# Using find_package for the needed libraries is broken in MSYS2, but
	# using pkg-config works fine. And we can use the nice IMPORTED_TARGET
	# feature for cleaner code here, as we already require a sufficient
	# CMake version (3.6 or higher required for IMPORTED_TARGET support).

	find_package(PkgConfig REQUIRED)
	set(DEPENDENCIES
		SDL2
		SDL2_image
		SDL2_mixer
	)
	foreach(DEPENDENCY ${DEPENDENCIES})
		pkg_check_modules("${DEPENDENCY}" REQUIRED IMPORTED_TARGET "${DEPENDENCY}")
		target_link_libraries("${EXE}" PRIVATE "PkgConfig::${DEPENDENCY}")
	endforeach()
else()
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
endif()

option(BUNDLE_LIBRARIES "Bundle the libraries needed by the game's executable, for distributable releases." FALSE)

# TODO: Generate game resource files from resource source files at build time
# into the build directory resource location; install the build directory
# resource directory into the installed resource location.

include("${SRC}/src/cmake/InstallRuntimeDependencies.cmake")
if(BUILD_TARGET STREQUAL Windows AND MSVC AND NOT MINGW)
	option(HIDE_WINDOWS_CONSOLE "Hide the Windows console.")

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

	# TODO: Generate a copyright file with all libraries' licenses appended to
	# the base copyright file for installs.
	install(FILES "${SRC}/copyright.txt" DESTINATION ".")

	install(FILES "${SRC}/resource/font.fnt" DESTINATION "resource")
	install(FILES "${SRC}/resource/font_0.png" DESTINATION "resource")
elseif(BUILD_TARGET STREQUAL Darwin)
	# TODO: Support installation for macOS.
elseif(BUILD_TARGET STREQUAL Linux)
	# TODO: Support installation for Linux.
endif()

set(CPACK_PACKAGE_NAME "${NAME}")
set(CPACK_PACKAGE_VENDOR "${ORG}")
if(VCPKG_TARGET_TRIPLET)
	set(CPACK_PACKAGE_FILE_NAME "${NAME} ${VCPKG_TARGET_TRIPLET}")
elseif(BUILD_TARGET STREQUAL Darwin)
	set(CPACK_PACKAGE_FILE_NAME "${NAME} macOS")
elseif(APPLE OR NOT CMAKE_SYSTEM_PROCESSOR)
	set(CPACK_PACKAGE_FILE_NAME "${NAME} ${BUILD_TARGET}")
else()
	set(CPACK_PACKAGE_FILE_NAME "${NAME} ${BUILD_TARGET} ${CMAKE_SYSTEM_PROCESSOR}")
endif()
include(CPack)
