# QtFetch

基于 Qt6 的多线程分块下载器。图形界面使用 Qt Widgets 编写，底层是一个独立的 C++20 下载引擎（`download_tool`），两者通过任务接口衔接。

## 功能特性

- **图形化界面**
  - 下载参数配置：下载链接、保存文件名、保存目录、单任务下载线程数
  - 任务列表：任务 ID、下载链接、实时进度条、暂停 / 继续、取消
  - 导航栏：全部开始、全部暂停、清空已完成、同时下载任务数设置
- **下载引擎（download_tool）**
  - 基于 POSIX socket 的 HTTP/1.1 客户端，自行解析响应头
  - HEAD 请求探测 `Accept-Ranges`，支持断点续传；服务器不支持时自动降级为单线程
  - `Range` 分块下载，多线程并行，`pwrite` 按偏移写入同一文件
  - 暂停 / 恢复 / 取消任务控制
  - C++20 协程任务包装与线程池调度

## 项目结构

```
QtFetch/
├── CMakeLists.txt              # 主程序构建脚本
├── main.cpp                    # 程序入口
├── mainwindow.h / .cpp         # 主窗口界面与交互逻辑
├── LICENSE                     # MIT 许可证
└── download_tool/              # 独立下载引擎（C++20）
    ├── CMakeLists.txt          # 引擎测试的构建脚本
    ├── include/
    │   ├── client.hpp          # 下载任务封装（Worker：解析 URL、探测、分片）
    │   ├── download_class.hpp  # 分块下载核心（连接、Range 请求、写文件）
    │   ├── http_parser.hpp     # HTTP 响应头解析
    │   ├── thread_pool.hpp     # 线程池
    │   └── coro.hpp            # C++20 协程任务包装
    ├── src/                    # 引擎源码（待迁移）
    └── test/                   # 单元测试
```

## 环境要求

- CMake ≥ 3.19
- Qt 6.5 及以上（Core、Widgets 模块）
- 支持 C++20 的编译器（GCC 10+ / Clang 10+ / MSVC 2019+）
- 引擎基于 POSIX socket，目前主要面向 Linux / macOS；Windows 需移植网络层

## 构建与运行

```bash
# 主程序
cmake -S . -B build
cmake --build build

# 运行
./build/QtFetch
```

引擎的独立测试：

```bash
cd download_tool
cmake -S . -B build
cmake --build build
```

## 使用说明

1. 在「下载参数」区填入下载链接，可选填保存文件名（留空则用链接中的文件名）；
2. 选择保存目录（默认为 `~/Downloads`），设置下载线程数；
3. 点击「开始下载」，任务出现在右侧任务列表，可随时暂停 / 继续 / 取消；
4. 通过顶部导航栏批量控制所有任务，并设置同时下载的任务数。

## 当前状态

- **已完成**：HTTP 头解析、线程池、`Range` 分块下载、响应头剥离后完整落盘、暂停 / 恢复。
- **进行中**：GUI 与引擎的调度层尚未接通，导航栏按钮（全部开始、全部暂停、清空已完成等）当前为界面骨架，点击仅显示状态栏提示。
- **未支持**：仅支持 HTTP（端口 80），HTTPS 需要补充 TLS 层。

## 许可证

本项目采用 [MIT License](LICENSE)。
