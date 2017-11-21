#include <net/socket.h>
#include <tls.h>
#include <vector>

namespace net {

socket::~socket() {
  close();
}

std::string_view socket::alpn() noexcept {
  if (tls_) {
    if (const auto alpn = tls_conn_alpn_selected(tls_.get())) {
      return alpn;
    }
  }
  return {};
}

}  // namespace net
