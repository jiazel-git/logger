#pragma once
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include "jzlog/utils/log_buffer.h"
#include "sink.h"
#include <cstdint>
#include <fstream>
#include <string>
namespace jzlog
{
namespace sinks
{
using namespace loglevel;
class CFileSink final : public ISink {

public:
    using _M_Base = ISink;

public:
    explicit CFileSink() noexcept;
    explicit CFileSink( LogLevel _lvl, uint32_t _fsize, uint32_t _buf_size, std::string _dir,
                        std::string _fname, bool _enable ) noexcept;
    CFileSink( const CFileSink& )            = delete;
    CFileSink& operator=( const CFileSink& ) = delete;
    CFileSink( CFileSink&& )                 = delete;
    CFileSink& operator=( CFileSink&& )      = delete;

    void write( const LogRecord& _r ) override;

    void flush() override;

    void set_level( LogLevel _lvl ) noexcept override;

    LogLevel level() const noexcept override;

    bool should_log( LogLevel _lvl ) const noexcept override;

    void set_enabled( bool _enable ) noexcept override;

    bool enabled() const noexcept override;

    ~CFileSink();

private:
    void create_buffers();

    void create_new_file();

    std::string format_log_recore( LogRecord _r );

    void flush_buffer_to_file();

    void rotate_file();

private:
    LogLevel      _level;
    uint32_t      _file_size;
    uint32_t      _buffer_size;
    std::string   _file_path;
    std::string   _cur_file_name;
    uint32_t      _cur_file_size;
    bool          _enabled;
    Buffer        _buffer;
    std::ofstream _file_stream;
};
}  // namespace sinks
}  // namespace jzlog
