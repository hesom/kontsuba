
# Configure and build all dependencies
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable assimp install" FORCE)
set(BUILD_SHARED_LIBS  OFF CACHE BOOL "Build static libs" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable assimp tests" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Build assimp zlib" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(assimp)

add_subdirectory(tinyxml2)

add_subdirectory(tinyply)

add_subdirectory(fmt)

if(SKBUILD)
    add_subdirectory(nanobind)
endif()