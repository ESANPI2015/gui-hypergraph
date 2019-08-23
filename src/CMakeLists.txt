SET(ENV{PKG_CONFIG_PATH}
    ${CMAKE_BINARY_DIR}:$ENV{PKG_CONFIG_PATH}
    )

find_package(Qt4 REQUIRED)
set(QT_USE_QTOPENGL TRUE)
include(${QT_USE_FILE})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set(SOURCES_gui
    main.cpp
    HypergraphGUI.cpp
    HypergraphViewer.cpp
    ConceptgraphViewer.cpp
    CommonConceptGraphViewer.cpp
    HypergraphControl.cpp
    HyperedgeItem.cpp
    ConceptgraphItem.cpp
    CommonConceptGraphItem.cpp
    )
set(QT_HEADERS_gui # header which should be processed by moc
    ../include/HypergraphGUI.hpp
    ../include/HypergraphViewer.hpp
    ../include/ConceptgraphViewer.hpp
    ../include/CommonConceptGraphViewer.hpp
    ../include/HypergraphControl.hpp
    )
set(HEADERS_gui # normal cpp-headers, for installing
    ../include/HyperedgeItem.hpp
    ../include/ConceptgraphItem.hpp
    ../include/CommonConceptGraphItem.hpp
    )
set(FORMS_gui
    ../forms/HypergraphGUI.ui
    ../forms/HypergraphViewer.ui
    ../forms/CommonConceptGraphViewer.ui
    ../forms/HypergraphControl.ui
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
    componentnet
    )
