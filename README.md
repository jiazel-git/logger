## 高性能C++17日志库设计文档
1. 设计目标
1.1 核心需求
- 高性能: 零分配或最少分配，异步日志写入
- 线程安全: 多线程环境下安全使用
- 低延迟: 对主程序性能影响最小化
- 灵活性: 支持多种输出格式和目标
- 易用性: 简洁直观的API接口
1.2 性能指标
- 单条日志写入时间 < 100ns (无I/O)
- 内存分配次数最小化
- 支持每秒百万级日志写入
2. 架构设计
2.1 分层架构
```text
┌─────────────────┐
│    Logger API   │  ← 用户接口层
├─────────────────┤
│   Core Engine   │  ← 核心引擎层
├─────────────────┤
│     Sink        │  ← 输出层
├─────────────────┤
│  Async Buffer   │  ← 异步缓冲层
└─────────────────┘
```
2.2 核心组件
- Logger: 主接口，提供不同级别的日志方法
- Sink: 抽象输出目标（文件、控制台、网络等）
- Formatter: 日志格式器
- AsyncQueue: 异步消息队列
- RingBuffer: 环形缓冲区（无锁实现）
3. 详细设计
3.1 核心类设计
```cpp
// 日志级别枚举
enum class LogLevel : uint8_t {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};
// 日志记录结构
struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string_view logger_name;
    std::string_view message;
    std::source_location location;  // C++20特性，可选择性使用
};

// 抽象Sink接口
class ISink {
public:
    virtual ~ISink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

// 格式化器接口
class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual std::string format(const LogRecord& record) = 0;
};
```
3.2 高性能环形缓冲区
```cpp
template<typename T, size_t Capacity>
class RingBuffer {
private:
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    T buffer_[Capacity];
    
public:
    bool try_push(T&& item) noexcept;
    bool try_pop(T& item) noexcept;
    size_t size() const noexcept;
};
```
3.3 异步日志器
```cpp
class AsyncLogger {
private:
    std::vector<std::unique_ptr<ISink>> sinks_;
    std::unique_ptr<IFormatter> formatter_;
    RingBuffer<LogRecord, 1024 * 1024> ring_buffer_;
    std::atomic<bool> running_{true};
    std::thread worker_thread_;
    
    void worker_function();
    
public:
    AsyncLogger();
    ~AsyncLogger();
    
    void log(LogRecord&& record);
    void add_sink(std::unique_ptr<ISink> sink);
    void set_formatter(std::unique_ptr<IFormatter> formatter);
};
```
3.4 主Logger类
```cpp
class Logger {
private:
    std::string name_;
    LogLevel level_{LogLevel::INFO};
    AsyncLogger& async_logger_;
    
    template<typename... Args>
    void log_impl(LogLevel level, std::string_view fmt, Args&&... args);
    
public:
    Logger(std::string_view name, AsyncLogger& async_logger);
    
    template<typename... Args>
    void trace(std::string_view fmt, Args&&... args);
    
    template<typename... Args>
    void debug(std::string_view fmt, Args&&... args);
    
    template<typename... Args>
    void info(std::string_view fmt, Args&&... args);
    
    template<typename... Args>
    void warn(std::string_view fmt, Args&&... args);
    
    template<typename... Args>
    void error(std::string_view fmt, Args&&... args);
    
    template<typename... Args>
    void fatal(std::string_view fmt, Args&&... args);
    
    void set_level(LogLevel level) noexcept;
    LogLevel get_level() const noexcept;
};
```
4. 关键实现细节
4.1 零分配日志写入
````cpp
template<typename... Args>
void Logger::log_impl(LogLevel level, std::string_view fmt, Args&&... args) {
    if (level < level_) return;
    
    thread_local std::array<char, 4096> buffer;
    
    // 使用fmt库进行格式化（编译期格式字符串检查）
    auto size = fmt::format_to_n(buffer.data(), buffer.size() - 1, 
                                fmt, std::forward<Args>(args)...).size;
    buffer[size] = '\0';
    
    LogRecord record{
        .timestamp = std::chrono::system_clock::now(),
        .level = level,
        .logger_name = name_,
        .message = std::string_view(buffer.data(), size)
    };
    
    async_logger_.log(std::move(record));
}
````
4.2 高效时间戳获取
````cpp
class Timestamp {
private:
    static constexpr auto kClockType = 
        std::chrono::high_resolution_clock::is_steady 
            ? std::chrono::high_resolution_clock{}
            : std::chrono::steady_clock{};
    
public:
    static auto now() noexcept {
        return kClockType.now();
    }
    
    static std::string to_string(std::chrono::system_clock::time_point tp) {
        auto t = std::chrono::system_clock::to_time_t(tp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;
        
        std::tm tm_buf;
        localtime_r(&t, &tm_buf);  // 线程安全版本
        
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);
        
        char result[128];
        snprintf(result, sizeof(result), "%s.%03d", buffer, static_cast<int>(ms.count()));
        
        return result;
    }
};
````
4.3 文件Sink实现
````cpp
class FileSink : public ISink {
private:
    std::FILE* file_{nullptr};
    std::string filename_;
    std::mutex mutex_;
    size_t max_size_{1024 * 1024 * 100};  // 100MB
    int max_files_{10};
    
    void rotate_file();
    
public:
    explicit FileSink(std::string_view filename);
    ~FileSink() override;
    
    void write(const LogRecord& record) override;
    void flush() override;
    
    void set_max_size(size_t size) noexcept;
    void set_max_files(int count) noexcept;
};
````
5. 性能优化策略
5.1 内存池
````cpp
class MemoryPool {
private:
    struct Block {
        std::unique_ptr<char[]> data;
        size_t size;
    };
    
    std::vector<Block> blocks_;
    std::atomic<size_t> current_idx_{0};
    const size_t block_size_;
    
public:
    explicit MemoryPool(size_t block_size = 4096);
    
    char* allocate(size_t size);
    void reset() noexcept;
};
````
5.2 线程本地存储
````cpp
thread_local MemoryPool tls_pool(4096);

class ThreadLocalBuffer {
private:
    char* buffer_{nullptr};
    size_t capacity_{0};
    size_t size_{0};
    
public:
    ~ThreadLocalBuffer();
    
    void append(std::string_view str);
    void reset() noexcept;
    std::string_view view() const noexcept;
};
````
5.3 编译期字符串处理
````cpp
template<size_t N>
struct ConstexprString {
    char data[N] = {};
    constexpr ConstexprString(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = str[i];
        }
    }
};

template<ConstexprString Str>
constexpr auto format_string = [] {
    // 编译期格式字符串验证
    return std::string_view(Str.data);
};
````
6. 使用示例
````cpp
// 初始化
auto async_logger = std::make_unique<AsyncLogger>();
async_logger->add_sink(std::make_unique<FileSink>("app.log"));
async_logger->add_sink(std::make_unique<ConsoleSink>());

// 创建Logger
Logger logger("Main", *async_logger);
logger.set_level(LogLevel::DEBUG);

// 使用
logger.info("Application started");
logger.debug("User {} logged in from {}", user_id, ip_address);
logger.error("Failed to connect to database: {}", error_msg);

// 带位置的日志（C++20）
#define LOG_INFO(fmt, ...) \
    logger.info(fmt __VA_OPT__(,) __VA_ARGS__); \
    // 可以添加source_location支持
````
7. 配置文件格式（可选）
````yaml
logging:
  level: INFO
  pattern: "[%Y-%m-%d %H:%M:%S.%f] [%l] [%n] %v"
  sinks:
    - type: file
      filename: "logs/app.log"
      max_size: 104857600  # 100MB
      max_files: 10
    - type: console
      colorful: true
  async:
    buffer_size: 1048576  # 1MB
    flush_interval: 1000ms
````
8. 基准测试
8.1 测试指标
- 吞吐量测试: 单线程/多线程每秒日志数
- 延迟测试: 日志调用到写入完成的时间
- 内存测试: 内存分配次数和大小
- 并发测试: 高并发下的正确性和性能
8.2 性能对比
- 与现有流行日志库（如spdlog、glog）的性能对比
9. 扩展功能
9.1 支持的特性
- 构化日志: 支持JSON、XML格式输出
- 日志轮转: 基于时间/大小的文件轮转
- 日志过滤: 运行时动态过滤规则
- 远程日志: UDP/TCP日志发送
- 指标收集: 日志频率、延迟等监控
9.2 可选的第三方集成
- 系统日志: syslog/journald支持
- 分布式追踪: OpenTelemetry集成
- 监控系统: Prometheus指标导出
10. 构建和依赖
10.1 依赖
- C++17编译器
- fmt库（可选，用于更好的格式化）
- 无其他运行时依赖
10.2 构建系统
- 支持CMake、Bazel、Meson等多种构建系统
11. 部署考虑
- 11.1 平台支持
- Linux/Unix
- Windows
- macOS
- 嵌入式系统
11.2 二进制兼容性
- ABI稳定性保证
- 向后兼容性
- 这个设计文档提供了一个完整的高性能日志库架构。实现时建议采用渐进式开发，先实现核心功能，再逐步添加高级特性。关键点包括：零分配策略、无锁环形缓冲区、高效的格式化机制。
```text
hplog/                              # 项目根目录
├── CMakeLists.txt                  # 主CMake配置文件
├── README.md                       # 项目说明文档
├── LICENSE                         # 许可证文件
├── .gitignore                      # Git忽略文件
├── .clang-format                   # 代码格式化配置
├── .clang-tidy                     # 代码检查配置
├── cmake/                          # CMake模块目录
│   ├── FindFmt.cmake              # Fmt库查找模块（可选）
│   ├── CompilerWarnings.cmake     # 编译器警告设置
│   └── StandardSettings.cmake     # 标准设置
│
├── include/                        # 公共头文件
│   └── hplog/
│       ├── hplog.h                # 主头文件（包含所有必要头文件）
│       ├── version.h              # 版本信息
│       ├── logger.h               # Logger类
│       ├── async_logger.h         # AsyncLogger类
│       ├── sinks/                 # Sink接口和实现
│       │   ├── sink.h             # ISink接口
│       │   ├── file_sink.h        # 文件Sink
│       │   ├── console_sink.h     # 控制台Sink
│       │   ├── rotating_sink.h    # 轮转文件Sink
│       │   ├── null_sink.h        # 空Sink（用于测试）
│       │   └── syslog_sink.h      # 系统日志Sink
│       │
│       ├── core/                  # 核心组件
│       │   ├── log_record.h       # 日志记录结构
│       │   ├── log_level.h        # 日志级别定义
│       │   ├── formatter.h        # 格式化器接口
│       │   ├── pattern_formatter.h # 模式格式化器
│       │   ├── json_formatter.h   # JSON格式化器
│       │   ├── timestamp.h        # 时间戳工具
│       │   └── macros.h           # 宏定义
│       │
│       ├── utils/                 # 工具类
│       │   ├── ring_buffer.h      # 环形缓冲区
│       │   ├── memory_pool.h      # 内存池
│       │   ├── thread_local_buffer.h # 线程本地缓冲区
│       │   ├── string_utils.h     # 字符串工具
│       │   └── config.h           # 配置解析
│       │
│       └── registry.h             # Logger注册表（单例）
│
├── src/                           # 实现文件
│   ├── logger.cpp
│   ├── async_logger.cpp
│   ├── registry.cpp
│   │
│   ├── sinks/
│   │   ├── sink.cpp
│   │   ├── file_sink.cpp
│   │   ├── console_sink.cpp
│   │   ├── rotating_sink.cpp
│   │   └── syslog_sink.cpp
│   │
│   ├── core/
│   │   ├── formatter.cpp
│   │   ├── pattern_formatter.cpp
│   │   ├── json_formatter.cpp
│   │   └── timestamp.cpp
│   │
│   └── utils/
│       ├── ring_buffer.cpp
│       ├── memory_pool.cpp
│       └── config.cpp
│
├── tests/                         # 测试目录
│   ├── CMakeLists.txt            # 测试CMake配置
│   ├── unit_tests/               # 单元测试
│   │   ├── test_logger.cpp
│   │   ├── test_async_logger.cpp
│   │   ├── test_sinks.cpp
│   │   ├── test_formatter.cpp
│   │   ├── test_ring_buffer.cpp
│   │   └── test_memory_pool.cpp
│   │
│   ├── benchmark/                # 性能测试
│   │   ├── benchmark_throughput.cpp
│   │   ├── benchmark_latency.cpp
│   │   ├── benchmark_memory.cpp
│   │   └── compare_with_spdlog.cpp
│   │
│   └── integration_tests/        # 集成测试
│       ├── test_multithread.cpp
│       ├── test_file_rotation.cpp
│       └── test_configuration.cpp
│
├── examples/                      # 使用示例
│   ├── CMakeLists.txt
│   ├── basic_usage.cpp           # 基础使用示例
│   ├── advanced_usage.cpp        # 高级特性示例
│   ├── configuration_example.cpp # 配置示例
│   └── custom_sink_example.cpp   # 自定义Sink示例
│
├── benchmarks/                    # 基准测试套件
│   ├── CMakeLists.txt
│   ├── throughput_benchmark.cpp
│   └── latency_benchmark.cpp
│
├── docs/                          # 文档目录
│   ├── API.md                    # API参考
│   ├── GettingStarted.md         # 入门指南
│   ├── PerformanceGuide.md       # 性能指南
│   ├── Configuration.md          # 配置说明
│   ├── Customization.md          # 自定义扩展指南
│   └── DesignPrinciples.md       # 设计原则
│
├── scripts/                       # 构建和维护脚本
│   ├── format.sh                 # 代码格式化脚本
│   ├── lint.sh                   # 代码检查脚本
│   ├── build.sh                  # 构建脚本
│   ├── run_tests.sh              # 测试运行脚本
│   └── benchmark.sh              # 性能测试脚本
│
├── third_party/                   # 第三方依赖（可选）
│   └── fmt/                      # fmt库（如果作为子模块）
│
└── tools/                         # 开发工具
    ├── log_viewer/               # 日志查看器工具
    ├── config_generator/         # 配置生成器
    └── profiler/                 # 性能分析工具
````
