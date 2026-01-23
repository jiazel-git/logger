#pragma once
#include "log_level.h"
#include "log_record.h"

#include <memory>
#include <string>
#include <string_view>

namespace jzlog
{
class CLogBuilder {
public:
    CLogBuilder();
    ~CLogBuilder();
    CLogBuilder( CLogBuilder&& _oth );
    CLogBuilder& operator=( CLogBuilder&& _oth );

public:
    explicit CLogBuilder( LogLevel _level, std::string_view _logger_name );
    void set_message( std::string_view _msg );

    void        set_location( const SourceLocation& _location ) noexcept;
    std::string to_string() const noexcept;
    LogRecord   build();

private:
    class CLogBuilderImpl;
    std::unique_ptr< CLogBuilderImpl > _impl;
};
}  // namespace jzlog
