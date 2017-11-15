#pragma once
#include <atomic>
#include <exception>
#include <experimental/coroutine>
#include <functional>
#include <iterator>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <cassert>
#include <cstdint>

namespace net {

using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

class async {
public:
  struct promise_type {
    async get_return_object() noexcept {
      return {};
    }

    constexpr auto initial_suspend() noexcept {
      return suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return suspend_never{};
    }

    constexpr void return_void() noexcept {
    }

    void unhandled_exception() noexcept {
      std::rethrow_exception(std::current_exception());
    }
  };
};

// ============================================================================
// Everything below this line is from <https://github.com/lewissbaker/cppcoro>.
// ============================================================================
// Copyright 2017 Lewis Baker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ============================================================================

// == async_mutex =============================================================

class async_mutex_lock;
class async_mutex_lock_operation;
class async_mutex_scoped_lock_operation;

class async_mutex {
public:
  async_mutex() noexcept;
  ~async_mutex();

  bool try_lock() noexcept;
  async_mutex_lock_operation lock_async() noexcept;
  async_mutex_scoped_lock_operation scoped_lock_async() noexcept;
  void unlock();

private:
  friend class async_mutex_lock_operation;

  static constexpr std::uintptr_t not_locked = 1;
  static constexpr std::uintptr_t locked_no_waiters = 0;
  std::atomic<std::uintptr_t> m_state;
  async_mutex_lock_operation* m_waiters;
};

class async_mutex_lock {
public:
  explicit async_mutex_lock(async_mutex& mutex, std::adopt_lock_t) noexcept : m_mutex(&mutex) {
  }

  async_mutex_lock(async_mutex_lock&& other) noexcept : m_mutex(other.m_mutex) {
    other.m_mutex = nullptr;
  }

  async_mutex_lock(const async_mutex_lock& other) = delete;
  async_mutex_lock& operator=(const async_mutex_lock& other) = delete;

  ~async_mutex_lock() {
    if (m_mutex != nullptr) {
      m_mutex->unlock();
    }
  }

private:
  async_mutex* m_mutex;
};

class async_mutex_lock_operation {
public:
  explicit async_mutex_lock_operation(async_mutex& mutex) noexcept : m_mutex(mutex) {
  }

  bool await_ready() const noexcept {
    return false;
  }
  bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept;
  void await_resume() const noexcept {
  }

protected:
  friend class async_mutex;

  async_mutex& m_mutex;

private:
  async_mutex_lock_operation* m_next;
  std::experimental::coroutine_handle<> m_awaiter;
};

class async_mutex_scoped_lock_operation : public async_mutex_lock_operation {
public:
  using async_mutex_lock_operation::async_mutex_lock_operation;

  [[nodiscard]] async_mutex_lock await_resume() const noexcept {
    return async_mutex_lock{ m_mutex, std::adopt_lock };
  }
};

// == task ====================================================================

namespace detail {
class continuation {
public:
  using callback_t = void(void*);

  continuation() noexcept : m_callback(nullptr), m_state(nullptr) {
  }

  explicit continuation(std::experimental::coroutine_handle<> awaiter) noexcept :
    m_callback(nullptr), m_state(awaiter.address()) {
  }

  explicit continuation(callback_t* callback, void* state) noexcept : m_callback(callback), m_state(state) {
  }

  explicit operator bool() const noexcept {
    return m_callback != nullptr || m_state != nullptr;
  }

  void resume() noexcept {
    if (m_callback == nullptr) {
      std::experimental::coroutine_handle<>::from_address(m_state).resume();
    } else {
      m_callback(m_state);
    }
  }

private:
  callback_t* m_callback;
  void* m_state;
};

}  // namespace detail

class broken_promise : public std::logic_error {
public:
  broken_promise() : std::logic_error("broken promise") {
  }
};

template <typename T>
class task;

namespace detail {

class task_promise_base {
  friend struct final_awaitable;

  struct final_awaitable {
    bool await_ready() const noexcept {
      return false;
    }

    template <typename PROMISE>
    void await_suspend(std::experimental::coroutine_handle<PROMISE> coroutine) {
      task_promise_base& promise = coroutine.promise();

      // Use 'release' memory semantics in case we finish before the
      // awaiter can suspend so that the awaiting thread sees our
      // writes to the resulting value.
      // Use 'acquire' memory semantics in case the caller registered
      // the continuation before we finished. Ensure we see their write
      // to m_continuation.
      if (promise.m_state.exchange(true, std::memory_order_acq_rel)) {
        promise.m_continuation.resume();
      }
    }

    void await_resume() noexcept {
    }
  };

public:
  task_promise_base() noexcept : m_state(false) {
  }

  auto initial_suspend() noexcept {
    return std::experimental::suspend_always{};
  }

  auto final_suspend() noexcept {
    return final_awaitable{};
  }

  void unhandled_exception() noexcept {
    m_exception = std::current_exception();
  }

  bool try_set_continuation(continuation c) {
    m_continuation = c;
    return !m_state.exchange(true, std::memory_order_acq_rel);
  }

protected:
  bool completed() const noexcept {
    return m_state.load(std::memory_order_relaxed);
  }

  bool completed_with_unhandled_exception() {
    return m_exception != nullptr;
  }

  void rethrow_if_unhandled_exception() {
    if (m_exception != nullptr) {
      std::rethrow_exception(m_exception);
    }
  }

private:
  continuation m_continuation;
  std::exception_ptr m_exception;

  // Initially false. Set to true when either a continuation is registered
  // or when the coroutine has run to completion. Whichever operation
  // successfully transitions from false->true got there first.
  std::atomic<bool> m_state;
};

template <typename T>
class task_promise final : public task_promise_base {
public:
  task_promise() noexcept = default;

  ~task_promise() {
    if (completed() && !completed_with_unhandled_exception()) {
      reinterpret_cast<T*>(&m_valueStorage)->~T();
    }
  }

  task<T> get_return_object() noexcept;

  template <typename VALUE, typename = std::enable_if_t<std::is_convertible_v<VALUE&&, T>>>
  void return_value(VALUE&& value) noexcept(std::is_nothrow_constructible_v<T, VALUE&&>) {
    new (&m_valueStorage) T(std::forward<VALUE>(value));
  }

  T& result() & {
    rethrow_if_unhandled_exception();
    return *reinterpret_cast<T*>(&m_valueStorage);
  }

  T&& result() && {
    rethrow_if_unhandled_exception();
    return std::move(*reinterpret_cast<T*>(&m_valueStorage));
  }

private:
#if CPPCORO_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)  // structure was padded due to alignment.
#endif

  // Not using std::aligned_storage here due to bug in MSVC 2015 Update 2
  // that means it doesn't work for types with alignof(T) > 8.
  // See MS-Connect bug #2658635.
  alignas(T) char m_valueStorage[sizeof(T)];

#if CPPCORO_COMPILER_MSVC
#pragma warning(pop)
#endif
};

template <>
class task_promise<void> : public task_promise_base {
public:
  task_promise() noexcept = default;

  task<void> get_return_object() noexcept;

  void return_void() noexcept {
  }

  void result() {
    rethrow_if_unhandled_exception();
  }
};

template <typename T>
class task_promise<T&> : public task_promise_base {
public:
  task_promise() noexcept = default;

  task<T&> get_return_object() noexcept;

  void return_value(T& value) noexcept {
    m_value = std::addressof(value);
  }

  T& result() {
    rethrow_if_unhandled_exception();
    return *m_value;
  }

private:
  T* m_value;
};

}  // namespace detail

/// \brief
/// A task represents an operation that produces a result both lazily
/// and asynchronously.
///
/// When you call a coroutine that returns a task, the coroutine
/// simply captures any passed parameters and returns exeuction to the
/// caller. Execution of the coroutine body does not start until the
/// coroutine is first co_await'ed.
template <typename T = void>
class task {
public:
  using promise_type = detail::task_promise<T>;

  using value_type = T;

private:
  struct awaitable_base {
    std::experimental::coroutine_handle<promise_type> m_coroutine;

    awaitable_base(std::experimental::coroutine_handle<promise_type> coroutine) noexcept : m_coroutine(coroutine) {
    }

    bool await_ready() const noexcept {
      return !m_coroutine || m_coroutine.done();
    }

    bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
      // NOTE: We are using the bool-returning version of await_suspend() here
      // to work around a potential stack-overflow issue if a coroutine
      // awaits many synchronously-completing tasks in a loop.
      //
      // We first start the task by calling resume() and then conditionally
      // attach the continuation if it has not already completed. This allows us
      // to immediately resume the awaiting coroutine without increasing
      // the stack depth, avoiding the stack-overflow problem. However, it has
      // the down-side of requiring a std::atomic to arbitrate the race between
      // the coroutine potentially completing on another thread concurrently
      // with registering the continuation on this thread.
      //
      // We can eliminate the use of the std::atomic once we have access to
      // coroutine_handle-returning await_suspend() on both MSVC and Clang
      // as this will provide ability to suspend the awaiting coroutine and
      // resume another coroutine with a guaranteed tail-call to resume().
      m_coroutine.resume();
      return m_coroutine.promise().try_set_continuation(detail::continuation{ awaiter });
    }
  };

public:
  task() noexcept : m_coroutine(nullptr) {
  }

  explicit task(std::experimental::coroutine_handle<promise_type> coroutine) : m_coroutine(coroutine) {
  }

  task(task&& t) noexcept : m_coroutine(t.m_coroutine) {
    t.m_coroutine = nullptr;
  }

  /// Disable copy construction/assignment.
  task(const task&) = delete;
  task& operator=(const task&) = delete;

  /// Frees resources used by this task.
  ~task() {
    if (m_coroutine) {
      m_coroutine.destroy();
    }
  }

  task& operator=(task&& other) noexcept {
    if (std::addressof(other) != this) {
      if (m_coroutine) {
        m_coroutine.destroy();
      }

      m_coroutine = other.m_coroutine;
      other.m_coroutine = nullptr;
    }

    return *this;
  }

  /// \brief
  /// Query if the task result is complete.
  ///
  /// Awaiting a task that is ready is guaranteed not to block/suspend.
  bool is_ready() const noexcept {
    return !m_coroutine || m_coroutine.done();
  }

  auto operator co_await() const & noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      decltype(auto) await_resume() {
        if (!this->m_coroutine) {
          throw broken_promise{};
        }

        return this->m_coroutine.promise().result();
      }
    };

    return awaitable{ m_coroutine };
  }

  auto operator co_await() const && noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      decltype(auto) await_resume() {
        if (!this->m_coroutine) {
          throw broken_promise{};
        }

        return std::move(this->m_coroutine.promise()).result();
      }
    };

    return awaitable{ m_coroutine };
  }

  /// \brief
  /// Returns an awaitable that will await completion of the task without
  /// attempting to retrieve the result.
  auto when_ready() const noexcept {
    struct awaitable : awaitable_base {
      using awaitable_base::awaitable_base;

      void await_resume() const noexcept {
      }
    };

    return awaitable{ m_coroutine };
  }

  // Internal helper method for when_all() implementation.
  auto get_starter() const noexcept {
    class starter {
    public:
      starter(std::experimental::coroutine_handle<promise_type> coroutine) noexcept : m_coroutine(coroutine) {
      }

      void start(detail::continuation continuation) noexcept {
        if (m_coroutine && !m_coroutine.done()) {
          m_coroutine.resume();
          if (m_coroutine.promise().try_set_continuation(continuation)) {
            return;
          }
        }

        continuation.resume();
      }

    private:
      std::experimental::coroutine_handle<promise_type> m_coroutine;
    };

    return starter{ m_coroutine };
  }

private:
  std::experimental::coroutine_handle<promise_type> m_coroutine;
};

namespace detail {

template <typename T>
task<T> task_promise<T>::get_return_object() noexcept {
  return task<T>{ std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
}

inline task<void> task_promise<void>::get_return_object() noexcept {
  return task<void>{ std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
}

template <typename T>
task<T&> task_promise<T&>::get_return_object() noexcept {
  return task<T&>{ std::experimental::coroutine_handle<task_promise>::from_promise(*this) };
}

}  // namespace detail

// == generator ===============================================================

template <typename T>
class generator;

namespace detail {

template <typename T>
class generator_promise {
public:
  using value_type = std::remove_reference_t<T>;
  using reference_type = std::conditional_t<std::is_reference_v<T>, T, T&>;
  using pointer_type = value_type*;

  generator_promise() = default;

  generator<T> get_return_object() noexcept;

  constexpr std::experimental::suspend_always initial_suspend() const {
    return {};
  }
  constexpr std::experimental::suspend_always final_suspend() const {
    return {};
  }

  template <typename U, typename = std::enable_if_t<std::is_same<U, T>::value>>
  std::experimental::suspend_always yield_value(U& value) noexcept {
    m_value = std::addressof(value);
    return {};
  }

  std::experimental::suspend_always yield_value(T&& value) noexcept {
    m_value = std::addressof(value);
    return {};
  }

  void unhandled_exception() {
    std::rethrow_exception(std::current_exception());
  }

  void return_void() {
  }

  reference_type value() const noexcept {
    return *m_value;
  }

  // Don't allow any use of 'co_await' inside the generator coroutine.
  template <typename U>
  std::experimental::suspend_never await_transform(U&& value) = delete;

private:
  pointer_type m_value;
};

template <typename T>
class generator_iterator {
  using coroutine_handle = std::experimental::coroutine_handle<generator_promise<T>>;

public:
  using iterator_category = std::input_iterator_tag;
  // What type should we use for counting elements of a potentially infinite sequence?
  using difference_type = std::size_t;
  using value_type = std::remove_reference_t<T>;
  using reference = value_type&;
  using pointer = value_type*;

  explicit generator_iterator(std::nullptr_t) noexcept : m_coroutine(nullptr) {
  }

  explicit generator_iterator(coroutine_handle coroutine) noexcept : m_coroutine(coroutine) {
  }

  bool operator==(const generator_iterator& other) const noexcept {
    return m_coroutine == other.m_coroutine;
  }

  bool operator!=(const generator_iterator& other) const noexcept {
    return !(*this == other);
  }

  generator_iterator& operator++() {
    m_coroutine.resume();
    if (m_coroutine.done()) {
      m_coroutine = nullptr;
    }

    return *this;
  }

  // Don't support post-increment as that would require taking a
  // copy of the old value into the returned iterator as there
  // are no guarantees it's still going to be valid after the
  // increment is executed.
  generator_iterator operator++(int) = delete;

  reference operator*() const noexcept {
    return m_coroutine.promise().value();
  }

  pointer operator->() const noexcept {
    return std::addressof(operator*());
  }

private:
  coroutine_handle m_coroutine;
};

}  // namespace detail

template <typename T>
class generator {
public:
  using promise_type = detail::generator_promise<T>;
  using iterator = detail::generator_iterator<T>;

  generator() noexcept : m_coroutine(nullptr) {
  }

  generator(generator&& other) noexcept : m_coroutine(other.m_coroutine) {
    other.m_coroutine = nullptr;
  }

  generator(const generator& other) = delete;

  ~generator() {
    if (m_coroutine) {
      m_coroutine.destroy();
    }
  }

  generator& operator=(generator other) noexcept {
    swap(other);
    return *this;
  }

  iterator begin() {
    if (m_coroutine) {
      m_coroutine.resume();
      if (!m_coroutine.done()) {
        return iterator{ m_coroutine };
      }
    }

    return iterator{ nullptr };
  }

  iterator end() noexcept {
    return iterator{ nullptr };
  }

  void swap(generator& other) noexcept {
    std::swap(m_coroutine, other.m_coroutine);
  }

private:
  friend class detail::generator_promise<T>;

  explicit generator(std::experimental::coroutine_handle<promise_type> coroutine) noexcept : m_coroutine(coroutine) {
  }

  std::experimental::coroutine_handle<promise_type> m_coroutine;
};

template <typename T>
void swap(generator<T>& a, generator<T>& b) {
  a.swap(b);
}

namespace detail {

template <typename T>
generator<T> generator_promise<T>::get_return_object() noexcept {
  using coroutine_handle = std::experimental::coroutine_handle<generator_promise<T>>;
  return generator<T>{ coroutine_handle::from_promise(*this) };
}

}  // namespace detail

// == async_generator =========================================================

template <typename T>
class async_generator;

namespace detail {

template <typename T>
class async_generator_iterator;
class async_generator_yield_operation;
class async_generator_advance_operation;

class async_generator_promise_base {
public:
  async_generator_promise_base() noexcept : m_state(state::value_ready_producer_suspended), m_exception(nullptr) {
    // Other variables left intentionally uninitialised as they're
    // only referenced in certain states by which time they should
    // have been initialised.
  }

  async_generator_promise_base(const async_generator_promise_base& other) = delete;
  async_generator_promise_base& operator=(const async_generator_promise_base& other) = delete;

  std::experimental::suspend_always initial_suspend() const noexcept {
    return {};
  }

  async_generator_yield_operation final_suspend() noexcept;

  void unhandled_exception() noexcept {
    // Don't bother capturing the exception if we have been cancelled
    // as there is no consumer that will see it.
    if (m_state.load(std::memory_order_relaxed) != state::cancelled) {
      m_exception = std::current_exception();
    }
  }

  void return_void() noexcept {
  }

  /// Query if the generator has reached the end of the sequence.
  ///
  /// Only valid to call after resuming from an awaited advance operation.
  /// i.e. Either a begin() or iterator::operator++() operation.
  bool finished() const noexcept {
    return m_currentValue == nullptr;
  }

  void rethrow_if_unhandled_exception() {
    if (m_exception) {
      std::rethrow_exception(std::move(m_exception));
    }
  }

  /// Request that the generator cancel generation of new items.
  ///
  /// \return
  /// Returns true if the request was completed synchronously and the associated
  /// producer coroutine is now available to be destroyed. In which case the caller
  /// is expected to call destroy() on the coroutine_handle.
  /// Returns false if the producer coroutine was not at a suitable suspend-point.
  /// The coroutine will be destroyed when it next reaches a co_yield or co_return
  /// statement.
  bool request_cancellation() noexcept {
    const auto previousState = m_state.exchange(state::cancelled, std::memory_order_acq_rel);

    // Not valid to destroy async_generator<T> object if consumer coroutine still suspended
    // in a co_await for next item.
    assert(previousState != state::value_not_ready_consumer_suspended);

    // A coroutine should only ever be cancelled once, from the destructor of the
    // owning async_generator<T> object.
    assert(previousState != state::cancelled);

    return previousState == state::value_ready_producer_suspended;
  }

protected:
  async_generator_yield_operation internal_yield_value() noexcept;

private:
  friend class async_generator_yield_operation;
  friend class async_generator_advance_operation;

  // State transition diagram
  //   VNRCA - value_not_ready_consumer_active
  //   VNRCS - value_not_ready_consumer_suspended
  //   VRPA  - value_ready_consumer_active
  //   VRPS  - value_ready_consumer_suspended
  //
  //       A         +---  VNRCA --[C]--> VNRCS   yield_value()
  //       |         |     |  A           |  A       |   .
  //       |        [C]   [P] |          [P] |       |   .
  //       |         |     | [C]          | [C]      |   .
  //       |         |     V  |           V  |       |   .
  //  operator++/    |     VRPS <--[P]--- VRPA       V   |
  //  begin()        |      |              |             |
  //                 |     [C]            [C]            |
  //                 |      +----+     +---+             |
  //                 |           |     |                 |
  //                 |           V     V                 V
  //                 +--------> cancelled         ~async_generator()
  //
  // [C] - Consumer performs this transition
  // [P] - Producer performs this transition
  enum class state {
    value_not_ready_consumer_active,
    value_not_ready_consumer_suspended,
    value_ready_producer_active,
    value_ready_producer_suspended,
    cancelled
  };

  std::atomic<state> m_state;

  std::exception_ptr m_exception;

  std::experimental::coroutine_handle<> m_consumerCoroutine;

protected:
  void* m_currentValue;
};

class async_generator_yield_operation final {
  using state = async_generator_promise_base::state;

public:
  async_generator_yield_operation(async_generator_promise_base& promise, state initialState) noexcept :
    m_promise(promise), m_initialState(initialState) {
  }

  bool await_ready() const noexcept {
    return m_initialState == state::value_not_ready_consumer_suspended;
  }

  bool await_suspend(std::experimental::coroutine_handle<> producer) noexcept;

  void await_resume() noexcept {
  }

private:
  async_generator_promise_base& m_promise;
  state m_initialState;
};

inline async_generator_yield_operation async_generator_promise_base::final_suspend() noexcept {
  m_currentValue = nullptr;
  return internal_yield_value();
}

inline async_generator_yield_operation async_generator_promise_base::internal_yield_value() noexcept {
  state currentState = m_state.load(std::memory_order_acquire);
  assert(currentState != state::value_ready_producer_active);
  assert(currentState != state::value_ready_producer_suspended);

  if (currentState == state::value_not_ready_consumer_suspended) {
    // Only need relaxed memory order since we're resuming the
    // consumer on the same thread.
    m_state.store(state::value_ready_producer_active, std::memory_order_relaxed);

    // Resume the consumer.
    // It might ask for another value before returning, in which case it'll
    // transition to value_not_ready_consumer_suspended and we can return from
    // yield_value without suspending, otherwise we should try to suspend
    // the producer in which case the consumer will wake us up again
    // when it wants the next value.
    m_consumerCoroutine.resume();

    // Need to use acquire semantics here since it's possible that the
    // consumer might have asked for the next value on a different thread
    // which executed concurrently with the call to m_consumerCoro on the
    // current thread above.
    currentState = m_state.load(std::memory_order_acquire);
  }

  return async_generator_yield_operation{ *this, currentState };
}

inline bool async_generator_yield_operation::await_suspend(std::experimental::coroutine_handle<> producer) noexcept {
  state currentState = m_initialState;
  if (currentState == state::value_not_ready_consumer_active) {
    bool producerSuspended = m_promise.m_state.compare_exchange_strong(
      currentState, state::value_ready_producer_suspended, std::memory_order_release, std::memory_order_acquire);
    if (producerSuspended) {
      return true;
    }

    if (currentState == state::value_not_ready_consumer_suspended) {
      // Can get away with using relaxed memory semantics here since we're
      // resuming the consumer on the current thread.
      m_promise.m_state.store(state::value_ready_producer_active, std::memory_order_relaxed);

      m_promise.m_consumerCoroutine.resume();

      // The consumer might have asked for another value before returning, in which case
      // it'll transition to value_not_ready_consumer_suspended and we can return without
      // suspending, otherwise we should try to suspend the producer, in which case the
      // consumer will wake us up again when it wants the next value.
      //
      // Need to use acquire semantics here since it's possible that the consumer might
      // have asked for the next value on a different thread which executed concurrently
      // with the call to m_consumerCoro.resume() above.
      currentState = m_promise.m_state.load(std::memory_order_acquire);
      if (currentState == state::value_not_ready_consumer_suspended) {
        return false;
      }
    }
  }

  // By this point the consumer has been resumed if required and is now active.

  if (currentState == state::value_ready_producer_active) {
    // Try to suspend the producer.
    // If we failed to suspend then it's either because the consumer destructed, transitioning
    // the state to cancelled, or requested the next item, transitioning the state to value_not_ready_consumer_suspended.
    const bool suspendedProducer = m_promise.m_state.compare_exchange_strong(
      currentState, state::value_ready_producer_suspended, std::memory_order_release, std::memory_order_acquire);
    if (suspendedProducer) {
      return true;
    }

    if (currentState == state::value_not_ready_consumer_suspended) {
      // Consumer has asked for the next value.
      return false;
    }
  }

  assert(currentState == state::cancelled);

  // async_generator object has been destroyed and we're now at a
  // co_yield/co_return suspension point so we can just destroy
  // the coroutine.
  producer.destroy();

  return true;
}

class async_generator_advance_operation {
  using state = async_generator_promise_base::state;

protected:
  async_generator_advance_operation(std::nullptr_t) noexcept : m_promise(nullptr), m_producerCoroutine(nullptr) {
  }

  async_generator_advance_operation(
    async_generator_promise_base& promise, std::experimental::coroutine_handle<> producerCoroutine) noexcept :
    m_promise(std::addressof(promise)),
    m_producerCoroutine(producerCoroutine) {
    state initialState = promise.m_state.load(std::memory_order_acquire);
    if (initialState == state::value_ready_producer_suspended) {
      // Can use relaxed memory order here as we will be resuming the producer
      // on the same thread.
      promise.m_state.store(state::value_not_ready_consumer_active, std::memory_order_relaxed);

      producerCoroutine.resume();

      // Need to use acquire memory order here since it's possible that the
      // coroutine may have transferred execution to another thread and
      // completed on that other thread before the call to resume() returns.
      initialState = promise.m_state.load(std::memory_order_acquire);
    }

    m_initialState = initialState;
  }

public:
  bool await_ready() const noexcept {
    return m_initialState == state::value_ready_producer_suspended;
  }

  bool await_suspend(std::experimental::coroutine_handle<> consumerCoroutine) noexcept {
    m_promise->m_consumerCoroutine = consumerCoroutine;

    auto currentState = m_initialState;
    if (currentState == state::value_ready_producer_active) {
      // A potential race between whether consumer or producer coroutine
      // suspends first. Resolve the race using a compare-exchange.
      if (m_promise->m_state.compare_exchange_strong(
            currentState, state::value_not_ready_consumer_suspended, std::memory_order_release,
            std::memory_order_acquire)) {
        return true;
      }

      assert(currentState == state::value_ready_producer_suspended);

      m_promise->m_state.store(state::value_not_ready_consumer_active, std::memory_order_relaxed);

      m_producerCoroutine.resume();

      currentState = m_promise->m_state.load(std::memory_order_acquire);
      if (currentState == state::value_ready_producer_suspended) {
        // Producer coroutine produced a value synchronously.
        return false;
      }
    }

    assert(currentState == state::value_not_ready_consumer_active);

    // Try to suspend consumer coroutine, transitioning to value_not_ready_consumer_suspended.
    // This could be racing with producer making the next value available and suspending
    // (transition to value_ready_producer_suspended) so we use compare_exchange to decide who
    // wins the race.
    // If compare_exchange succeeds then consumer suspended (and we return true).
    // If it fails then producer yielded next value and suspended and we can return
    // synchronously without suspended (ie. return false).
    return m_promise->m_state.compare_exchange_strong(
      currentState, state::value_not_ready_consumer_suspended, std::memory_order_release, std::memory_order_acquire);
  }

protected:
  async_generator_promise_base* m_promise;
  std::experimental::coroutine_handle<> m_producerCoroutine;

private:
  state m_initialState;
};

template <typename T>
class async_generator_promise final : public async_generator_promise_base {
  using value_type = std::remove_reference_t<T>;

public:
  async_generator_promise() noexcept = default;

  async_generator<T> get_return_object() noexcept;

  async_generator_yield_operation yield_value(value_type& value) noexcept {
    m_currentValue = std::addressof(value);
    return internal_yield_value();
  }

  async_generator_yield_operation yield_value(value_type&& value) noexcept {
    return yield_value(value);
  }

  T& value() const noexcept {
    return *static_cast<T*>(m_currentValue);
  }
};

template <typename T>
class async_generator_promise<T&&> final : public async_generator_promise_base {
public:
  async_generator_promise() noexcept = default;

  async_generator<T> get_return_object() noexcept;

  async_generator_yield_operation yield_value(T&& value) noexcept {
    m_currentValue = std::addressof(value);
    return internal_yield_value();
  }

  T&& value() const noexcept {
    return std::move(*static_cast<T*>(m_currentValue));
  }
};

template <typename T>
class async_generator_increment_operation final : public async_generator_advance_operation {
public:
  async_generator_increment_operation(async_generator_iterator<T>& iterator) noexcept :
    async_generator_advance_operation(iterator.m_coroutine.promise(), iterator.m_coroutine), m_iterator(iterator) {
  }

  async_generator_iterator<T>& await_resume();

private:
  async_generator_iterator<T>& m_iterator;
};

template <typename T>
class async_generator_iterator final {
  using promise_type = async_generator_promise<T>;
  using handle_type = std::experimental::coroutine_handle<promise_type>;

public:
  using iterator_category = std::input_iterator_tag;
  // Not sure what type should be used for difference_type as we don't
  // allow calculating difference between two iterators.
  using difference_type = std::size_t;
  using value_type = std::remove_reference_t<T>;
  using reference = std::add_lvalue_reference_t<T>;
  using pointer = std::add_pointer_t<value_type>;

  async_generator_iterator(std::nullptr_t) noexcept : m_coroutine(nullptr) {
  }

  async_generator_iterator(handle_type coroutine) noexcept : m_coroutine(coroutine) {
  }

  async_generator_increment_operation<T> operator++() noexcept {
    return async_generator_increment_operation<T>{ *this };
  }

  reference operator*() const noexcept {
    return m_coroutine.promise().value();
  }

  bool operator==(const async_generator_iterator& other) const noexcept {
    return m_coroutine == other.m_coroutine;
  }

  bool operator!=(const async_generator_iterator& other) const noexcept {
    return !(*this == other);
  }

private:
  friend class async_generator_increment_operation<T>;

  handle_type m_coroutine;
};

template <typename T>
async_generator_iterator<T>& async_generator_increment_operation<T>::await_resume() {
  if (m_promise->finished()) {
    // Update iterator to end()
    m_iterator = async_generator_iterator<T>{ nullptr };
    m_promise->rethrow_if_unhandled_exception();
  }

  return m_iterator;
}

template <typename T>
class async_generator_begin_operation final : public async_generator_advance_operation {
  using promise_type = async_generator_promise<T>;
  using handle_type = std::experimental::coroutine_handle<promise_type>;

public:
  async_generator_begin_operation(std::nullptr_t) noexcept : async_generator_advance_operation(nullptr) {
  }

  async_generator_begin_operation(handle_type producerCoroutine) noexcept :
    async_generator_advance_operation(producerCoroutine.promise(), producerCoroutine) {
  }

  bool await_ready() const noexcept {
    return m_promise == nullptr || async_generator_advance_operation::await_ready();
  }

  async_generator_iterator<T> await_resume() {
    if (m_promise == nullptr) {
      // Called begin() on the empty generator.
      return async_generator_iterator<T>{ nullptr };
    } else if (m_promise->finished()) {
      // Completed without yielding any values.
      m_promise->rethrow_if_unhandled_exception();
      return async_generator_iterator<T>{ nullptr };
    }

    return async_generator_iterator<T>{ handle_type::from_promise(*static_cast<promise_type*>(m_promise)) };
  }
};

}  // namespace detail

template <typename T>
class async_generator {
public:
  using promise_type = detail::async_generator_promise<T>;
  using iterator = detail::async_generator_iterator<T>;

  async_generator() noexcept : m_coroutine(nullptr) {
  }

  explicit async_generator(promise_type& promise) noexcept :
    m_coroutine(std::experimental::coroutine_handle<promise_type>::from_promise(promise)) {
  }

  async_generator(async_generator&& other) noexcept : m_coroutine(other.m_coroutine) {
    other.m_coroutine = nullptr;
  }

  ~async_generator() {
    if (m_coroutine) {
      if (m_coroutine.promise().request_cancellation()) {
        m_coroutine.destroy();
      }
    }
  }

  async_generator& operator=(async_generator&& other) noexcept {
    async_generator temp(std::move(other));
    swap(temp);
    return *this;
  }

  async_generator(const async_generator&) = delete;
  async_generator& operator=(const async_generator&) = delete;

  auto begin() noexcept {
    if (!m_coroutine) {
      return detail::async_generator_begin_operation<T>{ nullptr };
    }

    return detail::async_generator_begin_operation<T>{ m_coroutine };
  }

  auto end() noexcept {
    return iterator{ nullptr };
  }

  void swap(async_generator& other) noexcept {
    using std::swap;
    swap(m_coroutine, other.m_coroutine);
  }

private:
  std::experimental::coroutine_handle<promise_type> m_coroutine;
};

template <typename T>
void swap(async_generator<T>& a, async_generator<T>& b) noexcept {
  a.swap(b);
}

namespace detail {

template <typename T>
async_generator<T> async_generator_promise<T>::get_return_object() noexcept {
  return async_generator<T>{ *this };
}

}  // namespace detail
}  // namespace net
