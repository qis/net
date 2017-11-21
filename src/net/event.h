#pragma once
#include <experimental/coroutine>
#include <cstddef>
#include <cassert>

#if NET_USE_IOCP
#include <windows.h>
#elif NET_USE_EPOLL
#include <sys/epoll.h>
#define NET_TLS_RECV EPOLLIN
#define NET_TLS_SEND EPOLLOUT
#elif NET_USE_KQUEUE
#include <sys/event.h>
#define NET_TLS_RECV EVFILT_READ
#define NET_TLS_SEND EVFILT_WRITE
#endif

namespace net {

#if NET_USE_IOCP
using event_base = OVERLAPPED;
#elif NET_USE_EPOLL
using event_base = epoll_event;
#elif NET_USE_KQUEUE
using event_base = struct kevent;
#endif

class event final : public event_base {
public:
  using handle_type = std::experimental::coroutine_handle<>;

#if NET_USE_IOCP
  event() noexcept : event_base({}) {
  }
#elif NET_USE_EPOLL
  event(int service, int socket, uint32_t filter) noexcept : event_base({}), socket_(socket), service_(service) {
    events = filter;
    data.ptr = this;
  }
#elif NET_USE_KQUEUE
  event(int service, int socket, short filter) noexcept : event_base({}), service_(service) {
    const auto ev = static_cast<kevent*>(this);
    const auto ident = static_cast<uintptr_t>(socket);
    EV_SET(ev, ident, filter, EV_ADD | EV_ONESHOT, 0, 0, this);
  }
#endif

  event(event&& other) = delete;
  event& operator=(event&& other) = delete;

  event(const event& other) = delete;
  event& operator=(const event& other) = delete;

  ~event() = default;

  constexpr bool await_ready() noexcept {
    return false;
  }

  void await_suspend(handle_type handle) {
    handle_ = handle;
#if NET_USE_IOCP
    size_ = 0;
#elif NET_USE_EPOLL
    [[maybe_unused]] const auto rv = ::epoll_ctl(service_, EPOLL_CTL_ADD, socket_, this);
    assert(rv == 0);
#elif NET_USE_KQUEUE
    [[maybe_unused]] const auto rv = ::kevent(service_, this, 1, nullptr, 0, nullptr);
    assert(rv == 1);
#endif
  }

#if NET_USE_IOCP
  constexpr auto await_resume() noexcept {
    return size_;
  }
#else
  constexpr void await_resume() noexcept {
  }
#endif

#if NET_USE_IOCP
  void operator()(DWORD size) noexcept {
    size_ = size;
    handle_.resume();
  }
#else
  void operator()() noexcept {
#if NET_USE_EPOLL
    ::epoll_ctl(service_, EPOLL_CTL_DEL, socket_, this);
#endif
    handle_.resume();
  }
#endif

private:
  handle_type handle_ = nullptr;

#if NET_USE_IOCP
  DWORD size_ = 0;
#else
#if NET_USE_EPOLL
  int socket_ = -1;
#endif
  int service_ = -1;
#endif
};

}  // namespace net
