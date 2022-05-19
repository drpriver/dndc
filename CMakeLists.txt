# I just use the makefiles, but here is my attempt
# at writing a CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(Dndc VERSION 0.17.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(Python3 3.8 COMPONENTS Development Interpreter)
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

if(WIN32)
add_compile_options("-D_CRT_NONSTDC_NO_DEPRECATE" "-D_CRT_SECURE_NO_WARNINGS")
elseif(APPLE)
elseif(UNIX)
set(LIBM_LIBRARIES m)
endif()

add_executable(dndc
    Dndc/dndc_cli.c
)

add_library(quickjs-dylib
    SHARED
    Vendored/libquickjs.c
)
add_library(quickjs-lib
    STATIC
    Vendored/libquickjs.c
)
set_target_properties(quickjs-lib
    PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(quickjs-dylib PRIVATE
    CONFIG_VERSION="2021-03-27"
    BUILDING_SHARED_OBJECT
)
# Just use our project version as their version is date based.
if(WIN32)
set_target_properties(quickjs-lib
    PROPERTIES OUTPUT_NAME quickjs-static
    VERSION ${PROJECT_VERSION}
)
else()
set_target_properties(quickjs-lib
    PROPERTIES OUTPUT_NAME quickjs
    VERSION ${PROJECT_VERSION}
)
endif()
set_target_properties(quickjs-dylib
    PROPERTIES OUTPUT_NAME quickjs
    VERSION ${PROJECT_VERSION}
    SOVERSION 0.${PROJECT_VERSION_MINOR}
)
target_compile_definitions(quickjs-lib PRIVATE CONFIG_VERSION="2021-03-27")

target_link_libraries(dndc quickjs-lib Threads::Threads ${LIBM_LIBRARIES})
target_link_libraries(quickjs-dylib Threads::Threads ${LIBM_LIBRARIES})
# target_compile_definitions(dndc PRIVATE QJS_SHARED_LIBRARY)

add_library(dndc-lib
    STATIC
    Dndc/dndc.c
)
add_library(dndc-dylib
    SHARED
    Dndc/dndc.c
)
target_link_libraries(dndc-dylib Threads::Threads ${LIBM_LIBRARIES})
if(WIN32)
set_target_properties(dndc-lib
    PROPERTIES OUTPUT_NAME dndc-static
    VERSION ${PROJECT_VERSION}
)
else()
set_target_properties(dndc-lib
    PROPERTIES OUTPUT_NAME dndc
    VERSION ${PROJECT_VERSION}
)
endif()
target_link_libraries(dndc-dylib quickjs-lib)
# change the SOVERSION thing when I decide to release 1.0
set_target_properties(dndc-dylib
    PROPERTIES OUTPUT_NAME dndc
    VERSION ${PROJECT_VERSION}
    SOVERSION 0.${PROJECT_VERSION_MINOR}
    PUBLIC_HEADER "Dndc/dndc.h"
)

Python3_add_library(pydndc
    MODULE
    Dndc/pydndc.c
)
target_link_libraries(pydndc PRIVATE quickjs-lib)

if(APPLE)
    # TODO: We want code signing, but whatever for now.
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
    enable_language(OBJC)
	add_executable(DndEdit
        MACOSX_BUNDLE
        Platform/MacOS/gdndc.m
    )
	set_target_properties(DndEdit PROPERTIES
        MACOSX_BUNDLE YES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Platform/MacOS/Info.plist)
    # this is kind of weird
	target_compile_definitions(DndEdit PRIVATE APP_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Platform/MacOS/app_icon.png")

	target_link_libraries(DndEdit
        # quickjs-dylib
        quickjs-lib
        "-framework Cocoa"
        "-framework WebKit"
    )
	#target_compile_definitions(DndEdit PRIVATE QJS_SHARED_LIBRARY)
	target_compile_options(DndEdit PUBLIC "-fobjc-arc")

	add_executable(dndbr
		MACOSX_BUNDLE
		Platform/MacOS/dndbr.m
		Dndc/dndc_local_server.c
	)
	set_target_properties(dndbr PROPERTIES
			MACOSX_BUNDLE YES
			MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Platform/MacOS/DndBrInfo.plist)
	target_compile_definitions(dndbr PRIVATE
			APP_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Platform/MacOS/dndbr_app_icon.png")
	target_link_libraries(dndbr
			dndc-dylib
			"-framework Cocoa"
			)
    target_compile_options(dndbr PUBLIC "-fobjc-arc")

endif()

if(WIN32)
    # this was an experiment.
    #add_executable(gdndc
    #    Platform/Windows/gdndc.cpp
    #    Dndc/dndc.c
    #    )
    #target_link_libraries(gdndc
    #    ${Python3_LIBRARIES}
    #    quickjs-dylib
    #    )
# for rng
target_compile_options(quickjs-lib PUBLIC "-mrdseed")
target_compile_options(quickjs-dylib PUBLIC "-mrdseed")
endif()

add_executable(dndc-browse
    Dndc/dndc_browse.c
)
target_link_libraries(dndc-browse
    dndc-dylib
)
add_executable(dndc-tag
    Dndc/dndc_tag.c
)
target_link_libraries(dndc-tag
    dndc-dylib
)

# tests
enable_testing()
# Util testings
add_executable(TestArgumentParsing Utils/TestArgumentParsing.c)
add_test(argumentparsing TestArgumentParsing)
add_executable(TestBase64 Utils/TestBase64.c)
add_test(base64 TestBase64)
add_executable(TestDsort Utils/TestDsort.c)
add_test(dsort TestDsort)
add_executable(TestMStringBuilder Utils/TestMStringBuilder.c)
add_test(mstringbuilder TestMStringBuilder)
add_executable(TestPathUtil Utils/TestPathUtil.c)
add_test(pathutil TestPathUtil)
add_executable(TestRarray Utils/TestRarray.c)
add_test(rarray TestRarray)
add_executable(TestStrUtils Utils/TestStrUtils.c)
add_test(strutil TestStrUtils)
# Dndc tests
add_executable(TestMsbExtension Utils/TestMsbExtension.c)
add_test(msbext TestMsbExtension)
add_test(NAME testpydndc
		COMMAND ${Python3_EXECUTABLE} Dndc/testpydndc.py --extension-directory ${CMAKE_BINARY_DIR}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(testpydndc PROPERTIES DEPENDS pydndc)

add_executable(TestDndc Dndc/TestDndc.c)
target_compile_definitions(TestDndc PRIVATE QJS_SHARED_LIBRARY)
add_test(dndc TestDndc -C ${CMAKE_SOURCE_DIR})
target_link_libraries(TestDndc
    quickjs-dylib
    Threads::Threads
)

# installs
install(TARGETS dndc DESTINATION bin)
install(TARGETS dndc-lib DESTINATION lib)
install(TARGETS quickjs-dylib DESTINATION lib)
install(TARGETS quickjs-lib DESTINATION lib)
install(TARGETS dndc-dylib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/Dndc
)
install(TARGETS dndc-browse DESTINATION bin)
install(TARGETS dndc-tag DESTINATION bin)
if(UNIX)
set_target_properties(dndc-browse PROPERTIES
    INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib
    BUILD_WITH_INSTALL_RPATH 0
)
set_target_properties(dndc-tag PROPERTIES
    INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib
    BUILD_WITH_INSTALL_RPATH 0
)
endif()

# Qt
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets WebEngineWidgets)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets WebEngineWidgets)

if(Qt${QT_VERSION_MAJOR}_FOUND)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_add_executable(DndcEdit
            MANUAL_FINALIZATION
            QtDndcEdit/DndcEdit.cpp
        )
    else()
        add_executable(DndcEdit
            QtDndcEdit/DndcEdit.cpp
        )
    endif()

	#target_include_directories(DndcEdit PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(DndcEdit PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::WebEngineWidgets
        dndc-dylib)

    if(APPLE)
        set_target_properties(DndcEdit PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER david.com
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        )
    elseif(UNIX)
        # This sets it up so that during development it uses local libs but at
        # install time it uses the proper installed libs.
        set_target_properties(DndcEdit PROPERTIES
            INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib
            BUILD_WITH_INSTALL_RPATH 0
        )
        install(TARGETS DndcEdit DESTINATION bin)
    endif()

    if(WIN32)
        target_link_options(DndcEdit PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()

    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        qt_finalize_executable(DndcEdit)
    endif()

endif()
