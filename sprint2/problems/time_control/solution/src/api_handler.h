#pragma once
#include "http_server.h"
#include "content_type.h"
#include "players.h"
#include "model.h"

#include <boost/json.hpp>
#include <boost/asio/io_context.hpp>
#include <iomanip>  // для std::setprecision
#include <sstream>  // для std::ostringstream
#include <iostream> 

namespace api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio;
using namespace std::literals;

class ApiHandler {
public:
    explicit ApiHandler(model::Game& game, net::io_context& ioc)
        : game_(game)
        , strand_(net::make_strand(ioc)) {}

    template <typename Body, typename Allocator, typename Send>
    bool Handle(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target = std::string(req.target());

        if (!target.starts_with("/api/"))
            return false;   // это не API

        // ВСЕ API запросы должны выполняться в strand для thread safety
        if (target == "/api/v1/maps" || 
            target.starts_with("/api/v1/maps/") ||
            target == "/api/v1/game/join" ||
            target == "/api/v1/game/players" ||
            target == "/api/v1/game/state" ||
            target == "/api/v1/game/player/action" ||
            target == "/api/v1/game/tick") {
            
            boost::asio::dispatch(
                strand_,
                [this, target, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                    HandleRequestInStrand(target, std::move(req), std::move(send));
                }
            );
            return true;
        }

        SendJsonResponse(req, send, http::status::bad_request, 
            R"({"code":"badRequest","message":"Bad request"})");
        return true;
    }

    /*template <typename Body, typename Allocator, typename Send>
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

        if (target == "/api/v1/game/player/action") {
            boost::asio::dispatch (
                strand_,
                [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                    HandlePlayerAction(req, send);
                }
            );
            return true;
        }

        if (target == "/api/v1/game/tick") {
            boost::asio::dispatch (
                strand_,
                [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                    HandleTick(req, send);
                }
            );
            return true;
        }

        SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
        return true;
    }*/

private:
    model::Game& game_;
    net::strand<net::io_context::executor_type> strand_;

    template <typename Body, typename Send>
    void HandleRequestInStrand(const std::string& target, 
                               http::request<Body>&& req, 
                               Send&& send) {
        
        if (target == "/api/v1/maps") {
            HandleMaps(req, send);
            return;
        }

        constexpr std::string_view prefix = "/api/v1/maps/";
        if (target.starts_with(prefix)) {
            model::Map::Id id{target.substr(prefix.size())};
            HandleMapById(req, send, id);
            return;
        }     
        
        if (target == "/api/v1/game/join") {
            HandleJoin(req, send);
            return;
        }

        if (target == "/api/v1/game/players") {
            HandlePlayers(req, send);
            return;
        }

        if (target == "/api/v1/game/state") {
            HandleState(req, send);
            return;
        }

        if (target == "/api/v1/game/player/action") {
            HandlePlayerAction(req, send);
            return;
        }

        if (target == "/api/v1/game/tick") {
            HandleTick(req, send);
            return;
        }

        SendJsonResponse(req, send, http::status::bad_request, 
            R"({"code":"badRequest","message":"Bad request"})");
    }

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

        auto map = game_.FindMap(map_id);

        const auto& roads = map->GetRoads();
        const auto& road = roads.front();

        double x = static_cast<double>(road.GetStart().x);
        double y = static_cast<double>(road.GetStart().y);

        player->GetDog().SetPosition({x, y});

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

        auto session = player->GetSession();
        const auto& dogs = session->GetDogs();

        std::ostringstream out;

         // Добавляем форматирование для чисел с плавающей точкой
        out << std::fixed << std::setprecision(18);

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

        for (size_t i = 0; i < dogs.size(); ++i) {
        if (dogs[i]) {
            out << "    \"" << dogs[i]->GetId() << "\": {\n"
                << "      \"pos\": [" << dogs[i]->GetPosition().x 
                << ", " << dogs[i]->GetPosition().y << "],\n"
                << "      \"speed\": [" << dogs[i]->GetSpeed().x 
                << ", " << dogs[i]->GetSpeed().y << "],\n"
                << "      \"dir\": \"" << DirectionToString(dogs[i]->GetDirection()) << "\"\n"
                << "    }";
            if (i + 1 < dogs.size()) out << ",\n";
            else out << "\n";
        }
    }

        out << "  }\n";
        out << "}\n";

        
        SendJsonResponse(req, send, http::status::ok, out.str());
    }

    model::Dog* FindDogInSession(std::shared_ptr<GameSession> session, uint32_t dog_id) {
    const auto& dogs = session->GetDogs();
    for (const auto& dog_ptr : dogs) {
        if (dog_ptr && dog_ptr->GetId() == dog_id) {
            return dog_ptr.get();
        }
    }
    return nullptr;
}

    template <typename Body, typename Send>
    void HandlePlayerAction(http::request<Body> const& req, Send&& send) {
        // Проверяем метод
        if (req.method() != http::verb::post) {
            return SendJsonResponse(req, send, http::status::method_not_allowed,
                R"({"code":"invalidMethod","message":"Invalid method"})",
                /*allow_post=*/true 
            );
        }

        // Проверяем Content-Type
        auto it = req.find(http::field::content_type);
        if (it == req.end() || it->value() != "application/json") {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Invalid content type"})"
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

        // Парсим JSON
        json::value json;
        try {
            json = json::parse(req.body());
        } catch (...) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse action"})"
            );
        }

        // Проверяем, что JSON - объект
        if (!json.is_object()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse action"})"
            );
        }

        auto& obj = json.as_object();

        // Получаем сессию игрока
    auto session = player->GetSession();
    
    // Получаем ID карты из сессии
    auto map_id = session->GetMapId();
    
    // Проверка существования карты
    if (!game_.FindMap(map_id)) {
        return SendJsonResponse(
            req, send, http::status::not_found,
            R"({"code":"mapNotFound","message":"Map not found"})"
        );
    }

        // Проверяем наличие и корректность move
        if (!obj.contains("move") || !obj["move"].is_string()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse action"})"
            );
        }

        std::string move = obj["move"].as_string().c_str();
        if (move != "L" && move != "R" && move != "U" && move != "D" && move != "") {
            return SendJsonResponse(req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse action"})"
            );
        }

        // Получаем собаку из игрока
    auto& dog = player->GetDog();
    
    // Получаем скорость собаки из карты
    double dog_speed = game_.FindMap(map_id)->GetDogSpeed();
    
    // Устанавливаем скорость
    if (move == "L") {
        dog.SetSpeed({-dog_speed, 0});
        dog.SetDirection(model::Direction::WEST);
    } else if (move == "R") {
        dog.SetSpeed({dog_speed, 0});
        dog.SetDirection(model::Direction::EAST);
    } else if (move == "U") {
        dog.SetSpeed({0, -dog_speed});
        dog.SetDirection(model::Direction::NORTH);
    } else if (move == "D") {
        dog.SetSpeed({0, dog_speed});
        dog.SetDirection(model::Direction::SOUTH);
    } else {
        dog.SetSpeed({0, 0});
    }
    
    // Находим собаку в сессии и обновляем ее
    auto session_dog = FindDogInSession(session, dog.GetId());
    if (session_dog) {
        session_dog->SetSpeed(dog.GetSpeed());
        session_dog->SetDirection(dog.GetDirection());
    }

        // Формируем успешный ответ
        return SendJsonResponse(req, send, http::status::ok, "{}\n");
    }

    template <typename Body, typename Send>
    void HandleTick(http::request<Body> const& req, Send&& send) {
        // Проверяем метод
        if (req.method() != http::verb::post) {
            return SendJsonResponse(req, send, http::status::method_not_allowed,
                R"({"code":"invalidMethod","message":"Invalid method"})",
                /*allow_post=*/true 
            );
        }

        // Проверяем Content-Type
        auto it = req.find(http::field::content_type);
        if (it == req.end() || it->value() != "application/json") {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Invalid content type"})"
            );
        }

        // Парсим JSON
        json::value json;
        try {
            json = json::parse(req.body());
        } catch (...) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse tick request JSON"})"
            );
        }

        // Проверяем, что JSON - объект
        if (!json.is_object()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse tick request JSON"})"
            );
        }

        auto& obj = json.as_object();

        // Проверяем наличие и корректность move
        if (!obj.contains("timeDelta") || !obj["timeDelta"].is_int64()) {
            return SendJsonResponse(
                req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse tick request JSON"})"
            );
        }

        int dt_ms = obj["timeDelta"].as_int64();  // 10, 20, ...
        if (dt_ms < 0) {
            return SendJsonResponse(req, send, http::status::bad_request,
                R"({"code":"invalidArgument","message":"Failed to parse tick request JSON"})"
            );
        }

        std::cout << "=== TICK HANDLER CALLED ===" << std::endl;
        std::cout << "Request target: " << req.target() << std::endl;
        std::cout << "Request body: " << req.body() << std::endl;

        std::cout << "=== HandleTick ===" << std::endl;
    std::cout << "Request target: " << req.target() << std::endl;
    std::cout << "Request body: " << req.body() << std::endl;
    std::cout << "dt_ms: " << dt_ms << std::endl;
    
    // ДОБАВЛЕНО: вывод информации о всех сессиях
    
    // Выводим адреса всех сессий
    auto sessions = game_.GetSessions(); // Нужно добавить этот метод в Game
    for (size_t i = 0; i < sessions.size(); ++i) {
        if (sessions[i]) {
            std::cout << "Session " << i << " address: " << sessions[i].get() 
                      << ", map: " << *sessions[i]->GetMapId() << std::endl;
        }
    }

        //double dt_seconds = dt_ms / 1000.0;   
        game_.Tick(dt_ms);

        // Формируем успешный ответ
        return SendJsonResponse(req, send, http::status::ok, "{}");
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