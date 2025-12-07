#pragma once
#include <boost/json.hpp>
#include "api_handler.h"
#include "static_handler.h"

namespace http_handler {
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::optional<fs::path> root = std::nullopt)
        : api_{game}
        , stat_{root} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (!api_.Handle(std::move(req), send)) {
            stat_.Handle(std::move(req), send);  // вызываем только если API не обработал
        }
    }

private:
    api::ApiHandler api_;
    StaticHandler stat_;
};

}  // namespace http_handler
