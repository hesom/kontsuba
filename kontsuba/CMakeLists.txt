
# build core library
add_library(kontsuba_core INTERFACE)

# build the CLI Tool
add_executable(kontsuba
    app/kontsuba.cpp
    app/exporter.cpp
)
set_property(TARGET kontsuba PROPERTY CXX_STANDARD 17)

target_link_libraries(kontsuba
    PRIVATE kontsuba_core
    PRIVATE assimp
    PRIVATE tinyxml2
    PRIVATE tinyply
)