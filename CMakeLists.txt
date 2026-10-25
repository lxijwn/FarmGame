cmake_minimum_required(VERSION 3.20)
project(FarmGame LANGUAGES C CXX)
get_filename_component(DEFAULT_COCOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../cocos2d-x-3.17.2" ABSOLUTE)
set(COCOS2DX_ROOT_PATH "${DEFAULT_COCOS_ROOT}" CACHE PATH "cocos2d-x root")
set(CMAKE_MODULE_PATH "${COCOS2DX_ROOT_PATH}/cmake/Modules")
set(BUILD_LUA_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_JS_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /wd4996")
include(CocosBuildSet)
add_subdirectory("${COCOS2DX_ROOT_PATH}/cocos" "${ENGINE_BINARY_PATH}/cocos/core")
# Replace legacy PCH flags with explicit Ninja dependencies, without editing SDK.
set_target_properties(cocos2d PROPERTIES COMPILE_FLAGS "")
set_source_files_properties("${COCOS2DX_ROOT_PATH}/cocos/precheader.cpp"
    TARGET_DIRECTORY cocos2d PROPERTIES COMPILE_FLAGS "")
get_target_property(ENGINE_INTERFACE_SOURCES cocos2d INTERFACE_SOURCES)
if(ENGINE_INTERFACE_SOURCES)
    list(FILTER ENGINE_INTERFACE_SOURCES EXCLUDE REGEX "precheader\\.cpp$")
    set_property(TARGET cocos2d PROPERTY INTERFACE_SOURCES "${ENGINE_INTERFACE_SOURCES}")
endif()
target_precompile_headers(cocos2d PRIVATE "${COCOS2DX_ROOT_PATH}/cocos/precheader.h")
add_executable(FarmGame WIN32
    proj.win32/main.cpp Classes/AppDelegate.cpp Classes/Data/Catalog.cpp
    Classes/Scenes/FarmScenes.cpp Classes/Validation/Runtime.cpp
    Classes/Gameplay/TrafficModel.cpp Classes/Validation/TrafficChecks.cpp
    Classes/Gameplay/BoardModel.cpp Classes/Gameplay/GameModel.cpp
    Classes/Validation/GameplayChecks.cpp)
target_include_directories(FarmGame PRIVATE Classes)
target_compile_features(FarmGame PRIVATE cxx_std_14)
target_link_libraries(FarmGame cocos2d shell32)
setup_cocos_app_config(FarmGame)
set_target_properties(FarmGame PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
cocos_copy_target_dll(FarmGame)
add_custom_target(FarmResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "${CMAKE_BINARY_DIR}/bin/Resources"
    COMMENT "Synchronizing Farm resources" VERBATIM)
add_dependencies(FarmGame FarmResources)
set(MSVC_RUNTIME_DIR "" CACHE PATH "x86 VC142 CRT directory")
if(MSVC_RUNTIME_DIR)
    file(GLOB CRT_RUNTIME_DLLS "${MSVC_RUNTIME_DIR}/*.dll")
    foreach(CRT_DLL IN LISTS CRT_RUNTIME_DLLS)
        add_custom_command(TARGET FarmGame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CRT_DLL}" "$<TARGET_FILE_DIR:FarmGame>" VERBATIM)
    endforeach()
endif()
