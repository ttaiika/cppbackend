#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <memory>

#include "listener.h"

namespace http_server {

template <typename RequestHandler>
inline void ServeHttp(boost::asio::io_context& ioc, const boost::asio::ip::tcp::endpoint& endpoint, RequestHandler&& handler) {
  // При помощи decay_t исключим ссылки из типа RequestHandler,
  // чтобы Listener хранил RequestHandler по значению
  using MyListener = Listener<std::decay_t<RequestHandler>>;

  std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}
}