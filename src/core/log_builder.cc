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
    CLogBuilderImpl()                                         = default;
    CLogBuilderImpl( const CLogBuilderImpl& _oth )            = delete;
    CLogBuilderImpl& operator=( const CLogBuilderImpl& _oth ) = delete;
    CLogBuilderImpl( CLogBuilderImpl&& _oth )                 = default;
    CLogBuilderImpl& operator=( CLogBuilderImpl&& _oth )      = default;
    ~CLogBuilderImpl() {}

public:
    explicit CLogBuilderImpl( LogLevel _lvl, std::string_view _logger_name ) noexcept :
        _record( _lvl, _logger_name ) {}

    template < class... _Args >
    inline void set_message( std::string_view _msg ) {
        _record._message = _msg;
    }

    inline void set_location( const SourceLocation& _location ) noexcept {
        _record._loaction = _location;
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

CLogBuilder::CLogBuilder( LogLevel _lvl, std::string_view _logger_name ) :
    _impl( std::make_unique< CLogBuilderImpl >( _lvl, _logger_name ) ) {}

CLogBuilder::~CLogBuilder()                               = default;

CLogBuilder::CLogBuilder( CLogBuilder&& _oth )            = default;

CLogBuilder& CLogBuilder::operator=( CLogBuilder&& _oth ) = default;

void CLogBuilder::set_message( std::string_view _msg ) { this->_impl->set_message( _msg ); }

void CLogBuilder::set_location( const SourceLocation& _location ) noexcept {
    this->_impl->set_location( _location );
}

LogRecord CLogBuilder::build() { return this->_impl->build(); }

std::string CLogBuilder::to_string() const noexcept { return this->_impl->to_string(); }
}  // namespace jzlog
