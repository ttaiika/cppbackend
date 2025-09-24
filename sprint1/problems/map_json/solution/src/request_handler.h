#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

json::object ToJson(const model::Road& road);

json::object ToJson(const model::Building& b);

json::object ToJson(const model::Office& o);

json::object ToJson(const model::Map& map);

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    // Запрос, тело которого представлено в виде строки
/*using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;*/

    // Структура ContentType задаёт область видимости для констант,
    // задающий значения HTTP-заголовка Content-Type
    /*struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view TEXT_HTML = "application/json"sv;
        // При необходимости внутрь ContentType можно добавить и другие типы контента
    };

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}*/

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        
        //auto method = req.method();
        std::string target = std::string(req.target()); // копируем в std::string для удобства
        
        const std::string prefix0 = "/api/";
        // если запрос не начинается с /api/
        if (target.rfind(prefix0, 0) != 0) {
            return;
        }
        
        const std::string prefix = "/api/v1/maps/"; 
        const std::string prefix3 = "/api/v1/maps"; 

        // если запрос начинается с "/api/v1/maps/"
        if (target.rfind(prefix, 0) == 0) {
            // Достаём id-карты
            model::Map::Id id{target.substr(prefix.size())};

            // если id не было
            if ((*id).empty()) {
                // Создаем объект ответа с ошибкой
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(http::field::content_type, "application/json");
                res.keep_alive(req.keep_alive());
                res.body() = R"({"code":"badRequest","message":"Bad request"})";
                res.prepare_payload();
                return send(std::move(res));
            }

            const model::Map* map = game_.FindMap(id);
            //если существует карта с таким id
            if (map != nullptr) {
                http::response<http::string_body> res{http::status::ok, req.version()};
                res.set(http::field::content_type, "application/json");
                res.keep_alive(req.keep_alive());
                res.body() = json::serialize(ToJson(*map));   // сериализация карты
                res.prepare_payload();
                return send(std::move(res));
            } else {
                // если не существует
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.set(http::field::content_type, "application/json");
                res.keep_alive(req.keep_alive());
                res.body() = R"({"code":"mapNotFound","message":"Map not found"})";
                res.prepare_payload();
                return send(std::move(res));
            }
        }
        else if (target.rfind(prefix3, 0) == 0){
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = json::serialize(ToJsonMaps());  // сериализация карт
            res.prepare_payload();
            return send(std::move(res));
        } else {
            // Создаем объект ответа с ошибкой
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(http::field::content_type, "application/json");
                res.keep_alive(req.keep_alive());
                res.body() = R"({"code":"badRequest","message":"Bad request"})";
                res.prepare_payload();
                return send(std::move(res));
        }

        // Если URI-строка запроса начинается с /api/, 
        // но не подпадает ни под один из текущих форматов, 
        // сервер должен вернуть ответ с 400 статус-кодом. 
    }

private:
    json::object ToJsonMap(const model::Map& map) {
        json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        return obj;
    }

    json::array ToJsonMaps() {
        json::array arr;
        for (const auto& m : game_.GetMaps()) {
            arr.push_back(ToJsonMap(m));
        }

        return arr;
    }

    model::Game& game_;
};

}  // namespace http_handler
