#
# Copyright (c) 2021-2025, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

set(dlss_sdk "${dlss_SOURCE_DIR}")

if (WIN32)
	add_library(DLSS SHARED IMPORTED)

	set(dlss_platform "Windows_x86_64")
	set(dlss_lib_release "nvsdk_ngx_s.lib")
	set(dlss_lib_debug "nvsdk_ngx_s_dbg.lib")

	set_target_properties(DLSS PROPERTIES
		IMPORTED_IMPLIB "${dlss_sdk}/lib/${dlss_platform}/x64/${dlss_lib_release}"
		IMPORTED_IMPLIB_DEBUG "${dlss_sdk}/lib/${dlss_platform}/x64/${dlss_lib_debug}"
		IMPORTED_LOCATION "${dlss_sdk}/lib/${dlss_platform}/rel/nvngx_dlss.dll"
		IMPORTED_LOCATION_DEBUG "${dlss_sdk}/lib/${dlss_platform}/dev/nvngx_dlss.dll"
	)

	set(DLSS_SHARED_LIBRARY_PATH "${dlss_sdk}/lib/${dlss_platform}/$<IF:$<CONFIG:Debug>,dev,rel>/nvngx_dlss.dll" PARENT_SCOPE)

elseif (UNIX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	add_library(DLSS STATIC IMPORTED)

	set(dlss_platform "Linux_x86_64")

	# Search for the DLSS library files, because the version number is included in the filename,
	# and we don't know which version was pulled - that's controlled by DONUT_DLSS_FETCH_TAG.

	set(dlss_lib_search_path "${dlss_sdk}/lib/${dlss_platform}/rel/libnvidia-ngx-dlss.so.*")
	file(GLOB dlss_lib_files ${dlss_lib_search_path})

	if (dlss_lib_files)
		list(LENGTH dlss_lib_files dlss_lib_files_count)
		if (dlss_lib_files_count EQUAL 1)
			get_filename_component(dlss_lib "${dlss_lib_files}" NAME)
			message(STATUS "Found DLSS shared library file: ${dlss_lib}")
		else()
			message(FATAL_ERROR "Expected exactly one DLSS library file, found ${dlss_lib_files_count}")
		endif()
	else()
		message(FATAL_ERROR "No DLSS library files found at ${dlss_lib_search_path}")
	endif()
	
	set_target_properties(DLSS PROPERTIES
		IMPORTED_LOCATION "${dlss_sdk}/lib/${dlss_platform}/libnvsdk_ngx.a"
	)

	set(DLSS_SHARED_LIBRARY_PATH "${dlss_sdk}/lib/${dlss_platform}/$<IF:$<CONFIG:Debug>,dev,rel>/${dlss_lib}" PARENT_SCOPE)

else()
	message("DLSS is not supported on the target platform.")
endif()

if (TARGET DLSS)
	target_include_directories(DLSS INTERFACE "${dlss_sdk}/include")
endif()
