cmake_minimum_required(VERSION 3.19)
project(QtFetch LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql)

qt_standard_project_setup()
add_subdirectory(download_tool)

qt_add_executable(QtFetch
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    sqlite/sqlwriter.h sqlite/sqlwriter.cpp
    sqlite/sqlreader.h sqlite/sqlreader.cpp
    sqlite/taskrecord.h
    sqlite/sqlcommon.h
)

target_link_libraries(QtFetch
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Sql
        download_class
)

target_include_directories(QtFetch PRIVATE download_tool/include)

install(TARGETS QtFetch
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET QtFetch
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
