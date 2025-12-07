#pragma once
#include "http_server.h"
#include "content_type.h"
#include "players.h"
#include "model.h"

#include <boost/json.hpp>

namespace api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;

class ApiHandler {
public:
    explicit ApiHandler(model::Game& game)
        : game_(game) {}

    template <typename Body, typename Allocator, typename Send>
    bool Handle(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());

        if (!target.starts_with("/api/"))
            return false;   // это не API

        if (target == "/api/v1/maps") {
            HandleMaps(req, send);
            return true;
        }

        constexpr std::string_view prefix = "/api/v1/maps/";
        if (target.starts_with(prefix)) {
            model::Map::Id id{target.substr(prefix.size())};
            HandleMapById(req, send, id);
            return true;
        }     
        
        if (target == "/api/v1/game/join") {
            HandleJoin(req, send);
            return true;
        }

        if (target == "/api/v1/game/players") {
            HandlePlayers(req, send);
            return true;
        }

        if (target == "/api/v1/game/state") {
            HandleState(req, send);
            return true;
        }

        SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
        return true;
    }

private:
    model::Game& game_;

    template <typename Body, typename Send>
    void HandleMaps(http::request<Body> const& req, Send&& send) {
        SendJsonResponse(req, send, http::status::ok, json::serialize(MakeMapsJson()));
    }

    template <typename Body, typename Send>
    void HandleMapById(http::request<Body> const& req, Send&& send, model::Map::Id id) {
        // если id не было
        if ((*id).empty()) {
            // Создаем объект ответа с ошибкой
            return SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
        }

        const model::Map* map = game_.FindMap(id);

        //если существует карта с таким id
        if (map != nullptr) {
            return SendJsonResponse(req, send, http::status::ok, json::serialize(MakeMapJson(*map)));
        }
        return SendJsonResponse(req, send, http::status::not_found, R"({"code":"mapNotFound","message":"Map not found"})");
    }

    template <typename Body, typename Send>
    void HandleJoin(http::request<Body> const& req, Send&& send) {
        // Проверяем метод
        if (req.method() != http::verb::post) {
            return SendJsonResponse(req, send, http::status::method_not_allowed,
                R"({"code":"invalidMethod","message":"Only POST method is expected"})",
                /*allow_post=*/true 
            );
        }

        // Проверяем Content-Type
        auto it = req.find(http::field::content_type);
        if (it == req.end() || it->value() != "application/json") {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Invalid Content-Type"})"
            );
        }

        // Парсим JSON
        boost::json::value json;
        try {
            json = boost::json::parse(req.body());
        } catch (...) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Join game request parse error"})"
            );
        }

        // Проверяем, что JSON - объект
        if (!json.is_object()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Invalid JSON object"})"
            );
        }

        auto& obj = json.as_object();

        // Проверяем наличие userName и mapId
        if (!obj.contains("userName") || !obj.contains("mapId") ||
            !obj["userName"].is_string() || !obj["mapId"].is_string()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Invalid parameters"})"
            );
        }

        std::string userName = obj["userName"].as_string().c_str();
        std::string mapId = obj["mapId"].as_string().c_str();

        // Пустое имя игрока
        if (userName.empty()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Invalid name"})"
            );
        }

        model::Map::Id map_id{mapId};

        // Проверка существования карты
        if (!game_.FindMap(map_id)) {
            return SendJsonResponse(
                req, send, http::status::not_found,
                R"({"code":"mapNotFound","message":"Map not found"})"
            );
        }

        // Генерация playerId и токена
        auto player = game_.AddPlayer(userName, map_id);

        // Формируем успешный ответ
        json::object resp;
        resp["authToken"] = *player->GetToken();
        resp["playerId"] = player->GetDog().GetId();

        std::string body = boost::json::serialize(resp);
        
        return SendJsonResponse(req, send, http::status::ok, body);
    }

    template <typename Body, typename Send>
    void HandlePlayers(http::request<Body> const& req, Send&& send) {
        // Проверяем метод
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return SendJsonResponse(req, send, http::status::method_not_allowed,
                R"({"code":"invalidMethod","message":"Invalid method"})",
                /*allow_post=*/false, /*allow_get_head=*/true
            );
        }

        // Проверяем заголовок Authorization
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return SendJsonResponse(
                req, send, http::status::unauthorized,
                R"({"code":"invalidToken","message":"Authorization header is missing"})"
            );
        }

        std::string auth = std::string(req[http::field::authorization]);

        // Формат: Bearer <token>
        const std::string prefix = "Bearer ";
        if (auth.rfind(prefix, 0) != 0 || auth.size() <= prefix.size()) {
            return SendJsonResponse(
                req, send, http::status::unauthorized,
                R"({"code":"invalidToken","message":"Invalid Authorization header format"})"
            );
        }

        std::string token_str = auth.substr(prefix.size());
        Token token{token_str};

        // Проверяем, существует ли такой токен
        auto player = game_.GetPlayers().FindByToken(token);
        
        if (!player) {
            // Токен не найден вообще
            if (IsValidFormat(*token)) {
            // Если токен валидного формата, но игрока нет
            return SendJsonResponse(
                req, send, http::status::unauthorized,
                R"({"code":"unknownToken","message":"Player token has not been found"})"
            );
        } else {
            // Токен невалидный формат
            return SendJsonResponse(
                req, send, http::status::unauthorized,
                R"({"code":"invalidToken","message":"Player token has not been found"})"
            );
        }
        }

        auto map_id = player->GetSession()->GetMapId();

        // HEAD — только заголовки
        if (req.method() == http::verb::head) {
            SendJsonResponse(req, send, http::status::ok, "{}");
            return;
        }

        // Получаем всех игроков на карте
        auto others = game_.GetPlayers().GetPlayersOnMap(map_id);

        std::ostringstream out;
        out << "{";

        for (size_t i = 0; i < others.size(); ++i) {
            auto& p = others[i];
            out << "\"" << p->GetDog().GetId() << "\": {\"name\": \""
                << p->GetDog().GetName() << "\"}";
            if (i + 1 < others.size()) out << ",\n";
        }

        out << "}";

        SendJsonResponse(req, send, http::status::ok, out.str());
    }

    template <typename Body, typename Send>
    void HandleState(http::request<Body> const& req, Send&& send) {
        // Проверяем метод
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return SendJsonResponse(req, send, http::status::method_not_allowed,
                R"({"code":"invalidMethod","message":"Invalid method"})",
                /*allow_post=*/false, /*allow_get_head=*/true
            );
        }

        // Проверяем заголовок Authorization
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return SendJsonResponse(
                req, send, http::status::unauthorized,
                R"({"code":"invalidToken","message":"Authorization header is required"})"
            );
        }

        std::string auth = std::string(req[http::field::authorization]);

        // Формат: Bearer <token>
        const std::string prefix = "Bearer ";
        if (auth.rfind(prefix, 0) != 0 || auth.size() <= prefix.size()) {
            return SendJsonResponse(
                req, send, http::status::unauthorized,
                R"({"code":"invalidToken","message":"Invalid Authorization header format"})"
            );
        }

        std::string token_str = auth.substr(prefix.size());
        Token token{token_str};

        // Проверяем, существует ли такой токен
        auto player = game_.GetPlayers().FindByToken(token);
        
        if (!player) {
            // Токен не найден вообще
            if (IsValidFormat(*token)) {
            // Если токен валидного формата, но игрока нет
                return SendJsonResponse(
                    req, send, http::status::unauthorized,
                    R"({"code":"unknownToken","message":"Player token has not been found"})"
                );
            } else {
                // Токен невалидный формат
                return SendJsonResponse(
                    req, send, http::status::unauthorized,
                    R"({"code":"invalidToken","message":"Player token has not been found"})"
                );
            }
        }

        auto map_id = player->GetSession()->GetMapId();

        // Получаем всех игроков на карте
        auto others = game_.GetPlayers().GetPlayersOnMap(map_id);

        std::ostringstream out;

        // Лямбда для преобразования Direction -> строка
        std::function<std::string(model::Direction)> DirectionToString =
        [](model::Direction dir) -> std::string {
            switch (dir) {
                case model::Direction::NORTH: return "U";
                case model::Direction::SOUTH: return "D";
                case model::Direction::WEST:  return "L";
                case model::Direction::EAST:  return "R";
            }
            // На всякий случай, по умолчанию
            return "U";
        };

        out << "{\n";
        out << "  \"players\": {\n";

        for (size_t i = 0; i < others.size(); ++i) {
            auto& p = others[i];
            out << "    \"" << p->GetDog().GetId() << "\": {\n"
                << "      \"pos\": [" << p->GetDog().GetPosition().x << ", " << p->GetDog().GetPosition().y << "],\n"
                << "      \"speed\": [" << p->GetDog().GetSpeed().x << ", " << p->GetDog().GetSpeed().y << "],\n"
                << "      \"dir\": \"" << DirectionToString(p->GetDog().GetDirection()) << "\"\n"
                << "    }";
            if (i + 1 < others.size()) out << ",\n";
            else out << "\n";
        }

        out << "  }\n";
        out << "}\n";

        SendJsonResponse(req, send, http::status::ok, out.str());
    }

    json::object MakeRoadJson(const model::Road& road);
    json::object MakeBuildingJson(const model::Building& b);
    json::object MakeOfficeJson(const model::Office& o);
    json::object MakeMapJson(const model::Map& map);
    json::array MakeMapsJson();

    template <typename Body, typename Send>
    void SendJsonResponse(http::request<Body> const& req, Send&& send, http::status status, const std::string& body, bool allow_post = false, bool allow_get_head = false) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, ContentType::APPLICATION_JSON);
        res.set(http::field::cache_control, "no-cache");
        if (allow_post) {
            res.set(http::field::allow, "POST");
        }
        if (allow_get_head) {
            res.set(http::field::allow, "GET, HEAD");
        }
        res.keep_alive(req.keep_alive());
        res.body() = body;
        res.prepare_payload();
        send(std::move(res));
    }    
};

} // namespace api