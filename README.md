# jzlog

一个高性能的 C++17 日志库，专注于零拷贝、异步写入和自动归档管理。

## 特性

- **零拷贝设计** - 双缓冲区技术，写入路径无锁
- **异步写入** - 后台线程处理磁盘 I/O，不阻塞主线程
- **自动归档** - 支持定时打包、压缩和过期清理
- **线程安全** - 多线程环境下安全使用
- **文件轮转** - 按大小自动切割日志文件
- **格式化支持** - 类 printf 的格式化接口

## 架构设计

### 整体架构

```
┌───────────────────────────────────────────────────────────────┐
│                         用户代码                                │
│  logger.info("Hello %s", "world");                             │
└────────────────────────┬──────────────────────────────────────┘
                         │
                         ▼
┌───────────────────────────────────────────────────────────────┐
│                      CLogger (前端接口)                        │
│  - info/debug/warn/error/fatal                                │
│  - 格式化消息                                                   │
│  - 构造 LogRecord                                              │
└────────────────────────┬──────────────────────────────────────┘
                         │
                         ▼
┌───────────────────────────────────────────────────────────────┐
│                    CLoggerImpl (异步引擎)                      │
│  - 工作线程                                                     │
│  - 线程安全队列 (thread_safe_queue)                            │
│  - 多 Sink 管理                                                │
└────────────────────────┬──────────────────────────────────────┘
                         │
                         ▼
┌───────────────────────────────────────────────────────────────┐
│                     ISink (输出目标接口)                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐    │
│  │  CFileSink  │  │ ConsoleSink │  │   (自定义 Sink)      │    │
│  │             │  │  (未来扩展)  │  │                      │    │
│  │ - 双缓冲区   │  │             │  │                      │    │
│  │ - 文件轮转   │  │             │  │                      │    │
│  │ - 自动归档   │  │             │  │                      │    │
│  └─────────────┘  └─────────────┘  └─────────────────────┘    │
└───────────────────────────────────────────────────────────────┘
```

### 日志写入流程

```
用户线程                    工作线程                         磁盘
   │                          │                              │
   │ logger.info("msg")        │                              │
   │     │                     │                              │
   │     ▼                     │                              │
   │ 格式化消息                │                              │
   │ 构造 LogRecord            │                              │
   │     │                     │                              │
   │     ▼                     │                              │
   │ push 到队列  ───────────▶  pop LogRecord                 │
   │ (无锁/微秒级)              │                              │
   │                           │                              │
   │ 返回 (不阻塞)              │                              │
   │                           ▼                              │
   │                      遍历 Sinks                          │
   │                           │                              │
   │                           ▼                              │
   │                      sink.write()                        │
   │                           │                              │
   │                           ▼                              │
   │                      写入缓冲区   ──────────────────────▶  I/O
   │                           │                              │
   │                           ▼                              │
   │                      双缓冲区交换                          │
   │                           │                              │
   │                           └─────────────────────────────▶  刷盘
```

### 双缓冲区工作原理

```
缓冲区 A (4KB)          缓冲区 B (4KB)
┌──────────────┐       ┌──────────────┐
│  写入缓冲区   │       │  读取缓冲区   │
│              │       │              │
│ [msg1]       │       │ [msg5]       │
│ [msg2]       │       │ [msg6]       │
│ [msg3]  ◀───写入───  │              │
│ [msg4]       │       │              │
└──────────────┘       └──────┬───────┘
                              │
                       flush() 时交换
                              │
                              ▼
                         写入磁盘
```

**写入路径** (用户线程):
```cpp
// 完全无锁，只访问写缓冲区
_buffers[_write_buf_0 ? 0 : 1].push(msg);
```

**刷新路径** (工作线程):
```cpp
// 加锁保护交换
{
    std::lock_guard lock(_swap_mutex);
    _write_buf_0 = !_write_buf_0;  // O(1) 翻转索引
}
// 写入读缓冲区到磁盘
_buffers[_write_buf_0 ? 1 : 0].flush_to_file();
```

### 归档管理流程

```
当前日志目录               归档目录                  压缩目录
current/                  archived/                 compressed/
├── app_20250129_*.log    ├── 20250129/             ├── logs_20250129.tar.zst
└── app_20250130_*.log    │   ├── app_*.log         └── logs_20250128.tar.zst
                          │   └── ...
                          └── 20250128/

定时触发 (默认凌晨 2 点)
    │
    ▼
1. 移动前一天日志到 archived/YYYYMMDD/
    │
    ▼
2. 打包成 tar/logs_YYYYMMDD.tar
    │
    ▼
3. 当 tar 目录超过阈值 (100MB)
    │  使用 zstd 压缩
    │  删除原始 tar 和 archived/ 日志
    │
    ▼
4. 删除超过保留天数 (30天) 的文件
```

## 目录结构

```
logger/
├── CMakeLists.txt              # 构建配置
├── README.md                   # 项目文档
│
├── include/jzlog/              # 公共头文件
│   ├── logger.hpp              # 主日志器接口
│   ├── logger_impl.hpp         # 异步实现
│   │
│   ├── core/                   # 核心组件
│   │   ├── log_level.h         # 日志级别定义
│   │   ├── log_record.h        # 日志记录结构
│   │   └── log_builder.h       # 日志构建器
│   │
│   ├── sinks/                  # 输出目标
│   │   ├── sink.h              # Sink 接口
│   │   └── file_sink.h         # 文件 Sink (双缓冲区 + 归档)
│   │
│   ├── archive_manager/        # 归档管理器
│   │   └── archive_manager.h   # 自动归档、压缩、清理
│   │
│   └── utils/                  # 工具类
│       ├── log_buffer.h        # 缓冲区结构
│       └── thread_safe_queue.hpp # 线程安全队列
│
├── src/                        # 实现文件
│   ├── core/
│   │   └── log_builder.cc
│   ├── sinks/
│   │   └── file_sink.cc
│   └── archive_manager/
│       └── archive_manager.cc
│
├── tests/                      # 测试代码
│   ├── test_log.cc
│   ├── test_log_level.cc
│   └── test_log_record.cc
│
└── examples/                   # 使用示例
    └── basic_usage.cc
```

## 快速开始

### 编译

```bash
# 创建构建目录
mkdir build && cd build

# 配置并编译
cmake ..
make -j$(nproc)

# 运行测试
./bin/test_log
./bin/test_log_level
./bin/test_log_record

# 运行示例
./bin/basic_usage
```

### 基本使用

```cpp
#include "jzlog/logger.hpp"
#include "jzlog/sinks/file_sink.h"

int main() {
    // 创建日志器
    jzlog::CLogger logger;

    // 添加文件输出
    logger.add_sink(std::make_unique<jzlog::sinks::CFileSink>());

    // 记录日志
    logger.info("应用程序启动");
    logger.debug("调试信息");
    logger.warn("警告信息");
    logger.error("错误信息");

    // 格式化输出
    int count = 100;
    logger.info("处理了 %d 条记录", count);

    return 0;
}
```

### 启用自动归档

```cpp
#include "jzlog/logger.hpp"
#include "jzlog/sinks/file_sink.h"
#include "jzlog/archive_manager/archive_manager.h"

int main() {
    // 配置归档参数
    jzlog::sinks::ArchiveConfig config;
    config.base_path = "./log";              // 日志根目录
    config.pack_hour = 2;                    // 凌晨 2 点执行
    config.pack_minute = 0;
    config.compress_threshold = 100 * 1024 * 1024;  // 100MB 触发压缩
    config.retention_days = 30;              // 保留 30 天

    // 创建带归档功能的 FileSink
    jzlog::CLogger logger;
    logger.add_sink(
        std::make_unique<jzlog::sinks::CFileSink>(
            jzlog::LogLevel::INFO,
            10 * 1024 * 1024,  // 单文件最大 10MB
            4 * 1024,           // 缓冲区 4KB
            "./log",
            true,
            config              // 归档配置
        )
    );

    logger.info("日志已启用自动归档");
    return 0;
}
```

## 性能特点

### 双缓冲区优势

- **写入无锁** - 用户线程只访问写缓冲区，无需加锁
- **O(1) 交换** - flush 时只翻转索引，不复制数据
- **零阻塞** - 写入不被磁盘 I/O 阻塞

### 性能指标

- **单条日志延迟** < 100ns (不含 I/O)
- **内存占用** 最小化，仅 8KB 双缓冲区
- **支持高并发** - 多线程安全写入

### 缓冲区配置

```cpp
// 默认配置
_buffer_size = 4 * 1024;     // 每个缓冲区 4KB
_buffers[0] + _buffers[1]    // 总共 8KB

// 优势
- 4KB 足够缓存约 40 条日志
- flush 时间通常 < 1ms
- 1ms 内不太可能写满 4KB
```

## 日志级别

```cpp
enum class LogLevel : uint8_t {
    TRACE = 0,   // 最详细的跟踪信息
    DEBUG = 1,   // 调试信息
    INFO  = 2,   // 一般信息
    WARN  = 3,   // 警告信息
    ERROR = 4,   // 错误信息
    FATAL = 5,   // 致命错误
    OFF   = 6    // 关闭日志
};
```

## 归档功能

### 自动归档

- **每日打包** - 凌晨 2 点自动打包前一天日志
- **二级压缩** - tar 文件超过 100MB 自动使用 zstd 压缩
- **过期清理** - 自动删除超过 30 天的归档文件

### 目录结构

```
log/
├── current/              # 当前活跃日志
│   └── app_20250130_120000.log
├── archived/             # 按日期归档
│   ├── 20250129/
│   │   └── app_20250129_*.log
│   └── 20250128/
│       └── app_20250128_*.log
├── tar/                  # tar 打包文件
│   ├── logs_20250129.tar
│   └── logs_20250128.tar
└── compressed/           # zstd 压缩文件
    ├── logs_20250129.tar.zst
    └── logs_20250128.tar.zst
```

## 扩展开发

### 自定义 Sink

```cpp
#include "jzlog/sinks/sink.h"

class MyCustomSink : public jzlog::sinks::ISink {
public:
    void write(const jzlog::LogRecord& r) override {
        // 自定义写入逻辑
    }

    void flush() override {
        // 刷新逻辑
    }
};

// 使用
logger.add_sink(std::make_unique<MyCustomSink>());
```

## 依赖

- **C++17** - 编译器要求
- **pthread** - 线程支持
- **zstd** (可选) - 日志压缩

## 平台支持

- Linux
- macOS
- Windows (部分功能)

## 许可证

MIT License

## 作者

jzlog 开发团队
