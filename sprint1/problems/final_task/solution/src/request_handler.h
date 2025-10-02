#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    };

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        
        std::string target = std::string(req.target()); // копируем в std::string для удобства

        constexpr std::string_view prefix = "/api/v1/maps/";

        // если запрос начинается с "/api/v1/maps/"
        if (target.starts_with(prefix)) {
            // Достаём id-карты
            
            model::Map::Id id{target.substr(prefix.size())};

            // если id не было
            if ((*id).empty()) {
                // Создаем объект ответа с ошибкой
                return SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
            }

            const model::Map* map = game_.FindMap(id);

            //если существует карта с таким id
            if (map != nullptr) {
                return SendJsonResponse(req, send, http::status::ok, json::serialize(MakeMapJson(*map)));
            } else {
                return SendJsonResponse(req, send, http::status::not_found, R"({"code":"mapNotFound","message":"Map not found"})");
            }
        }
        
        if (target == "/api/v1/maps") {
            return SendJsonResponse(req, send, http::status::ok, json::serialize(MakeMapsJson()));
        } else {
            return SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
        }
    }

private:
    json::object MakeRoadJson(const model::Road& road);
    json::object MakeBuildingJson(const model::Building& b);
    json::object MakeOfficeJson(const model::Office& o);
    json::object MakeMapJson(const model::Map& map);
    json::array MakeMapsJson();

    template <typename Body, typename Send>
    void SendJsonResponse(http::request<Body> const& req, Send&& send, http::status status, const std::string& body) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, ContentType::APPLICATION_JSON);
        res.keep_alive(req.keep_alive());
        res.body() = body;
        res.prepare_payload();
        send(std::move(res));
    }

    model::Game& game_;
};

}  // namespace http_handler
