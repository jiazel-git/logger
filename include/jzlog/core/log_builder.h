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
    CLogBuilder( CLogBuilder&& oth );
    CLogBuilder& operator=( CLogBuilder&& oth );

public:
    explicit CLogBuilder( LogLevel level, std::string_view logger_name );
    void set_message( std::string_view msg );

    void        set_location( const SourceLocation& location ) noexcept;
    std::string to_string() const noexcept;
    LogRecord   build();

private:
    class CLogBuilderImpl;
    std::unique_ptr< CLogBuilderImpl > _impl;
};
}  // namespace jzlog
