/**
 * @file fixed_buffer.h
 * @brief 固定大小缓冲区实现
 * @details 使用栈上固定数组实现零分配缓冲区，用于高性能异步日志系统
 * @author carbon
 * @date 2026-03-29
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstring>

namespace jzlog
{
namespace utils
{

/**
 * @brief 小缓冲区大小常量
 */
constexpr size_t kSmallBuffer = 1024;

/**
 * @brief 大缓冲区大小常量
 * @details 4MB (muduo标准大小)，减少缓冲区满触发频率，提供更好的批量I/O性能
 */
constexpr size_t kLargeBuffer = 4 * 1024 * 1024;

/**
 * @class FixedBuffer
 * @brief 固定大小缓冲区模板类
 * @tparam _N 缓冲区大小（字节）
 * @details 使用栈上固定数组实现零分配缓冲区，提供高性能的内存操作。
 *          该类禁止拷贝语义，支持移动语义，适用于高性能日志场景。
 *          当缓冲区空间不足时，append操作会静默失败（不抛出异常）。
 */
template < size_t _N >
class FixedBuffer {
public:
    /**
     * @brief 构造函数
     * @details 初始化空缓冲区，已用空间大小为0
     */
    FixedBuffer() : _size( 0 ) {}

    /**
     * @brief 禁用拷贝构造函数
     */
    FixedBuffer( const FixedBuffer& )            = delete;

    /**
     * @brief 禁用拷贝赋值运算符
     */
    FixedBuffer& operator=( const FixedBuffer& ) = delete;

    /**
     * @brief 移动构造函数（使用默认实现）
     */
    FixedBuffer( FixedBuffer&& oth )             = default;

    /**
     * @brief 移动赋值运算符（使用默认实现）
     */
    FixedBuffer& operator=( FixedBuffer&& oth )  = default;

    /**
     * @brief 追加数据到缓冲区
     * @param buf 数据源指针
     * @param len 要追加的字节数
     * @details 如果剩余空间不足、buf为nullptr或len为0，操作静默失败。
     *          该方法使用memcpy高效复制数据。
     * @note 该方法不抛出异常，空间不足时静默忽略
     */
    void append( const char* buf, size_t len ) {
        if ( avail() < len || buf == nullptr || len == 0 ) {
            return;
        }
        std::memcpy( _data.data() + _size, buf, len );
        _size += len;
    }

    /**
     * @brief 获取缓冲区数据指针（常量版本）
     * @return 指向缓冲区内部数据的常量指针
     */
    const char* data() const { return _data.data(); }

    /**
     * @brief 获取缓冲区数据指针（可修改版本）
     * @return 指向缓冲区内部数据的指针
     */
    char*       data() { return _data.data(); }

    /**
     * @brief 获取当前已用空间大小
     * @return 已写入的字节数
     */
    size_t length() const { return _size; }

    /**
     * @brief 获取剩余可用空间大小
     * @return 可写入的字节数（_N - _size）
     */
    size_t avail() const { return _N - _size; }

    /**
     * @brief 追加单个字符到缓冲区
     * @param c 要追加的字符
     * @details 如果剩余空间不足，操作静默失败
     * @note 该方法不抛出异常，空间不足时静默忽略
     */
    void append( char c ) {
        if ( avail() < 1 ) {
            return;
        }
        _data[ _size++ ] = c;
    }

    /**
     * @brief 重置缓冲区
     * @details 将已用空间大小设为0，缓冲区内容保留不变但被视为空
     */
    void reset() { _size = 0; }

private:
    /**
     * @brief 固定大小数组，存储实际数据
     */
    std::array< char, _N > _data;

    /**
     * @brief 当前已用空间大小
     */
    size_t                 _size;
};

}  // namespace utils
}  // namespace jzlog
