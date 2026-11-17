cmake_minimum_required(VERSION 3.20)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(Starz VERSION 0.5.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VST3_SDK_ROOT "" CACHE PATH "Path to the VST3 SDK checkout")
set(VST3_SDK_TAG "master" CACHE STRING "VST3 SDK git ref used when VST3_SDK_ROOT is empty")

set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)

if(VST3_SDK_ROOT)
    set(sdk_dir ${VST3_SDK_ROOT})
else()
    include(FetchContent)
    FetchContent_Declare(vst3sdk
        GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
        GIT_TAG ${VST3_SDK_TAG}
        GIT_SHALLOW ON
        GIT_SUBMODULES_RECURSE ON)
    FetchContent_MakeAvailable(vst3sdk)
    set(sdk_dir ${vst3sdk_SOURCE_DIR})
endif()

if(NOT TARGET sdk)
    add_subdirectory(${sdk_dir} ${CMAKE_BINARY_DIR}/vst3sdk)
endif()
smtg_enable_vst3_sdk()

set(STARZ_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vocal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fx)

set(SCE ${sdk_dir}/public.sdk/source/vst/vstsinglecomponenteffect.cpp)

smtg_add_vst3plugin(StarzVocal src/vocal/plugin.cpp ${SCE})
target_include_directories(StarzVocal PRIVATE ${STARZ_INCLUDES})
target_link_libraries(StarzVocal PRIVATE sdk)

smtg_add_vst3plugin(StarzReverb src/fx/fxplugin.cpp ${SCE})
target_include_directories(StarzReverb PRIVATE ${STARZ_INCLUDES})
target_compile_definitions(StarzReverb PRIVATE ARK_REVERB=1)
target_link_libraries(StarzReverb PRIVATE sdk)

smtg_add_vst3plugin(StarzDelay src/fx/fxplugin.cpp ${SCE})
target_include_directories(StarzDelay PRIVATE ${STARZ_INCLUDES})
target_compile_definitions(StarzDelay PRIVATE ARK_DELAY=1)
target_link_libraries(StarzDelay PRIVATE sdk)

if(WIN32)
    foreach(t StarzVocal StarzReverb StarzDelay)
        target_link_libraries(${t} PRIVATE gdiplus shlwapi)
    endforeach()
endif()
