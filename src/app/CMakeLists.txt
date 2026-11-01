# Qt 6 desktop client for DanketSub.
#
# The GUI-free core library stays buildable on its own with
# -DDANKET_BUILD_DESKTOP=OFF; this target adds Qt Quick, RibbonUI and
# QWindowKit on top of it.

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Test
)

qt_standard_project_setup()

include(FetchContent)

# QWindowKit must be configured before RibbonUI so RibbonUI reuses the same
# static Quick module instead of its vendored submodule copy.
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_INSTALL OFF CACHE BOOL "" FORCE)
set(QWINDOWKIT_ENABLE_STYLE_AGENT ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS ON CACHE BOOL "" FORCE)

FetchContent_Declare(qwindowkit
    GIT_REPOSITORY https://github.com/stdware/qwindowkit.git
    GIT_TAG 5626b3ae5f22508d77361495f589abedb39c2ee4
    GIT_SUBMODULES qmsetup
)
FetchContent_MakeAvailable(qwindowkit)

set(RIBBONUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RIBBONUI_BUILD_QWINDOWKIT OFF CACHE BOOL "" FORCE)
set(RIBBONUI_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(RibbonUI
    GIT_REPOSITORY https://github.com/mentalfl0w/RibbonUI.git
    GIT_TAG 8a541e39393bb03825545156b851e7c7654825c0
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(RibbonUI)

# RibbonUI headers switch to plain (non-dllimport) linkage for static builds.
target_compile_definitions(RibbonUI PUBLIC RIBBONUI_BUILD_STATIC_LIB)

# ---------------------------------------------------------------------------
# QML module + theme manager, shared by the client and the UI tests.
# ---------------------------------------------------------------------------
qt_add_library(danket_ui STATIC)

qt_add_qml_module(danket_ui
    URI DanketSub
    VERSION 0.1
    RESOURCE_PREFIX "/qt/qml"
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/DanketSub
    QML_FILES
        Main.qml
        Workspace.qml
        ThemeSettingsPage.qml
        FoldRibbonButton.qml
    SOURCES
        theme_manager.cpp
        theme_manager.h
)

target_link_libraries(danket_ui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    RibbonUI
)
target_include_directories(danket_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# A static RibbonUI keeps its QML module in a plugin; link it explicitly.
if(TARGET RibbonUIplugin)
    target_link_libraries(danket_ui PUBLIC RibbonUIplugin)
endif()

# ---------------------------------------------------------------------------
# Client executable.
# ---------------------------------------------------------------------------
qt_add_executable(DanketSub
    WIN32
    main.cpp
)
set_target_properties(DanketSub PROPERTIES OUTPUT_NAME danket-sub)

target_compile_definitions(DanketSub PRIVATE
    RIBBONUI_BUILD_STATIC_LIB
    DANKETSUB_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(DanketSub PRIVATE danket_ui)
if(TARGET danket_uiplugin)
    target_link_libraries(DanketSub PRIVATE danket_uiplugin)
endif()

if(APPLE)
    set_target_properties(DanketSub PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------
# Palette resolution is pure Qt Core/Gui logic; test it without the QML stack.
add_executable(danket_theme_tests
    ${CMAKE_SOURCE_DIR}/tests/app/theme_manager_tests.cpp
    theme_manager.cpp
)
target_include_directories(danket_theme_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(danket_theme_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Qml)
add_test(NAME danket_theme_tests COMMAND danket_theme_tests)
set_tests_properties(danket_theme_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

# Window shell: ribbon folding (button, double-click, Ctrl+F1) and live
# palette switching, driven against the real QML scene.
add_executable(danket_ui_tests ${CMAKE_SOURCE_DIR}/tests/app/ui_tests.cpp)
target_link_libraries(danket_ui_tests PRIVATE danket_ui Qt6::Test)
if(TARGET danket_uiplugin)
    target_link_libraries(danket_ui_tests PRIVATE danket_uiplugin)
endif()
add_test(NAME danket_ui_tests COMMAND danket_ui_tests -platform offscreen)
set_tests_properties(danket_ui_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 120
)
