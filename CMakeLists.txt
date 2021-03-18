# I just use the makefiles, but here is my attempt
# at writing a CMakeLists.txt
# It is missing many targets.
cmake_minimum_required(VERSION 3.16)
project(Dndc VERSION 0.3.5)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(Python3 3.8 COMPONENTS Development)

include_directories(Utils DndC PythonEmbed Allocators)

if(WIN32)
include_directories(Platform/Windows)
elseif(APPLE)
elseif(UNIX)
endif()

add_executable(dndc
		DndC/dndc.c
		PythonEmbed/frozenstdlib.c
		)

target_link_libraries(dndc ${Python3_LIBRARIES})
target_include_directories(dndc PRIVATE ${Python3_INCLUDE_DIRS})

Python3_add_library(pydndc
		MODULE
		DndC/pydndc.c
		)
target_compile_definitions(pydndc PRIVATE NOMAIN)

if(APPLE)
	# TODO: We want code signing, but whatever for now.
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
	enable_language(OBJC)
	add_executable(gdndc
			MACOSX_BUNDLE
			Platform/MacOS/gdndc.m
			DndC/dndc.c
			PythonEmbed/frozenstdlib.c
			)
	set_target_properties(gdndc PROPERTIES
		MACOSX_BUNDLE YES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Macos/Info.plist)
	target_compile_definitions(gdndc PRIVATE NOMAIN)
	# this is kind of weird
	target_compile_definitions(gdndc PRIVATE APP_ICON_PATH="../Platform/MacOS/app_icon.png")

	target_link_libraries(gdndc
			${Python3_LIBRARIES}
			"-framework Cocoa"
			"-framework AppKit"
			"-framework WebKit"
			)
	target_compile_options(gdndc PUBLIC "-fobjc-arc")
	target_include_directories(gdndc PRIVATE ${Python3_INCLUDE_DIRS} DndC)
endif()
