#pragma once
#include <cstdint>
#include <memory>
#include <sys/types.h>
#include <utility>
namespace jzlog
{
struct Buffer {
    std::unique_ptr< char[] > _buf;
    uint32_t                  _size{ 0 };
    uint32_t                  _pos{ 0 };

    Buffer() = default;

    explicit Buffer( const uint32_t& _n ) :
        _buf( std::make_unique< char[] >( _n ) ), _size( 0 ), _pos( 0 ) {}

    explicit Buffer( const Buffer& _oth )   = delete;
    Buffer& operator=( const Buffer& _oth ) = delete;

    Buffer( Buffer&& _oth ) :
        _buf( std::move( _oth._buf ) ), _size( _oth._size ), _pos( _oth._pos ) {
        _oth._size = 0;
        _oth._pos  = 0;
    }
    Buffer& operator=( Buffer&& _oth ) {
        if ( this != &_oth ) {
            _buf       = std::move( _oth._buf );
            _size      = _oth._size;
            _pos       = _oth._pos;
            _oth._size = 0;
            _oth._pos  = 0;
        }
        return *this;
    }

    void clear_buf() noexcept {
        _pos  = 0;
        _size = 0;
    }
};
}  // namespace jzlog
