cmake_minimum_required(VERSION 2.8)

project(HypergraphGUI)

include_directories(src)
include_directories(include)

# they are rebuilt completely from scratch inside our build-tree
# each subdir is a "project", so some standard variables are defined therein (like protocol_SOURCE_DIR for example)
# but we ourself may be add_subdirectory'ed, with some of the targets already present. so check this!

# This is not possible without mars cmake file :/ So we have to compile this on our own
if(NOT TARGET hypergraph)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib/hypergraph hypergraph)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../lib/hypergraph/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../lib/hypergraph/src)
endif(NOT TARGET hypergraph)

SET(ENV{PKG_CONFIG_PATH}
    ${CMAKE_BINARY_DIR}:$ENV{PKG_CONFIG_PATH}
    )

find_package(Qt4 REQUIRED)
set(QT_USE_QTOPENGL TRUE)
include(${QT_USE_FILE})

# needed to get the correct qwt stuff
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
# needed for qt's generated headers (ui files)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
#find_package(Qwt REQUIRED)
#include_directories(${QWT_INCLUDE_DIR})

find_package(PkgConfig)
pkg_check_modules(${PROJECT_NAME}_PKGCONFIG REQUIRED
    hypergraph
    )

include_directories(${${PROJECT_NAME}_PKGCONFIG_INCLUDE_DIRS})
message(${${PROJECT_NAME}_PKGCONFIG_INCLUDE_DIRS})
link_directories(${${PROJECT_NAME}_PKGCONFIG_LIBRARY_DIRS})
add_definitions(${${PROJECT_NAME}_PKGCONFIG_CFLAGS_OTHER} -ggdb -Wall -pedantic -std=c++11)

set(SOURCES_gui
    src/main.cpp
    src/HypergraphGUI.cpp
    src/HypergraphViewer.cpp
    src/ConceptgraphViewer.cpp
    src/CommonConceptGraphViewer.cpp
    src/HypergraphControl.cpp
    src/HyperedgeItem.cpp
    src/ConceptgraphItem.cpp
    src/CommonConceptGraphItem.cpp
    )
set(QT_HEADERS_gui # header which should be processed by moc
    include/HypergraphGUI.hpp
    include/HypergraphViewer.hpp
    include/ConceptgraphViewer.hpp
    include/CommonConceptGraphViewer.hpp
    include/HypergraphControl.hpp
    )
set(HEADERS_gui # normal cpp-headers, for installing
    include/HyperedgeItem.hpp
    include/ConceptgraphItem.hpp
    include/CommonConceptGraphItem.hpp
    )
set(FORMS_gui
    forms/HypergraphGUI.ui
    forms/HypergraphViewer.ui
    forms/CommonConceptGraphViewer.ui
    forms/HypergraphControl.ui
    )
set(RESOURCES_gui
#    widgets_plotter.qrc
    )

qt4_wrap_cpp(HEADERS_gui_MOC ${QT_HEADERS_gui})
qt4_wrap_ui(FORMS_gui_HEADERS ${FORMS_gui})
qt4_add_resources(RESOURCES_gui_RCC ${RESOURCES_gui})

add_executable(${PROJECT_NAME}
    ${SOURCES_gui}
    ${HEADERS_gui}
    ${QT_HEADERS_gui}
    ${HEADERS_gui_MOC}
    ${FORMS_gui_HEADERS}
    ${RESOURCES_gui_RCC}
    )

target_link_libraries(${PROJECT_NAME}
    ${QT_LIBRARIES}
    ${${PROJECT_NAME}_PKGCONFIG_LIBRARIES}
    )
