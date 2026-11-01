cmake_minimum_required(VERSION 3.22)

project(DanketSub VERSION 0.1.0 LANGUAGES CXX)

option(DANKET_BUILD_DESKTOP "Build the Qt 6 desktop client" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# GUI-free ASS model, parser and writer. Deliberately does not use wxWidgets.
add_library(danket_core STATIC
    src/ass_attachment.cpp
    src/ass_dialogue.cpp
    src/ass_entry.cpp
    src/ass_file.cpp
    src/ass_override.cpp
    src/ass_parser.cpp
    src/ass_style.cpp
    src/core/ass_io.cpp
    src/core/core_context.cpp
    src/core/undo_manager.cpp

    libaegisub/ass/time.cpp
    libaegisub/ass/string_codec.cpp
    libaegisub/ass/uuencode.cpp
    libaegisub/common/color.cpp
    libaegisub/common/parser.cpp
    libaegisub/common/ycbcr.cpp
)

set_target_properties(danket_core PROPERTIES OUTPUT_NAME danketsub-core)

target_include_directories(danket_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libaegisub/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/boost_1_83_0/boost")
    target_include_directories(danket_core SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/subprojects/boost_1_83_0
    )
else()
    find_package(Boost 1.70 REQUIRED)
    if(TARGET Boost::headers)
        target_link_libraries(danket_core PUBLIC Boost::headers)
    elseif(TARGET Boost::boost)
        target_link_libraries(danket_core PUBLIC Boost::boost)
    else()
        target_include_directories(danket_core SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
    endif()
endif()

if(MSVC)
    target_compile_options(danket_core PRIVATE /utf-8 /bigobj)
endif()

enable_testing()

add_executable(danket_core_tests tests/core/core_tests.cpp)
target_link_libraries(danket_core_tests PRIVATE danket_core)

add_test(NAME danket_core_tests
    COMMAND danket_core_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/core
)

if(DANKET_BUILD_DESKTOP)
    add_subdirectory(src/app)
endif()
