#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

namespace jzlog
{
template < class _Ty >
class CThreadSafeQueue {
public:
    using value_type      = _Ty;
    using reference       = _Ty&;
    using const_reference = const _Ty&;
    using pointer         = _Ty*;
    using const_pointer   = const _Ty*;

public:
    CThreadSafeQueue()                                     = default;
    CThreadSafeQueue( const CThreadSafeQueue& )            = delete;
    CThreadSafeQueue& operator=( const CThreadSafeQueue& ) = delete;
    CThreadSafeQueue( CThreadSafeQueue&& _Oth )            = delete;
    CThreadSafeQueue& operator=( CThreadSafeQueue&& _Oth ) = delete;
    ~CThreadSafeQueue()                                    = default;

    void push( const value_type& _val ) {
        {
            std::lock_guard< std::mutex > guard( _mutex );
            _data.emplace( _val );
        }
        // std::cout << "notify_one" << std::endl;

        notify_one();
    }
    void push( value_type&& _val ) {
        {
            std::lock_guard< std::mutex > guard( _mutex );
            _data.emplace( std::move( _val ) );
        }
        // std::cout << "notify_one" << std::endl;

        notify_one();
    }
    value_type pop() {
        std::unique_lock< std::mutex > ulock( _mutex );
        _cond.wait( ulock, [ this ]() {
            return !_data.empty();
        } );

        auto value = _data.front();
        _data.pop();
        return value;
    }
    bool try_pop( reference _val ) {
        std::lock_guard< std::mutex > guard{ _mutex };
        if ( _data.empty() ) {
            return false;
        }
        _val = _data.front();
        _data.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard< std::mutex > guard( _mutex );
        return _data.empty();
    }
    size_t size() const {
        std::lock_guard< std::mutex > guard( _mutex );
        return _data.size();
    }
    std::queue< value_type > get_data() {
        std::unique_lock< std::mutex > ulock( _mutex );
        _cond.wait( ulock, [ this ]() {
            return !_data.empty();
        } );
        std::queue< value_type > new_queue;
        new_queue.swap( _data );
        return new_queue;
    }

    inline void notify_one() const noexcept { _cond.notify_one(); }
    inline void notify_all() const noexcept { _cond.notify_all(); }

    std::condition_variable& get_cond() { return _cond; }
    std::mutex&              get_lock() { return _mutex; }

private:
    mutable std::mutex              _mutex;
    std::queue< value_type >        _data;
    mutable std::condition_variable _cond;
};
}  // namespace jzlog
