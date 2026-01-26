#pragma once

#include <boost/beast/core.hpp>
#include "../logger.h"

namespace http_server {

using namespace std::literals;

inline void ReportError(boost::beast::error_code ec, std::string_view what) {
  Logger::LogErr(ec, what);
}

}  // namespace http_server