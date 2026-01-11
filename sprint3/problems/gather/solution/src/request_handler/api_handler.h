#pragma once 

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>
#include <iomanip>  
#include <sstream>  

#include "http_server/content_type.h"
#include "app/application.h"
#include "http_server/http_server.h"
#include "app/players.h"
#include "model/model.h"
#include "utils/ticker.h"
#include "loot_generator.h"

namespace api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace net = boost::asio;
using namespace std::literals;

class ApiHandler {
public:
    ApiHandler(net::strand<net::io_context::executor_type>& strand, Application& app);
    
    template <typename Body, typename Allocator, typename Send>
    void Handle(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    void StartAutoTick(std::chrono::milliseconds period);
    
    // JSON методы для отображения
    json::object CreateRoadJson(const model::Road& road);
    json::object CreateBuildingJson(const model::Building& building);
    json::object CreateOfficeJson(const model::Office& office);
    json::object CreateMapJson(const model::Map& map);
    json::array CreateMapsJson();
    
    // Вспомогательные методы
    model::Dog* FindDogInSession(std::shared_ptr<GameSession> session, uint32_t dog_id);
    void ApplyMovement(model::Dog& dog, const std::string& move, double speed);
    std::string DirectionToString(model::Direction dir);
    
    // Методы для работы с JSON полями
    bool ExtractStringField(const json::object& obj,
                          const std::string& field_name,
                          std::string& out_value);
    bool ExtractInt64Field(const json::object& obj,
                         const std::string& field_name,
                         int64_t& out_value);
    bool IsValidFormat(std::string value) const;

private:
    net::strand<net::io_context::executor_type> strand_;
    Application& app_;
    std::shared_ptr<Ticker> ticker_;

    // Вспомогательные типы и константы
    enum class Endpoint {
        Maps,
        MapById,
        Join,
        Players,
        State,
        PlayerAction,
        Tick,
        Unknown
    };

    static constexpr std::string_view API_PREFIX = "/api/v1/";
    static constexpr std::string_view MAPS_ENDPOINT = "maps";
    static constexpr std::string_view JOIN_ENDPOINT = "game/join";
    static constexpr std::string_view PLAYERS_ENDPOINT = "game/players";
    static constexpr std::string_view STATE_ENDPOINT = "game/state";
    static constexpr std::string_view PLAYER_ACTION_ENDPOINT = "game/player/action";
    static constexpr std::string_view TICK_ENDPOINT = "game/tick";

    Endpoint RouteRequest(const std::string& target, std::string& param) const;
    void Tick(std::chrono::milliseconds delta);
    
    // Основной обработчик запросов в strand
    template <typename Body, typename Send>
    void HandleRequestInStrand(const std::string& target, 
                               http::request<Body>&& req, 
                               Send&& send);

    // Валидация и вспомогательные методы
    template <typename Body, typename Send>
    bool ValidateMethod(const http::request<Body>& req,
                        const std::vector<http::verb>& allowed_methods,
                        Send&& send);
    
    template <typename Body, typename Send>
    bool ValidateContentType(const http::request<Body>& req,
                             const std::string& expected_type,
                             Send&& send);
    
    template <typename Body>
    std::optional<json::value> ParseJsonBody(const http::request<Body>& req);
    
    template <typename Body>
    std::optional<Token> ExtractBearerToken(const http::request<Body>& req);
    
    template <typename Body, typename Send>
    std::shared_ptr<Player> AuthenticatePlayer(const http::request<Body>& req,
                                               Send&& send);

    // Обработчики конкретных эндпоинтов
    template <typename Body, typename Send>
    void HandleMaps(const http::request<Body>& req, Send&& send);
    
    template <typename Body, typename Send>
    void HandleMapById(const http::request<Body>& req, Send&& send,
                       const std::string& map_id_str);
    
    template <typename Body, typename Send>
    void HandleJoin(const http::request<Body>& req, Send&& send);
    
    template <typename Body, typename Send>
    void HandlePlayers(const http::request<Body>& req, Send&& send);
    
    template <typename Body, typename Send>
    void HandleState(const http::request<Body>& req, Send&& send);
    
    template <typename Body, typename Send>
    void HandlePlayerAction(const http::request<Body>& req, Send&& send);
    
    template <typename Body, typename Send>
    void HandleTick(const http::request<Body>& req, Send&& send);

    // Методы отправки ответов
    template <typename Body, typename Send>
    void SendJsonResponse(const http::request<Body>& req,
                          Send&& send,
                          http::status status,
                          const std::string& body,
                          const std::string& allow_header = "");
    
    template <typename Body, typename Send>
    void SendErrorResponse(const http::request<Body>& req,
                           Send&& send,
                           http::status status,
                           const std::string& code,
                           const std::string& message,
                           const std::string& allow_header = "");
};

template <typename Body, typename Allocator, typename Send>
void ApiHandler::Handle(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    std::string target = std::string(req.target());

    if (!target.starts_with("/api/")) {
        return;   // это не API
    }

    // Диспетчеризация запросов в strand для thread safety
    net::dispatch(
        strand_,
        [this, target, req = std::move(req), send = std::forward<Send>(send)]() mutable {
            HandleRequestInStrand(target, std::move(req), std::move(send));
        }
    );
}

template <typename Body, typename Send>
void ApiHandler::HandleRequestInStrand(const std::string& target, 
                                       http::request<Body>&& req, 
                                       Send&& send) {
    std::string param;
    Endpoint endpoint = RouteRequest(target, param);

    switch (endpoint) {
        case Endpoint::Maps:
            HandleMaps(req, send);
            break;
        case Endpoint::MapById:
            HandleMapById(req, send, param);
            break;
        case Endpoint::Join:
            HandleJoin(req, send);
            break;
        case Endpoint::Players:
            HandlePlayers(req, send);
            break;
        case Endpoint::State:
            HandleState(req, send);
            break;
        case Endpoint::PlayerAction:
            HandlePlayerAction(req, send);
            break;
        case Endpoint::Tick: {
            if (!app_.IsManualTicker()) {
                // Эндпоинт недоступен, как несуществующий
                SendErrorResponse(req, send,
                    http::status::bad_request,
                    "badRequest",
                    "Invalid endpoint");
            } else {
                HandleTick(req, send);
            }
            break;
        }
        case Endpoint::Unknown:
            SendErrorResponse(req, send, http::status::bad_request,
                "badRequest", "Bad request");
            break;
    }
}

template <typename Body, typename Send>
bool ApiHandler::ValidateMethod(const http::request<Body>& req,
                                const std::vector<http::verb>& allowed_methods,
                                Send&& send) {
    auto it = std::find(allowed_methods.begin(), allowed_methods.end(), req.method());
    if (it != allowed_methods.end()) {
        return true;
    }

    // Формируем список разрешенных методов для заголовка Allow
    std::string allow_header;
    for (size_t i = 0; i < allowed_methods.size(); ++i) {
        allow_header += to_string(allowed_methods[i]);
        if (i + 1 < allowed_methods.size()) {
            allow_header += ", ";
        }
    }

    SendErrorResponse(req, send, http::status::method_not_allowed,
        "invalidMethod", "Invalid method", allow_header);
    return false;
}

template <typename Body, typename Send>
bool ApiHandler::ValidateContentType(const http::request<Body>& req,
                                     const std::string& expected_type,
                                     Send&& send) {
    auto it = req.find(http::field::content_type);
    if (it == req.end() || it->value() != expected_type) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", 
            std::string("Invalid Content-Type. Expected: ") + expected_type);
        return false;
    }
    return true;
}

template <typename Body>
std::optional<json::value> ApiHandler::ParseJsonBody(const http::request<Body>& req) {
    try {
        return json::parse(req.body());
    } catch (...) {
        return std::nullopt;
    }
}

template <typename Body>
std::optional<Token> ApiHandler::ExtractBearerToken(const http::request<Body>& req) {
    auto auth_it = req.find(http::field::authorization);
    if (auth_it == req.end()) {
        return std::nullopt;
    }

    std::string auth = std::string(auth_it->value());
    const std::string prefix = "Bearer ";
    
    if (!auth.starts_with(prefix) || auth.size() <= prefix.size()) {
        return std::nullopt;
    }

    return Token{auth.substr(prefix.size())};
}

template <typename Body, typename Send>
std::shared_ptr<Player> ApiHandler::AuthenticatePlayer(const http::request<Body>& req,
                                                       Send&& send) {
    // Проверяем заголовок Authorization
    auto auth_it = req.find(http::field::authorization);
    if (auth_it == req.end()) {
        SendErrorResponse(req, send, http::status::unauthorized,
            "invalidToken", "Authorization header is missing");
        return nullptr;
    }

    std::string auth = std::string(auth_it->value());

    // Формат: Bearer <token>
    const std::string prefix = "Bearer ";
    if (auth.rfind(prefix, 0) != 0 || auth.size() <= prefix.size()) {
        SendErrorResponse(req, send, http::status::unauthorized,
            "invalidToken", "Invalid Authorization header format");
        return nullptr;
    }

    std::string token_str = auth.substr(prefix.size());
    Token token{token_str};

    // Проверяем, существует ли такой токен
    auto player = app_.FindPlayer(token);
    
    if (!player) {
        // Токен не найден вообще
        if (IsValidFormat(token_str)) {
            // Если токен валидного формата, но игрока нет
            SendErrorResponse(req, send, http::status::unauthorized,
                "unknownToken", "Player token has not been found");
        } else {
            // Токен невалидный формат
            SendErrorResponse(req, send, http::status::unauthorized,
                "invalidToken", "Player token has not been found");
        }
        return nullptr;
    }

    return player;
}

template <typename Body, typename Send>
void ApiHandler::HandleMaps(const http::request<Body>& req, Send&& send) {
    if (!ValidateMethod(req, {http::verb::get, http::verb::head}, send)) {
        return;
    }

    SendJsonResponse(req, send, http::status::ok, 
        json::serialize(CreateMapsJson()));
}

template <typename Body, typename Send>
void ApiHandler::HandleMapById(const http::request<Body>& req, Send&& send,
                               const std::string& map_id_str) {
    if (!ValidateMethod(req, {http::verb::get, http::verb::head}, send)) {
        return;
    }

    if (map_id_str.empty()) {
        SendErrorResponse(req, send, http::status::bad_request,
            "badRequest", "Invalid map ID");
        return;
    }

    model::Map::Id map_id{map_id_str};
    const model::Map* map = app_.FindMap(map_id);

    if (!map) {
        SendErrorResponse(req, send, http::status::not_found,
            "mapNotFound", "Map not found");
        return;
    }

    SendJsonResponse(req, send, http::status::ok,
        json::serialize(CreateMapJson(*map)));
}

template <typename Body, typename Send>
void ApiHandler::HandleJoin(const http::request<Body>& req, Send&& send) {
    if (!ValidateMethod(req, {http::verb::post}, send)) {
        return;
    }

    if (!ValidateContentType(req, "application/json", send)) {
        return;
    }

    auto json_body = ParseJsonBody(req);
    if (!json_body || !json_body->is_object()) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Invalid JSON object");
        return;
    }

    const auto& obj = json_body->as_object();
    
    std::string user_name, map_id_str;
    if (!ExtractStringField(obj, "userName", user_name) ||
        !ExtractStringField(obj, "mapId", map_id_str)) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Invalid parameters");
        return;
    }

    if (user_name.empty()) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Invalid name");
        return;
    }

    model::Map::Id map_id{map_id_str};
    if (!app_.FindMap(map_id)) {
        SendErrorResponse(req, send, http::status::not_found,
            "mapNotFound", "Map not found");
        return;
    }

    try {
        auto player = app_.JoinGame(map_id, user_name);

        std::ostringstream out;
        out << "{"
            << "\"authToken\":\"" << *player->GetToken() << "\","
            << "\"playerId\":" << player->GetDog().GetId()
            << "}";

        SendJsonResponse(req, send, http::status::ok, out.str());
    } catch (const std::exception& e) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", e.what());
    }
}

template <typename Body, typename Send>
void ApiHandler::HandlePlayers(const http::request<Body>& req, Send&& send) {
    // Проверка метода
    if (!ValidateMethod(req, {http::verb::get, http::verb::head}, send)) {
        return;
    }

    // Проверка авторизации
    auto player = AuthenticatePlayer(req, send);
    if (!player) {
        return;
    }

    // Для HEAD запроса возвращаем только заголовки
    if (req.method() == http::verb::head) {
        SendJsonResponse(req, send, http::status::ok, "{}");
        return;
    }

    // Получаем всех игроков на карте
    auto map_id = player->GetSession()->GetMapId();
    auto others = app_.GetPlayers().GetPlayersOnMap(map_id);

    std::ostringstream out;
    out << "{";

    for (size_t i = 0; i < others.size(); ++i) {
        auto& p = others[i];
        const auto& dog = p->GetDog();
        out << "\"" << dog.GetId() << "\": {\"name\": \"" << dog.GetName() << "\"}";
        if (i + 1 < others.size()) out << ",\n";
    }

    out << "}";

    SendJsonResponse(req, send, http::status::ok, out.str());
}

template <typename Body, typename Send>
void ApiHandler::HandleState(const http::request<Body>& req, Send&& send) {
    if (!ValidateMethod(req, {http::verb::get, http::verb::head}, send)) {
        return;
    }

    auto player = AuthenticatePlayer(req, send);
    if (!player) {
        return;
    }

    auto session = player->GetSession();
    const auto& dogs = session->GetDogs();
    const auto& loot_objects = session->GetLootObjects();

    std::ostringstream out;
    out << std::fixed << std::setprecision(18);  

    out << "{\n";
    out << "  \"players\": {\n";

    bool first_dog = true;
    for (const auto& dog_ptr : dogs) {
        if (!dog_ptr) continue;

        const auto& dog = *dog_ptr;
        
        if (!first_dog) {
            out << ",\n";
        }
        first_dog = false;

        out << "    \"" << dog.GetId() << "\": {\n"
            << "      \"pos\": [" << dog.GetPosition().x 
            << ", " << dog.GetPosition().y << "],\n"
            << "      \"speed\": [" << dog.GetSpeed().x 
            << ", " << dog.GetSpeed().y << "],\n"
            << "      \"dir\": \"" << DirectionToString(dog.GetDirection()) << "\"\n"
            << "    }";
    }

    out << "\n  },\n";
    
    out << "  \"lostObjects\": {\n";
    
    bool first_loot = true;
    for (const auto& [loot_id, loot_obj] : loot_objects) {
        if (!first_loot) {
            out << ",\n";
        }
        first_loot = false;
        
        out << "    \"" << *loot_obj.id << "\": {\n"
            << "      \"type\": " << loot_obj.type << ",\n"
            << "      \"pos\": [" << loot_obj.x 
            << ", " << loot_obj.y << "]\n"
            << "    }";
    }
    
    out << "\n  }\n";
    out << "}\n";
    
    SendJsonResponse(req, send, http::status::ok, out.str());
}

template <typename Body, typename Send>
void ApiHandler::HandlePlayerAction(const http::request<Body>& req, Send&& send) {
    if (!ValidateMethod(req, {http::verb::post}, send)) {
        return;
    }

    if (!ValidateContentType(req, "application/json", send)) {
        return;
    }

    auto player = AuthenticatePlayer(req, send);
    if (!player) {
        return;
    }

    auto json_body = ParseJsonBody(req);
    if (!json_body || !json_body->is_object()) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Failed to parse action");
        return;
    }

    const auto& obj = json_body->as_object();
    
    std::string move;
    if (!ExtractStringField(obj, "move", move)) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Failed to parse action");
        return;
    }

    // Валидация направления движения
    static const std::vector<std::string> valid_moves = {"L", "R", "U", "D", ""};
    if (std::find(valid_moves.begin(), valid_moves.end(), move) == valid_moves.end()) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Invalid move direction");
        return;
    }

    // Применяем движение
    auto& dog = player->GetDog();
    auto map_id = player->GetSession()->GetMapId();
    const auto* map = app_.FindMap(map_id);
    
    if (!map) {
        SendErrorResponse(req, send, http::status::not_found,
            "mapNotFound", "Map not found");
        return;
    }

    double dog_speed = map->GetDogSpeed();
    ApplyMovement(dog, move, dog_speed);
    
    // Обновляем собаку в сессии
    if (auto session_dog = FindDogInSession(player->GetSession(), dog.GetId())) {
        session_dog->SetSpeed(dog.GetSpeed());
        session_dog->SetDirection(dog.GetDirection());
    }

    SendJsonResponse(req, send, http::status::ok, "{}\n");
}

template <typename Body, typename Send>
void ApiHandler::HandleTick(const http::request<Body>& req, Send&& send) {
    if (!ValidateMethod(req, {http::verb::post}, send)) {
        return;
    }

    if (!ValidateContentType(req, "application/json", send)) {
        return;
    }

    auto json_body = ParseJsonBody(req);
    if (!json_body || !json_body->is_object()) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Failed to parse tick request");
        return;
    }

    const auto& obj = json_body->as_object();
    
    int64_t time_delta;
    if (!ExtractInt64Field(obj, "timeDelta", time_delta)) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "Invalid timeDelta parameter");
        return;
    }

    if (time_delta < 0) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", "timeDelta must be non-negative");
        return;
    }

    try {
        app_.Tick(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::microseconds(time_delta)
        ));
        SendJsonResponse(req, send, http::status::ok, "{}");
    } catch (const std::runtime_error& e) {
        SendErrorResponse(req, send, http::status::bad_request,
            "invalidArgument", e.what());
    }
}

template <typename Body, typename Send>
void ApiHandler::SendJsonResponse(const http::request<Body>& req,
                                  Send&& send,
                                  http::status status,
                                  const std::string& body,
                                  const std::string& allow_header) {
    
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, ContentType::APPLICATION_JSON);
    res.set(http::field::cache_control, "no-cache");
    
    if (!allow_header.empty()) {
        res.set(http::field::allow, allow_header);
    }
    
    res.keep_alive(req.keep_alive());
    res.body() = body;
    res.prepare_payload();
    
    send(std::move(res));
}

template <typename Body, typename Send>
void ApiHandler::SendErrorResponse(const http::request<Body>& req,
                                   Send&& send,
                                   http::status status,
                                   const std::string& code,
                                   const std::string& message,
                                   const std::string& allow_header) {
    
    json::object error_response{
        {"code", code},
        {"message", message}
    };
    
    SendJsonResponse(req, send, status,
        json::serialize(error_response), allow_header);
}

} // namespace api