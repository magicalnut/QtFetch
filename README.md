# QtFetch

基于 Qt6 的多线程分块下载器。图形界面使用 Qt Widgets 编写，底层是一个独立的 C++20 下载引擎（`download_tool`），两者之间由 `DownloadSession` 做适配，任务与完成记录持久化在 SQLite 中。

## 功能特性

### 界面

- **两列布局**（`QSplitter`，可拖动分隔）
  - 左列：下载参数 —— 下载链接、文件保存名（留空则取链接中的文件名）、保存目录、单任务下载线程数
  - 右列：任务列表，分为「下载中」和「已完成」两个页签
- **导航栏**：全部开始、全部暂停、清空已完成
- **任务列表**（下载中）：任务 ID、下载链接、实时进度条、开始 / 暂停 / 继续按钮、取消按钮
- **已完成列表**：下载链接、文件名、打开文件所在位置、删除记录

### 任务生命周期

```
创建任务（state=paused，不自动下载）
   └─ 点「开始」→ 下载中（state=downloading）→ 进度条实时更新
        ├─ 点「暂停」→ 暂停（state=paused）→ 点「继续」接着下
        ├─ 点「取消」→ 停下载、删任务记录、移除行
        └─ 下载完成 → 移出「下载中」，写入「已完成」
```

- 创建任务时是暂停态，必须手动点「开始」
- 完成后任务记录从 `tasks` 表搬到 `completed` 表（同一事务）
- 重启后未完成的任务会重新出现，按钮显示「重新下载」（会从 0 覆盖文件，见下方限制）

### 下载引擎（download_tool）

- 平台抽象层 `platform.hpp`：Windows（Winsock）与 Linux/macOS（POSIX socket）都可用
- HTTP/1.1 客户端，自行解析响应头
- HEAD 探测 `Accept-Ranges`，服务器不支持时自动降级为单线程
- `Range` 分块下载，线程池并行，按偏移写入同一文件
- 进度与速度查询接口（`get_progress()` / `get_speed()`）
- 暂停 / 恢复 / 停止控制

## 项目结构

```
QtFetch/
├── CMakeLists.txt              # 主程序构建脚本（含 add_subdirectory(download_tool)）
├── main.cpp                    # 程序入口
├── mainwindow.h / .cpp         # 主窗口界面与交互逻辑
├── downloadsession.h / .cpp    # 引擎适配层：QTimer 轮询进度 → Qt 信号
├── sqlite/                     # SQLite 持久化层
│   ├── sqlcommon.h             # 连接管理（打开 / 关闭 / PRAGMA），header-only
│   ├── sqlwriter.h / .cpp      # 写线程：建表、增删改、完成任务搬运
│   ├── sqlreader.h / .cpp      # 读线程：启动时一次性读回任务与完成记录
│   └── taskrecord.h            # 一行记录的内存表示
├── LICENSE                     # MIT 许可证
└── download_tool/              # 独立下载引擎（C++20）
    ├── CMakeLists.txt
    ├── include/
    │   ├── client.hpp          # 下载任务封装（Worker：解析 URL、探测、分片）
    │   ├── download_class.hpp  # 分块下载核心（连接、Range 请求、写文件）
    │   ├── http_parser.hpp     # HTTP 响应头解析
    │   ├── thread_pool.hpp     # 线程池
    │   ├── timer_wheel.hpp     # 时间轮（速度统计）
    │   ├── platform.hpp        # 平台抽象（socket / close / pwrite）
    │   └── coro.hpp            # C++20 协程任务包装（暂未使用）
    ├── src/timer_wheel.cpp     # 全局时间轮实例
    └── test/                   # 引擎单元测试
```

## 环境要求

- CMake ≥ 3.20
- Qt 6.4 及以上（**Core、Widgets、Sql** 三个模块）
- 主程序使用 C++17，引擎使用 C++20
- 已验证组合：
  - Windows 10/11 + MSVC 2022 + Qt 6.5.3（msvc2019_64）
  - Ubuntu 24.04 + GCC + Qt 6.4.2（系统包）

## 构建与运行

### 1. 获取代码

```bash
git clone https://github.com/magicalnut/QtFetch.git
cd QtFetch
```

下载引擎 `download_tool` 就在主仓库目录里，**不是 submodule**，不需要单独 clone。

### 2. 构建

Linux / macOS：

```bash
# 配置（CMAKE_PREFIX_PATH 指向你的 Qt 安装目录）
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5.3/gcc_64

# 编译
cmake --build build --config Release

# 运行
./build/QtFetch
```

Windows + MSVC：

```powershell
# 这里的 Qt 路径换成你自己的安装位置（Qt 安装器默认在 C:/Qt/<版本>/）
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/msvc2019_64

cmake --build build --config Release

.\build\Release\QtFetch.exe
```

> 用 Qt Creator 打开更省事：直接选 `CMakeLists.txt`，配置好 Kit 后会自动识别 Qt 路径，不需要手写 `CMAKE_PREFIX_PATH`。

### 引擎测试

测试目标默认只在**单独构建 `download_tool`** 时生成，作为主工程子目录时不会跟着编译：

```bash
cmake -S download_tool -B build/download_tests
cmake --build build/download_tests
```

需要主工程一起编测试时显式开启：

```bash
cmake -S . -B build -DDOWNLOAD_TOOL_BUILD_TESTS=ON
```

## 发布包使用说明

如果你拿到的是打包好的可执行程序（不需要自己编译），按下面两个小节操作。两个包都已内置 Qt 运行库和 SQLite 驱动插件，**目标机器不需要安装 Qt**。

### Windows

发布包：`QtFetch-windows-x64.zip`，或自解压版 `QtFetch-windows-x64-*.exe`

1. 解压 zip（或双击自解压版，选一个目录让它解压）
2. 双击目录里的 `QtFetch.exe`
3. 首次运行会自动创建数据库，位置在：
   ```
   C:\Users\<你的用户名>\AppData\Roaming\QtFetch\qtfetch.db
   ```

**注意**

- 如果 Windows Defender / 杀软提示「未知发布者」，选「仍要运行」即可（程序没有代码签名）。
- 系统要求：Windows 10 及以上（64 位）。

### Linux

发布包：`QtFetch-linux-x86_64.tar.gz`

```bash
tar -xzf QtFetch-linux-x86_64.tar.gz -C ~/QtFetch
cd ~/QtFetch/bin
./QtFetch
```

目录结构如下，**整个目录一起保留**：

```
bin/QtFetch          主程序（RPATH 已设为 $ORIGIN/../lib）
bin/qt.conf          告诉 Qt 插件目录在 ../plugins
lib/                 内置的 Qt 与 ICU 库
plugins/platforms/   libqxcb.so
plugins/sqldrivers/  libqsqlite.so
```

**注意**

- **系统要求**：Ubuntu 24.04 及以上（包内 Qt 由 glibc 2.39 环境编译，更老的发行版会报 `GLIBC_2.39 not found`）。
- **中文字体**：包里不含字体。系统缺少中文字体时界面会显示方框，安装即可：
  ```bash
  sudo apt install fonts-noto-cjk      # 或 fonts-wqy-microhei
  ```
- **桌面运行库**：`libqxcb.so` 依赖 `libxcb-cursor0`、`libxkbcommon-x11-0`、`libfontconfig1`、`libgl1`。桌面版 Ubuntu 默认都有；精简系统/容器里需要手动装。
- 不要用 `root` 运行图形程序，否则会出现 `XDG_RUNTIME_DIR ... not owned by UID 0` 的告警。用普通用户运行，数据库在：
  ```
  ~/.local/share/QtFetch/qtfetch.db
  ```
- 没有点击启动图标，需要从终端运行（或自己建一个 `.desktop` 文件）。

## 使用说明

1. 左列填入下载链接；保存文件名可留空（自动取链接最后一段）；选择保存目录、设置线程数
2. 点「创建任务」——任务出现在「下载中」页，此时**还没开始下载**
3. 点该行按钮「开始」，进度条开始走动
4. 下载中可「暂停 / 继续」，也可直接「取消」（删任务、移除行）
5. 下载完成后任务自动移到「已完成」页，可「打开位置」或「删除记录」
6. 导航栏可批量「全部开始」「全部暂停」，或「清空已完成」清空完成记录

### 本地测试

下载引擎目前只支持明文 HTTP，本地起一个测试服务最方便（注意主程序不解析 URL 里的端口，必须用 80）：

```bash
cd /some/dir && python -m http.server 80
# 然后在程序里填 http://127.0.0.1/测试文件.bin
```

`python -m http.server` 不支持 `Range`，引擎会自动降级为单线程下载；想测多线程分片可以用 `python -m RangeHTTPServer 80`。

## 数据存储

- 数据库位置：`QStandardPaths::AppDataLocation` + `/qtfetch.db`
  - Windows：`C:/Users/<用户>/AppData/Roaming/QtFetch/qtfetch.db`
  - Linux：`~/.local/share/QtFetch/qtfetch.db`
- WAL 模式，会同时存在 `qtfetch.db-wal` / `qtfetch.db-shm`
- 表结构：
  - `tasks`：未完成的任务（完成或取消即删除）
  - `completed`：已完成记录（可单条删除或整体清空；**只删记录，不删磁盘文件**）

## 已知限制

| 限制 | 说明 |
|---|---|
| **不支持 HTTPS** | 引擎只做明文 HTTP，且端口写死 80；URL 里的非标准端口解析不了 |
| **不支持断点续传** | 引擎用 `O_TRUNC` 打开文件且 `run()` 没有续传入口，重启后只能从 0 重下 |
| **没有并发上限** | 点几个就同时下几个，暂无任务队列 |
| **暂停/取消有延迟** | 引擎的 `recv` 没有超时，取消时靠后台线程销毁 Worker，文件句柄会晚几秒释放 |
| **多任务速度统计不准** | 引擎的 `update_speed_loop()` 是带 sleep 的死循环，挂在全局时间轮上，多任务时会互相阻塞（界面暂未显示速度） |
| **Linux 未实测** | 目标平台是 Linux，但当前只在 Windows/MSVC 上编译验证过 |

## 许可证

本项目采用 [MIT License](LICENSE)。

## 参与贡献

欢迎参与本项目，详情请见 [贡献指南](CONTRIBUTING.md)。
