cmake_minimum_required(VERSION 3.20)

project(download_tools CXX)

set(CMAKE_CXX_STANDARD 20)

# ---------------------------------------------------------------------------
#  测试目标开关
#
#  · 单独构建这个目录时（cmake -S download_tool -B <dir>）：默认 ON，测试照常可跑
#  · 作为 QtFetch 的子目录被 add_subdirectory 时：默认 OFF，
#    避免主程序构建顺带编译 4 个测试可执行文件
#
#  主程序里想一起编测试就显式打开：
#    cmake -S . -B build -DDOWNLOAD_TOOL_BUILD_TESTS=ON
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(_download_tests_default ON)
else()
  set(_download_tests_default OFF)
endif()

option(DOWNLOAD_TOOL_BUILD_TESTS
       "Build download_tool standalone test executables"
       ${_download_tests_default})

file(GLOB SRC ./src/*.cpp)

add_library(download_class ${SRC})
target_include_directories(download_class PRIVATE ./include)

if(WIN32)
  target_link_libraries(download_class PUBLIC ws2_32)
endif()

if(DOWNLOAD_TOOL_BUILD_TESTS)
  add_executable(test_find_servet ./test/find_socket_test.cpp)
  target_link_libraries(test_find_servet download_class)

  add_executable(test_client ./test/client_test.cpp)
  target_link_libraries(test_client download_class)

  add_executable(test_thread ./test/thread_test.cpp)
  target_link_libraries(test_thread download_class)

  add_executable(test_timer_wheel ./test/timer_wheel_test.cpp)
  target_link_libraries(test_timer_wheel download_class)
endif()
