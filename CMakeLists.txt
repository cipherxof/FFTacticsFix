cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mingw-toolchain.cmake" CACHE STRING "Toolchain file for MinGW")

project(FFTacticsFix VERSION 1.0 LANGUAGES CXX)

if(MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64 -static")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

add_library(FFTacticsFix SHARED
    FFTacticsFix/dllmain.cpp
    FFTacticsFix/logger.cpp
    FFTacticsFix/Memory.cpp
)

set(MINHOOK_BUILD_SHARED OFF CACHE BOOL "Build MinHook as a shared library" FORCE)
set(MINHOOK_BUILD_STATIC ON CACHE BOOL "Build MinHook as a static library" FORCE)
add_subdirectory(external/minhook)

target_include_directories(FFTacticsFix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/FFTacticsFix/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/external/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/minhook/include
)

target_compile_definitions(FFTacticsFix PRIVATE
    _WINDOWS
    _USRDLL
    FFTACTICSFIX_EXPORTS
    NDEBUG
)

target_compile_options(FFTacticsFix PRIVATE -fpermissive)

set_target_properties(FFTacticsFix PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    PREFIX ""
    OUTPUT_NAME "FFTacticsFix"
    SUFFIX ".asi"
)

target_link_libraries(FFTacticsFix PRIVATE
    shlwapi
    version
    minhook
)