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
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

namespace jz
{
class logger {
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
    logger( const std::string& _Path, const std::string& _Filepre = "log", const std::size_t _Size = 1024 )
        :  //_Fstream( _Filepre, std::ios_base::out | std::ios_base::app ),
          _File_path( _Path ),
          _Cur_file_idx( 0 ),
          _Log_size( _Size ),
          _File_pre( _Filepre ),
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
                      std::string _Head = _Prepare_head( _Item._Level );
                      if ( !std::filesystem::exists( _File_path + "/" + _File ) ) {
                          auto _Tmp = _File_path + "/" + _File;
                          // std::cout << "file is not exists:" << _Tmp << std::endl;
                          _Create_file();
                      } else {
                          int s = _Log_size - std::filesystem::file_size( _File_path + "/" + _File ) - 1 - _Head.size() - _Item._Buffer.size();

                          std::cout << "file_size=" << std::filesystem::file_size( _File_path + "/" + _File ) << " " << s << std::endl;
                          bool _Is_need_create = ( _Log_size - std::filesystem::file_size( _File_path + "/" + _File ) - 1 - _Head.size() - _Item._Buffer.size() ) <= 0 ? true : false;
                          if ( _Is_need_create ) {
                              std::cout << "file_size==" << std::filesystem::file_size( _File_path + "/" + _File ) << std::endl;
                              //_Fstream.flush();
                              _Create_file();
                          }
                      }
                      _Fstream.open( _File_path + "/" + _File, std::ios_base::out | std::ios_base::app );
                      if ( _Fstream.is_open() ) {
                          _Fstream.write( _Head.c_str(), _Head.size() );
                          _Fstream.write( _Item._Buffer.c_str(), _Item._Buffer.size() );
                          _Fstream.write( "\n", 1 );
                          _Fstream.flush();
                          std::cout << "write " << _Item._Buffer.c_str() << " to " << _File << std::endl;
                      }
                  } );
              }
          } ),
          _Quit( false ),
          _Buffers( new _Buf_Vec ) {
        for ( auto& entry : std::filesystem::directory_iterator( _File_path ) ) {
            if ( entry.is_directory() || entry.file_size() == _Log_size ) {
                continue;
            }
            auto _Pos     = entry.path().string().find_last_of( "\\" );
            _File         = entry.path().string().substr( _Pos + 1 );
            _Pos          = _File.find_last_of( "_" );
            auto _Pos1    = _File.find( "." );
            _Cur_file_idx = std::atoi( _File.substr( _Pos + 1, _Pos1 - _Pos - 1 ).c_str() );
            std::cout << _File << " " << _Cur_file_idx << std::endl;
            break;
        }
    }
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

private:
    [[nodiscard]] std::string _Prepare_head( int _Level ) {
        std::string _Head;

        auto time = _Get_local_time();

        std::stringstream ss;
        ss.fill( '0' );
        ss << "[" << std::setw( 4 ) << 1900 + time.tm_year << std::setw( 2 ) << 1 + time.tm_mon << std::setw( 2 ) << std::setw( 2 ) << time.tm_mday << "-" << time.tm_hour << ":" << std::setw( 2 )
           << time.tm_min << ":" << std::setw( 2 ) << time.tm_sec << "]"
           << "[" << _Logstr[ _Level ] << "]";
        ss >> _Head;

        return _Head;
    }

    [[nodiscard]] std::tm _Get_local_time() {
        auto    _Now      = std::chrono::system_clock::now();
        auto    _Time_now = std::chrono::system_clock::to_time_t( _Now );
        std::tm _Time;
        localtime_s( &_Time, &_Time_now );
        return _Time;
    }

    void _Create_file() {
        std::cout << "create file" << std::endl;
        auto _Time = _Get_local_time();

        std::stringstream ss;
        ss.fill( '0' );
        ss << std::setw( 4 ) << 1900 + _Time.tm_year << std::setw( 2 ) << 1 + _Time.tm_mon << std::setw( 2 ) << std::setw( 2 ) << _Time.tm_mday;
        std::string _Cur_time{ ss.str() };
        ss.str( "" );
        ss.clear();
        // 是否需要切日 重置_Cur_file_idx
        if ( !_File.empty() ) {
            auto _Pos = _File.find_first_of( "_" );
            if ( _Pos == std::string::npos ) {
                throw std::logic_error( "file name is illegal" );
            }
            std::string _Old_time = _File.substr( _Pos + 1, 8 );

            if ( _Cur_time > _Old_time ) {
                _Cur_file_idx = 0;
            } else {
                ++_Cur_file_idx;
            }
        }

        ss << _File_pre << "_" << _Cur_time << "_" << _Cur_file_idx << ".log";
        _File = ss.str();
    }

private:
    _Buf_Ptr                _Buffers{ nullptr };
    std::mutex              _Mutex{};
    std::condition_variable _Cond{};
    std::thread             _Thread{};
    std::atomic< bool >     _Quit{};
    std::ofstream           _Fstream{};
    std::string             _File_path{};
    std::string             _File{};
    std::string             _File_pre{};
    std::size_t             _Cur_file_idx{};
    int                     _Log_size{};

private:
    inline static std::vector< std::string > _Logstr{ "NORMAL", "ERROR", "WARNING", "DEBUG" };
};
}  // namespace jz
#endif