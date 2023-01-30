
# Configure and build all dependencies
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable assimp install" FORCE)
set(ASSIMP_SHARED_LIBS OFF CACHE BOOL "Build static libs" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable assimp tests" FORCE)
add_subdirectory(assimp)

add_subdirectory(tinyxml2)

add_subdirectory(tinyply)