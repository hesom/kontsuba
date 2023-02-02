
# build core library
add_library(kontsuba_core STATIC
    core/converter.cpp
)
target_include_directories(kontsuba_core
    PUBLIC core/include
    PRIVATE core/include/kontsuba # shortcut for internal includes
)
target_link_libraries(kontsuba_core
    PRIVATE assimp
    PRIVATE tinyxml2
    PRIVATE tinyply
)
set_property(TARGET kontsuba_core PROPERTY CXX_STANDARD 17)

# build the CLI Tool
add_executable(kontsuba
    app/kontsuba.cpp
)
set_property(TARGET kontsuba PROPERTY CXX_STANDARD 17)

target_link_libraries(kontsuba
    PRIVATE kontsuba_core
)