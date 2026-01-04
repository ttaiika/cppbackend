#pragma once

#include "request_handler.h"
#include "logger.h"
#include <boost/beast/http.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <string>

namespace log_handler {

namespace http = boost::beast::http;

class LoggingRequestHandler {
public:
    LoggingRequestHandler(http_handler::RequestHandler& handler)
        : handler_(handler) {
    }

    template <typename Send>
    void operator()(http::request<http::string_body>&& req, Send&& send, const std::string& ip) {
        LogRequest(req, ip);

        const auto start_time = std::chrono::steady_clock::now();

        handler_(std::move(req), 
            [this, start_time, send = std::forward<Send>(send)]
            (auto&& response) mutable {
                LogResponse(response, start_time);
                send(std::forward<decltype(response)>(response));
            }
        );
    }

private:
    http_handler::RequestHandler& handler_;

    // REQUEST LOGGING
    void LogRequest(const http::request<http::string_body>& req, const std::string& client_ip){
        logger::json::value data{
            {"ip", client_ip},
            {"URI", std::string(req.target())},
            {"method", std::string(req.method_string())},
        };

        BOOST_LOG_TRIVIAL(info)
            << boost::log::add_value(logger::additional_data, data)
            << "request received";
    }

    // RESPONSE LOGGING
    template <typename Response>
    void LogResponse(const Response& res,
                     const std::chrono::steady_clock::time_point& start) {
        using namespace std::chrono;
        int ms = duration_cast<milliseconds>(steady_clock::now() - start).count();

        std::string content_type = "unknown";
        if (auto it = res.base().find(http::field::content_type); it != res.base().end()) {
            content_type = std::string(it->value());
        }

        logger::json::value data{
            {"response_time", ms},
            {"code", res.result_int()},
            {"content_type", content_type}
        };

        BOOST_LOG_TRIVIAL(info)
            << boost::log::add_value(logger::additional_data, data)
            << "response sent";
    }
};

} // namespace log_handler