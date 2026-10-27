cmake_minimum_required(VERSION 3.19)
project(QtFetch LANGUAGES CXX)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Sql)

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
    downloadsession.h downloadsession.cpp
)

target_link_libraries(QtFetch
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Sql
        download_class
)

target_include_directories(QtFetch PRIVATE download_tool/include)

# ---------------------------------------------------------------------------
#  Windows 运行时部署
#
#  1) POST_BUILD：编译完就把 Qt 运行时（DLL + platforms/ + sqldrivers/）部署到
#     QtFetch.exe 旁边，这样 build/<config>/ 里直接双击就能跑。
#     其中 sqldrivers/qsqlite.dll 必须存在，否则程序启动时会弹
#     「无法打开 / 创建数据库」然后退出。
#
#  2) InstallRequiredSystemLibraries：把 VC++ 运行时（vcruntime140.dll、
#     msvcp140.dll 等）纳入 install 规则，装到没有装 vcredist 的机器上也能跑。
#     注意不要改用 /MT 静态 CRT——Qt 的 DLL 是 /MD 编的，混用两套 CRT 有坑。
# ---------------------------------------------------------------------------
if(WIN32)
    if(TARGET Qt6::windeployqt)
        add_custom_command(TARGET QtFetch POST_BUILD
            COMMAND Qt6::windeployqt --no-translations --no-opengl-sw
                    "$<TARGET_FILE:QtFetch>"
            COMMENT "部署 Qt 运行时到 QtFetch.exe 旁边")
    endif()

    include(InstallRequiredSystemLibraries)
endif()

install(TARGETS QtFetch
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET QtFetch
    FILENAME_VARIABLE deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
