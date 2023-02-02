
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
set_property(TARGET kontsuba_core PROPERTY POSITION_INDEPENDENT_CODE ON)

# build the CLI Tool
if(NOT SKBUILD)
add_executable(kontsuba
    app/kontsuba.cpp
)
set_property(TARGET kontsuba PROPERTY CXX_STANDARD 17)

target_link_libraries(kontsuba
    PRIVATE kontsuba_core
)
endif()

if(SKBUILD)
    # Build python bindings

    # Fix missing shared library name for cibuildwheel+windows+pypy3.9
    if (MSVC AND NOT PYTHON_LIBRARY AND (${PYTHON_VERSION_STRING} MATCHES "3.9."))
        get_filename_component(PYTHON_LIBRARY ${PYTHON_INCLUDE_DIR} DIRECTORY)
        set(PYTHON_LIBRARY "${PYTHON_LIBRARY}/libs/python39.lib")
    endif()

    set(Python_VERSION "${PYTHON_VERSION_STRING}")
    set(Python_EXECUTABLE "${PYTHON_EXECUTABLE}")
    set(Python_INCLUDE_DIR "${PYTHON_INCLUDE_DIR}")
    set(Python_LIBRARY "${PYTHON_LIBRARY}")

    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
        find_package(Python 3.8 COMPONENTS Interpreter Development.Module Development.SABIModule REQUIRED)
    else()
        find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)
    endif()
    
    nanobind_add_module(kontsuba_ext
        bindings/bindings.cpp NB_STATIC
    )

    target_link_libraries(kontsuba_ext
        PRIVATE kontsuba_core
    )
    set_property(TARGET kontsuba_ext PROPERTY POSITION_INDEPENDENT_CODE ON)

    install(TARGETS kontsuba_ext DESTINATION LIBRARY DESTINATION .)

endif()