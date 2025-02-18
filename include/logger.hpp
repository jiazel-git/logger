/**
 * @description: logger.hpp
 * @Date       :2025/02/18 14:27:48
 * @Author     :lijiaze
 * @Version    :1.0
 * @Description:
 */
#ifndef LOGGER_H
#define LOGGER_H
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace jz
{
class logger
{
public:
    enum log_level : int
    {
        loggerNormal,
        loggerError,
        loggerWaring,
        loggerDebug
    };

private:
    struct _Bufferitem
    {
        log_level   _Level;
        std::string _Buffer;
    };

public:
    using _Buf_Vec = std::vector< _Bufferitem >;
    using _Buf_Ptr = std::unique_ptr< _Buf_Vec >;

public:
    logger( const std::string& _File )
        : _Fstream( _File, std::ios_base::out | std::ios_base::app ),
          _Thread( [ this ]() {
              for ( ;; ) {
                  if ( _Quit ) {
                      return;
                  }
                  std::unique_lock< std::mutex > _Ulock{ _Mutex };
                  _Cond.wait( _Ulock, [ this ]() { return !_Buffers->empty(); } );
                  _Buf_Ptr _Tmp( new _Buf_Vec );
                  std::swap( _Tmp, _Buffers );
                  _Ulock.unlock();
                  std::for_each( _Tmp->cbegin(), _Tmp->cend(), [ this ]( const auto& _Item ) {
                      std::string _Head = prepareHead( _Item._Level );
                      _Fstream.write( _Head.c_str(), _Head.size() );
                      _Fstream.write( _Item._Buffer.c_str(), _Item._Buffer.size() );
                      _Fstream.write( "\n", 1 );
                      _Fstream.flush();
                  } );
              }
          } ),
          _Quit( false ),
          _Buffers( new _Buf_Vec ) {}
    ~logger() {
        _Quit = true;
        if ( _Thread.joinable() ) {
            _Thread.join();
        }
    }
    template < typename... _Args >
    void add_log( log_level _Level, const char* fmt, _Args&&... _Arg ) {

        char _Buf[ 1024 ] = { 0 };
        sprintf( _Buf, fmt, std::forward< _Args >( _Arg )... );
        _Add_log( _Level, _Buf );
    }
    void add_log( log_level _Level, std::string&& add_log ) {
        _Add_log( _Level, std::move( add_log ) );
    }
    void add_log( log_level _Level, std::string& add_log ) {
        _Add_log( _Level, add_log );
    }

private:
    template < typename... _Args >
    void _Add_log( _Args&&... _Arg ) {
        {
            std::lock_guard< std::mutex > _Ulock( _Mutex );
            _Buffers->emplace_back( std::forward< _Args >( _Arg )... );
        }
        _Cond.notify_one();
    }

public:
    std::string prepareHead( int _Level ) {
        std::string _Head;

        auto    now      = std::chrono::system_clock::now();
        auto    time_now = std::chrono::system_clock::to_time_t( now );
        std::tm time;
        localtime_s( &time, &time_now );
        std::stringstream ss;
        ss.fill( '0' );
        ss << "[" << std::setw( 4 ) << 1900 + time.tm_year << std::setw( 2 ) << 1 + time.tm_mon << std::setw( 2 ) << std::setw( 2 ) << time.tm_mday << "-" << time.tm_hour << ":" << std::setw( 2 )
           << time.tm_min << ":" << std::setw( 2 ) << time.tm_sec << "]"
           << "[" << _Logstr[ _Level ] << "]";
        ss >> _Head;

        return _Head;
    }

private:
    _Buf_Ptr                _Buffers{ nullptr };
    std::mutex              _Mutex{};
    std::condition_variable _Cond{};
    std::thread             _Thread{};
    std::atomic< bool >     _Quit{};
    std::ofstream           _Fstream{};

private:
    inline static std::vector< std::string > _Logstr{ "NORMAL", "ERROR", "WARNING", "DEBUG" };
};
}  // namespace jz
#endif