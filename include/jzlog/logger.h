#pragma once

#include <memory>
#include <string_view>
namespace jzlog
{
class CLogger {
public:
    CLogger();
    CLogger( CLogger&& _oth )            = delete;
    CLogger& operator=( CLogger&& _oth ) = delete;
    ~CLogger();

public:
    template < class... _Args >
    void info( std::string_view _fmt, _Args&&... _args ) const noexcept;
    template < class... _Args >
    void trace( std::string_view _fmt, _Args&&... _args ) const noexcept;
    template < class... _Args >
    void debug( std::string_view _fmt, _Args&&... _args ) const noexcept;
    template < class... _Args >
    void warn( std::string_view _fmt, _Args&&... _args ) const noexcept;
    template < class... _Args >
    void error( std::string_view _fmt, _Args&&... _args ) const noexcept;
    template < class... _Args >
    void fatal( std::string_view _fmt, _Args&&... _args ) const noexcept;

private:
    class CLoggerImpl;
    std::unique_ptr< CLoggerImpl > _impl;
};
}  // namespace jzlog
