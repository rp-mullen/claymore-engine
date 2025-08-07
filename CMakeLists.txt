cmake_minimum_required(VERSION 3.15)
project(Claymore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ============================
# MSVC OPTIONS
# ============================
if(MSVC)
    add_compile_options(/Zc:__cplusplus /Zc:preprocessor)
endif()

# ============================
# ASSIMP CONFIGURATION
# ============================
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)

# ============================
# EXTERNAL DEPENDENCIES
# ============================
add_subdirectory(external/glfw)
add_subdirectory(external/assimp)

foreach(lib glfw assimp)
    set_target_properties(${lib} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endforeach()

# ============================
# JOLT PHYSICS
# ============================
file(GLOB_RECURSE JOLT_SRC external/JoltPhysics/Jolt/*.cpp)
add_library(JoltPhysics STATIC ${JOLT_SRC})
target_include_directories(JoltPhysics PUBLIC external/JoltPhysics)
target_compile_options(JoltPhysics PRIVATE /wd4244 /wd4305)
set_target_properties(JoltPhysics PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# ============================
# OPENSSL CONFIG
# ============================
find_package(OpenSSL REQUIRED)

# ============================
# DOTNET BRIDGE (Dynamic nethost.dll)
# ============================
add_library(DotnetBridge STATIC
    src/interop/DotnetBridge.cpp
    src/scripting/DotNetHost.cpp
    src/scripting/ScriptReflectionInterop.cpp
)
target_include_directories(DotnetBridge PUBLIC
    external/dotnet_hosting
    external/json/include
    external/glm
    src
)

target_link_libraries(DotnetBridge PRIVATE OpenSSL::SSL OpenSSL::Crypto JoltPhysics imgui_bgfx imgui)

set_target_properties(DotnetBridge PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# ============================
# IMGUI
# ============================
file(GLOB IMGUI_SRC
    external/imgui/*.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
)
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC 
    external/imgui
    external/imgui/backends
    external/glfw/include
    src
)
set_target_properties(imgui PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# ============================
# IMGUI BGFX BACKEND
# ============================
file(GLOB IMGUI_BGFX_SRC external/imgui/backends/imgui_impl_bgfx_docking.cpp)
add_library(imgui_bgfx STATIC ${IMGUI_BGFX_SRC})
target_include_directories(imgui_bgfx PUBLIC
    src
    external/bgfx/include
    external/imgui
    external/imgui/backends
    external/bx/include
    external/stb
    external/ImGuizmo
    external/imnodes
    external/bimg/include
)
target_link_libraries(imgui_bgfx PRIVATE imgui OpenSSL::SSL OpenSSL::Crypto JoltPhysics)
set_target_properties(imgui_bgfx PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# ============================
# MAIN EXECUTABLE
# ============================
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS src/*.h)
set(EXTERNAL_SOURCES external/ImGuizmo/ImGuizmo.cpp)

add_executable(Claymore
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
    ${EXTERNAL_SOURCES}
)
target_include_directories(Claymore PRIVATE
    external/bgfx/include
    external/bx/include
    external/bimg/include
    external/glfw/include
    external/glm
    external/assimp/include
    external/ImGuizmo
    external/imnodes
    external/json/include
    external/JoltPhysics
    external/dotnet_hosting
    src
)
set_target_properties(Claymore PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    ENABLE_EXPORTS ON
)

target_link_libraries(Claymore PRIVATE
    ${CMAKE_SOURCE_DIR}/external/bgfx/.build/win64_vs2022/bin/bgfxDebug.lib
    ${CMAKE_SOURCE_DIR}/external/bgfx/.build/win64_vs2022/bin/bimgDebug.lib
    ${CMAKE_SOURCE_DIR}/external/bgfx/.build/win64_vs2022/bin/bxDebug.lib
    glfw
    assimp
    imgui
    imgui_bgfx
    DotnetBridge
    OpenSSL::SSL 
    OpenSSL::Crypto
)

# ============================
# FILE ORGANIZATION IN VS
# ============================
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "src" FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})
source_group("External\\ImGuizmo" FILES ${EXTERNAL_SOURCES})

# ============================
# COMPILE DEFINITIONS
# ============================
foreach(target Claymore imgui imgui_bgfx)
    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:BX_CONFIG_DEBUG=0>
    )
endforeach()

# ============================
# SHADER COMPILATION
# ============================
set(IMGUI_SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders/imgui)
set(IMGUI_SHADER_OUT ${CMAKE_BINARY_DIR}/shaders/imgui)

add_custom_target(compile_imgui_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${IMGUI_SHADER_OUT}
    COMMAND ${CMAKE_SOURCE_DIR}/tools/shaderc.exe
        -f ${IMGUI_SHADER_DIR}/vs_ocornut_imgui.sc
        -o ${IMGUI_SHADER_OUT}/vs_ocornut_imgui.bin
        --type vertex --platform windows --profile s_5_0
        --varyingdef ${IMGUI_SHADER_DIR}/varying.def.sc
        -i ${CMAKE_SOURCE_DIR}/shaders -i ${CMAKE_SOURCE_DIR}/external/bgfx/src
    COMMAND ${CMAKE_SOURCE_DIR}/tools/shaderc.exe
        -f ${IMGUI_SHADER_DIR}/fs_ocornut_imgui.sc
        -o ${IMGUI_SHADER_OUT}/fs_ocornut_imgui.bin
        --type fragment --platform windows --profile s_5_0
        --varyingdef ${IMGUI_SHADER_DIR}/varying.def.sc
        -i ${CMAKE_SOURCE_DIR}/shaders -i ${CMAKE_SOURCE_DIR}/external/bgfx/src
    DEPENDS ${IMGUI_SHADER_DIR}/vs_ocornut_imgui.sc ${IMGUI_SHADER_DIR}/fs_ocornut_imgui.sc
)
add_dependencies(Claymore compile_imgui_shaders)

# ============================
# POST-BUILD STEPS
# ============================
add_custom_command(TARGET Claymore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:Claymore>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${IMGUI_SHADER_OUT} $<TARGET_FILE_DIR:Claymore>/shaders/imgui
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Claymore>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/external/dotnet_hosting/nethost.dll $<TARGET_FILE_DIR:Claymore>
)

set(CLAYMORE_ENGINE_DLL_DIR ${CMAKE_SOURCE_DIR}/scripts/ClaymoreEngine/bin/Debug/net8.0)

add_custom_command(TARGET Claymore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CLAYMORE_ENGINE_DLL_DIR}
        $<TARGET_FILE_DIR:Claymore>
)

# Root CMakeLists.txt or relevant subdir
file(GLOB TOOL_FILES tools/*)

foreach(TOOL_FILE ${TOOL_FILES})
    get_filename_component(FILENAME ${TOOL_FILE} NAME)
    configure_file(${TOOL_FILE} ${CMAKE_BINARY_DIR}/tools/${FILENAME} COPYONLY)
endforeach()


# ============================
# WINDOWS SUBSYSTEM SETTINGS
# ============================
if (MSVC)
    set_target_properties(Claymore PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
    )
endif()
