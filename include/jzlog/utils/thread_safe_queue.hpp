/**
 * @file thread_safe_queue.hpp
 * @brief 线程安全队列模板类
 */
#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

namespace jzlog
{

/**
 * @class CThreadSafeQueue
 * @brief 线程安全队列模板类
 * @tparam _Ty 元素类型
 */
template < class _Ty >
class CThreadSafeQueue {
public:
    using value_type      = _Ty;
    using reference       = _Ty&;
    using const_reference = const _Ty&;
    using pointer         = _Ty*;
    using const_pointer   = const _Ty*;

public:
    /**
     * @brief 默认构造函数
     */
    CThreadSafeQueue() = default;

    /**
     * @brief 拷贝构造函数（已删除）
     */
    CThreadSafeQueue( const CThreadSafeQueue& ) = delete;

    /**
     * @brief 拷贝赋值运算符（已删除）
     */
    CThreadSafeQueue& operator=( const CThreadSafeQueue& ) = delete;

    /**
     * @brief 移动构造函数（已删除）
     */
    CThreadSafeQueue( CThreadSafeQueue&& _Oth ) = delete;

    /**
     * @brief 移动赋值运算符（已删除）
     */
    CThreadSafeQueue& operator=( CThreadSafeQueue&& _Oth ) = delete;

    /**
     * @brief 析构函数
     */
    ~CThreadSafeQueue() = default;

    /**
     * @brief 推入元素到队列（拷贝版本）
     * @param _val 要推入的元素
     */
    void push( const value_type& _val ) {
        {
            std::lock_guard guard( _mutex );
            _data.emplace( _val );
        }
        notify_one();
    }

    /**
     * @brief 推入元素到队列（移动版本）
     * @param _val 要推入的元素
     */
    void push( value_type&& _val ) {
        {
            std::lock_guard guard( _mutex );
            _data.emplace( std::move( _val ) );
        }
        notify_one();
    }

    /**
     * @brief 从队列中弹出一个元素（阻塞等待）
     * @return 弹出的元素
     */
    [[nodiscard]] value_type pop() {
        std::unique_lock ulock( _mutex );
        _cond.wait( ulock, [ this ]() {
            return !_data.empty();
        } );

        auto value = _data.front();
        _data.pop();
        return value;
    }

    /**
     * @brief 尝试从队列中弹出一个元素（非阻塞）
     * @return 如果队列非空返回元素，否则返回 std::nullopt
     */
    [[nodiscard]] std::optional< value_type > try_pop() {
        std::lock_guard guard{ _mutex };
        if ( _data.empty() ) {
            return std::nullopt;
        }
        auto value = _data.front();
        _data.pop();
        return value;
    }

    /**
     * @brief 判断队列是否为空
     * @return 空返回 true，否则返回 false
     */
    bool empty() const {
        std::lock_guard guard( _mutex );
        return _data.empty();
    }

    /**
     * @brief 获取队列大小
     * @return 队列大小
     */
    size_t size() const {
        std::lock_guard guard( _mutex );
        return _data.size();
    }

    /**
     * @brief 获取队列中的所有数据并清空队列
     * @return 包含所有元素的队列
     */
    [[nodiscard]] std::queue< value_type > get_data() noexcept {
        std::unique_lock ulock( _mutex );
        _cond.wait( ulock, [ this ]() {
            return !_data.empty();
        } );
        std::queue< value_type > new_queue;
        new_queue.swap( _data );
        return new_queue;
    }

    /**
     * @brief 通知一个等待的线程
     */
    inline void notify_one() const noexcept { _cond.notify_one(); }

    /**
     * @brief 通知所有等待的线程
     */
    inline void notify_all() const noexcept { _cond.notify_all(); }

private:
    mutable std::mutex              _mutex;  // 互斥锁
    std::queue< value_type >        _data;   // 数据队列
    mutable std::condition_variable _cond;   // 条件变量
};
}  // namespace jzlog
