#include "jzlog/core/log_builder.h"
#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
namespace jzlog
{
class CLogBuilder::CLogBuilderImpl {
public:
    CLogBuilderImpl()                                        = default;
    CLogBuilderImpl( const CLogBuilderImpl& oth )            = delete;
    CLogBuilderImpl& operator=( const CLogBuilderImpl& oth ) = delete;
    CLogBuilderImpl( CLogBuilderImpl&& oth )                 = default;
    CLogBuilderImpl& operator=( CLogBuilderImpl&& oth )      = default;
    ~CLogBuilderImpl() {}

public:
    explicit CLogBuilderImpl( LogLevel lvl, std::string_view logger_name ) noexcept :
        _record( lvl, logger_name ) {}

    template < class... Args >
    inline void set_message( std::string_view msg ) {
        _record._message = msg;
    }

    inline void set_location( const SourceLocation& location ) noexcept {
        _record._loaction = location;
    }

    LogRecord build() {
        _record._thread_id = std::this_thread::get_id();
        _record._timestamp = std::chrono::system_clock::now();
        return _record;
    }

    inline std::string to_string() const noexcept { return _record.to_string(); }

private:
    LogRecord _record;
};

CLogBuilder::CLogBuilder( LogLevel lvl, std::string_view logger_name ) :
    _impl( std::make_unique< CLogBuilderImpl >( lvl, logger_name ) ) {}

CLogBuilder::~CLogBuilder()                              = default;

CLogBuilder::CLogBuilder( CLogBuilder&& oth )            = default;

CLogBuilder& CLogBuilder::operator=( CLogBuilder&& oth ) = default;

void CLogBuilder::set_message( std::string_view msg ) { this->_impl->set_message( msg ); }

void CLogBuilder::set_location( const SourceLocation& location ) noexcept {
    this->_impl->set_location( location );
}

LogRecord CLogBuilder::build() { return this->_impl->build(); }

std::string CLogBuilder::to_string() const noexcept { return this->_impl->to_string(); }
}  // namespace jzlog
