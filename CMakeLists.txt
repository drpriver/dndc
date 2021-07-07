# I just use the makefiles, but here is my attempt
# at writing a CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(Dndc VERSION 0.6.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(Python3 3.8 COMPONENTS Development)

include_directories(Utils Dndc PythonEmbed Allocators PythonEmbed/frozen)

if(WIN32)
include_directories(Platform/Windows)
add_compile_options("-D_CRT_NONSTDC_NO_DEPRECATE" "-D_CRT_SECURE_NO_WARNINGS")
elseif(APPLE)
elseif(UNIX)
endif()

add_executable(dndc
    Dndc/dndc.c
    PythonEmbed/frozenstdlib.c
    )

target_link_libraries(dndc ${Python3_LIBRARIES})
target_include_directories(dndc PRIVATE ${Python3_INCLUDE_DIRS})
target_compile_definitions(dndc PRIVATE DNDCMAIN)

# Cmake sucks and doesn't allow a library and
# executable with the same name, even though the output files are different!
add_library(dndc-lib
    Dndc/dndc.c
    PythonEmbed/frozenstdlib.c
	)
target_link_libraries(dndc-lib ${Python3_LIBRARIES})
target_include_directories(dndc-lib PRIVATE ${Python3_INCLUDE_DIRS})
set_target_properties(dndc-lib
        PROPERTIES OUTPUT_NAME dndc)

Python3_add_library(pydndc
    MODULE
    Dndc/pydndc.c
    )

if(APPLE)
    # TODO: We want code signing, but whatever for now.
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
    enable_language(OBJC)
    add_executable(gdndc
        MACOSX_BUNDLE
        Platform/MacOS/gdndc.m
        PythonEmbed/frozenstdlib.c
        )
    set_target_properties(gdndc PROPERTIES
        MACOSX_BUNDLE YES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Macos/Info.plist)
    # this is kind of weird
    target_compile_definitions(gdndc PRIVATE APP_ICON_PATH="../Platform/MacOS/app_icon.png")

    target_link_libraries(gdndc
        ${Python3_LIBRARIES}
        "-framework Cocoa"
        "-framework WebKit"
        )
    target_compile_options(gdndc PUBLIC "-fobjc-arc")
    target_include_directories(gdndc PRIVATE ${Python3_INCLUDE_DIRS} Dndc)
endif()

if(WIN32)
add_executable(gdndc
    Platform/Windows/gdndc.cpp
    Dndc/dndc.c
    PythonEmbed/frozenstdlib.c
    )
target_link_libraries(gdndc ${Python3_LIBRARIES})
target_include_directories(gdndc PRIVATE ${Python3_INCLUDE_DIRS} Dndc)
endif()

# tests
enable_testing()
add_executable(TestArgumentParsing Utils/TestArgumentParsing.c)
add_test(argumentparsing TestArgumentParsing)
add_executable(TestBase64 Utils/TestBase64.c)
add_test(base64 TestBase64)
add_executable(TestByteBuilder Utils/TestByteBuilder.c)
add_test(bytebuilder TestByteBuilder)
add_executable(TestMStringBuilder Utils/TestMStringBuilder.c)
add_test(mstringbuilder TestMStringBuilder)
add_executable(TestPathUtil Utils/TestPathUtil.c)
add_test(pathutil TestPathUtil)
add_executable(TestStrUtils Utils/TestStrUtils.c)
add_test(strutil TestStrUtils)
add_executable(TestRarray Utils/TestRarray.c)
add_test(rarray TestRarray)

add_executable(TestDndc Dndc/TestDndc.c PythonEmbed/frozenstdlib.c)
add_test(dndc TestDndc -C ${CMAKE_SOURCE_DIR})
target_link_libraries(TestDndc ${Python3_LIBRARIES})
target_include_directories(TestDndc PRIVATE ${Python3_INCLUDE_DIRS})
