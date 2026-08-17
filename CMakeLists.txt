cmake_minimum_required(VERSION 3.25.0)

project(PitchPanicVST3
    VERSION 1.0.0
    DESCRIPTION "Whammy, dive-bomb, harmony and octaver VST3 effect"
    LANGUAGES C CXX
)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

if(NOT DEFINED VST3_SDK_ROOT)
    if(DEFINED ENV{VST3_SDK_ROOT})
        set(VST3_SDK_ROOT "$ENV{VST3_SDK_ROOT}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk/CMakeLists.txt")
        set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk/CMakeLists.txt")
        set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk")
    else()
        message(FATAL_ERROR "Set VST3_SDK_ROOT to a complete Steinberg VST3 SDK checkout")
    endif()
endif()

get_filename_component(VST3_SDK_ROOT "${VST3_SDK_ROOT}" ABSOLUTE)

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_MODULE_INFO ON CACHE BOOL "" FORCE)

add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
smtg_enable_vst3_sdk()

set(PITCHPANIC_SOURCES
    source/cids.h
    source/controller.cpp
    source/controller.h
    source/factory.cpp
    source/parameters.h
    source/pitchengine.cpp
    source/pitchengine.h
    source/processor.cpp
    source/processor.h
    source/version.h
    resource/pitchpanic.uidesc
)

smtg_add_vst3plugin(PitchPanic
    PACKAGE_NAME "PitchPanic"
    SOURCES_LIST ${PITCHPANIC_SOURCES}
)

target_compile_features(PitchPanic PRIVATE cxx_std_17)
target_include_directories(PitchPanic PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
    "${VST3_SDK_ROOT}"
)
target_link_libraries(PitchPanic PRIVATE sdk vstgui_support)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(PitchPanic PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-gnu-zero-variadic-macro-arguments
    )
elseif(MSVC)
    target_compile_options(PitchPanic PRIVATE /W4)
endif()

smtg_target_add_plugin_resources(PitchPanic
    RESOURCES resource/pitchpanic.uidesc
)

if(SMTG_MAC)
    smtg_target_set_bundle(PitchPanic
        BUNDLE_IDENTIFIER "audio.panicmechanics.pitchpanic"
        COMPANY_NAME "Panic Mechanics"
    )
    # The SDK helper only fills these fields for Xcode projects. Keep Make/Ninja
    # bundles equally complete because the one-command build uses Unix Makefiles.
    set_target_properties(PitchPanic PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "audio.panicmechanics.pitchpanic"
        MACOSX_BUNDLE_BUNDLE_NAME "Pitch Panic"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_STRING "Pitch Panic ${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 Panic Mechanics"
    )

    get_target_property(PITCHPANIC_PACKAGE_PATH PitchPanic SMTG_PLUGIN_PACKAGE_PATH)
    add_custom_target(PitchPanicBundle ALL
        COMMAND codesign --force --deep --sign - "${PITCHPANIC_PACKAGE_PATH}"
        DEPENDS PitchPanic
        COMMENT "Applying final ad-hoc signature after resources and moduleinfo"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(pitchpanic_dsp_smoke
        tests/pitchengine_test.cpp
        source/pitchengine.cpp
        source/pitchengine.h
        source/parameters.h
    )
    target_compile_features(pitchpanic_dsp_smoke PRIVATE cxx_std_17)
    target_include_directories(pitchpanic_dsp_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/source"
        "${VST3_SDK_ROOT}"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(pitchpanic_dsp_smoke PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME pitchpanic_dsp_smoke COMMAND pitchpanic_dsp_smoke)

    add_executable(pitchpanic_ui_smoke tests/ui_resource_test.cpp)
    target_compile_features(pitchpanic_ui_smoke PRIVATE cxx_std_17)
    target_compile_definitions(pitchpanic_ui_smoke PRIVATE
        PITCHPANIC_UIDESC_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resource/pitchpanic.uidesc"
    )
    target_include_directories(pitchpanic_ui_smoke PRIVATE
        "${VST3_SDK_ROOT}"
        "${VST3_SDK_ROOT}/vstgui4"
    )
    target_link_libraries(pitchpanic_ui_smoke PRIVATE vstgui vstgui_uidescription)
    add_test(NAME pitchpanic_ui_smoke COMMAND pitchpanic_ui_smoke)
endif()
