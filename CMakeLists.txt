cmake_minimum_required(VERSION 3.5.0)
project(dwarfNG VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS "-static")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)
include_directories(third_party)

add_subdirectory(third_party/libdwarf)

aux_source_directory(src DIR_SRCS)
aux_source_directory(third_party/imgui IMGUI_SRC)

add_executable(dwarfNG ${DIR_SRCS} ${IMGUI_SRC})

target_link_libraries(dwarfNG PRIVATE libdwarf::dwarf-static d3d11.lib dxgi.lib d3dcompiler.lib dxguid.lib Dwmapi.lib)
