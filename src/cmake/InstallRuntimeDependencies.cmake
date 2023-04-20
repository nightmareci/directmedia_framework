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

function(install_runtime_dependencies TARGET)
	if(NOT TARGET "${TARGET}")
		message(FATAL_ERROR "Build target \"${TARGET}\" does not exist")
	endif()

	if("${CMAKE_SYSTEM_NAME}" STREQUAL Windows AND MSVC)
		# This seems to be valid on Windows 10, at least.
		# Update as necessary, if other system libraries get picked up.
		install(CODE "
			set(PRE_EXCLUDES
				\"^api-ms-win\"
			)

			set(POST_EXCLUDES
				\"system32\"
			)

			file(GET_RUNTIME_DEPENDENCIES
				EXECUTABLES $<TARGET_FILE:${TARGET}>
				PRE_EXCLUDE_REGEXES \${PRE_EXCLUDES}
				POST_EXCLUDE_REGEXES \${POST_EXCLUDES}
				RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPENDENCIES
				UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPENDENCIES
			)
			file(INSTALL
				DESTINATION \${CMAKE_INSTALL_PREFIX}
				TYPE SHARED_LIBRARY
				FOLLOW_SYMLINK_CHAIN
				FILES \${RESOLVED_DEPENDENCIES}
			)
		")
	else()
		message(WARNING "The current build target and compiler are not supported for installation of runtime dependencies")
	endif()
endfunction()