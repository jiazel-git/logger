/**
 * @file fixed_buffer.h
 * @brief 固定大小缓冲区实现
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstring>

namespace jzlog
{
namespace utils
{

constexpr size_t kSmallBuffer = 1024;             // 小缓冲区大小
constexpr size_t kLargeBuffer = 4 * 1024 * 1024;  // 大缓冲区大小

/**
 * @class FixedBuffer
 * @brief 固定大小缓冲区模板类
 * @tparam _N 缓冲区大小
 */
template < size_t _N >
class FixedBuffer {
public:
    /**
     * @brief 构造函数
     */
    FixedBuffer() : _size( 0 ) {}

    /**
     * @brief 拷贝构造函数（已删除）
     */
    FixedBuffer( const FixedBuffer& ) = delete;

    /**
     * @brief 拷贝赋值运算符（已删除）
     */
    FixedBuffer& operator=( const FixedBuffer& ) = delete;

    /**
     * @brief 移动构造函数
     */
    FixedBuffer( FixedBuffer&& oth ) = default;

    /**
     * @brief 移动赋值运算符
     */
    FixedBuffer& operator=( FixedBuffer&& oth ) = default;

    /**
     * @brief 追加数据到缓冲区
     * @param buf 数据指针
     * @param len 数据长度
     */
    bool append( const char* buf, size_t len ) {
        if ( avail() < len || buf == nullptr || len == 0 ) {
            return false;
        }
        std::memcpy( _data.data() + _size, buf, len );
        _size += len;
        return true;
    }

    /**
     * @brief 获取缓冲区数据指针（常量版本）
     * @return 数据指针
     */
    [[nodiscard]] const char* data() const { return _data.data(); }

    /**
     * @brief 获取缓冲区数据指针
     * @return 数据指针
     */
    [[nodiscard]] char* data() { return _data.data(); }

    /**
     * @brief 获取缓冲区已用大小
     * @return 已用大小
     */
    size_t length() const noexcept { return _size; }

    /**
     * @brief 获取缓冲区可用大小
     * @return 可用大小
     */
    size_t avail() const noexcept { return _N - _size; }

    /**
     * @brief 追加单个字符到缓冲区
     * @param c 字符
     */
    bool append( char c ) {
        if ( avail() < 1 ) {
            return false;
        }
        _data[ _size++ ] = c;
        return true;
    }

    /**
     * @brief 重置缓冲区
     */
    void reset() noexcept { _size = 0; }

private:
    std::array< char, _N > _data;  // 数据数组
    size_t                 _size;  // 已用大小
};

}  // namespace utils
}  // namespace jzlog
