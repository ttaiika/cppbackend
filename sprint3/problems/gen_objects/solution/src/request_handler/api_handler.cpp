#include "request_handler/api_handler.h"
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>

namespace api {

ApiHandler::ApiHandler(net::strand<net::io_context::executor_type>& strand, Application& app)
    : strand_(strand), app_(app) {}

void ApiHandler::StartAutoTick(std::chrono::milliseconds period) { 
    if (ticker_) {
        return; // уже запущен
    }

    // Если ручной тик включен, автоматический не запускаем
    if (app_.IsManualTicker()) {
        return;
    }

    ticker_ = std::make_shared<Ticker>(
        strand_,
        period,
        [this](std::chrono::milliseconds delta) {
            this->Tick(delta);
        }
    );

    ticker_->Start();
}

bool ApiHandler::IsValidFormat(std::string value) const {
    if (value.size() != 32) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c){
        return std::isxdigit(c);
    });
}

// Маршрутизация запросов
ApiHandler::Endpoint ApiHandler::RouteRequest(const std::string& target, std::string& param) const {
    if (!target.starts_with(API_PREFIX)) {
        return Endpoint::Unknown;
    }

    std::string path = target.substr(API_PREFIX.size());

    if (path == MAPS_ENDPOINT) {
        return Endpoint::Maps;
    }

    if (path.starts_with(std::string(MAPS_ENDPOINT) + "/")) {
        param = path.substr(std::string(MAPS_ENDPOINT).size() + 1);
        return Endpoint::MapById;
    }

    if (path == JOIN_ENDPOINT) {
        return Endpoint::Join;
    }

    if (path == PLAYERS_ENDPOINT) {
        return Endpoint::Players;
    }

    if (path == STATE_ENDPOINT) {
        return Endpoint::State;
    }

    if (path == PLAYER_ACTION_ENDPOINT) {
        return Endpoint::PlayerAction;
    }

    if (path == TICK_ENDPOINT) {
        return Endpoint::Tick;
    }

    return Endpoint::Unknown;
}

// Метод Tick для автоматических тиков
void ApiHandler::Tick(std::chrono::milliseconds delta) {
    app_.Tick(delta);
}

// Поиск собаки в сессии
model::Dog* ApiHandler::FindDogInSession(std::shared_ptr<GameSession> session, uint32_t dog_id) {
    const auto& dogs = session->GetDogs();
    auto it = std::find_if(
        dogs.begin(),
        dogs.end(),
        [&](const auto& dog_ptr) {
            return dog_ptr && dog_ptr->GetId() == dog_id;
        }
    );

    return it != dogs.end() ? it->get() : nullptr;
}

// Применение движения
void ApiHandler::ApplyMovement(model::Dog& dog, const std::string& move, double speed) {
    if (move == "L") {
        dog.SetSpeed({-speed, 0});
        dog.SetDirection(model::Direction::WEST);
    } else if (move == "R") {
        dog.SetSpeed({speed, 0});
        dog.SetDirection(model::Direction::EAST);
    } else if (move == "U") {
        dog.SetSpeed({0, -speed});
        dog.SetDirection(model::Direction::NORTH);
    } else if (move == "D") {
        dog.SetSpeed({0, speed});
        dog.SetDirection(model::Direction::SOUTH);
    } else {
        dog.SetSpeed({0, 0});
    }
}

// Преобразование направления в строку
std::string ApiHandler::DirectionToString(model::Direction dir) {
    switch (dir) {
        case model::Direction::NORTH: return "U";
        case model::Direction::SOUTH: return "D";
        case model::Direction::WEST:  return "L";
        case model::Direction::EAST:  return "R";
        default: return "U";
    }
}

// Извлечение строкового поля из JSON
bool ApiHandler::ExtractStringField(const json::object& obj,
                                    const std::string& field_name,
                                    std::string& out_value) {
    auto it = obj.find(field_name);
    if (it == obj.end() || !it->value().is_string()) {
        return false;
    }
    out_value = it->value().as_string().c_str();
    return true;
}

// Извлечение числового поля из JSON
bool ApiHandler::ExtractInt64Field(const json::object& obj,
                                   const std::string& field_name,
                                   int64_t& out_value) {
    auto it = obj.find(field_name);
    if (it == obj.end() || !it->value().is_int64()) {
        return false;
    }
    out_value = it->value().as_int64();
    return true;
}

// Создание JSON для дороги
json::object ApiHandler::CreateRoadJson(const model::Road& road) {
    json::object j;
    j["x0"] = road.GetStart().x;
    j["y0"] = road.GetStart().y;
    
    if (road.IsHorizontal()) {
        j["x1"] = road.GetEnd().x;
    } else {
        j["y1"] = road.GetEnd().y;
    }
    
    return j;
}

// Создание JSON для здания
json::object ApiHandler::CreateBuildingJson(const model::Building& building) {
    auto bounds = building.GetBounds();
    return json::object{
        {"x", bounds.position.x},
        {"y", bounds.position.y},
        {"w", bounds.size.width},
        {"h", bounds.size.height}
    };
}

// Создание JSON для офиса
json::object ApiHandler::CreateOfficeJson(const model::Office& office) {
    return json::object{
        {"id", *office.GetId()},
        {"x", office.GetPosition().x},
        {"y", office.GetPosition().y},
        {"offsetX", office.GetOffset().dx},
        {"offsetY", office.GetOffset().dy}
    };
}

// Создание JSON для карты
json::object ApiHandler::CreateMapJson(const model::Map& map) {
    // Создаем массивы
    json::array roads_json;
    for (const auto& road : map.GetRoads()) {
        roads_json.push_back(CreateRoadJson(road));
    }

    json::array buildings_json;
    for (const auto& building : map.GetBuildings()) {
        buildings_json.push_back(CreateBuildingJson(building));
    }

    json::array offices_json;
    for (const auto& office : map.GetOffices()) {
        offices_json.push_back(CreateOfficeJson(office));
    }

    // Формируем базовый объект
    json::object result = json::object{
        {"id", *map.GetId()},
        {"name", map.GetName()},
        {"roads", std::move(roads_json)},
        {"buildings", std::move(buildings_json)},
        {"offices", std::move(offices_json)}
    };

    // Добавляем lootTypes из extra_data (для фронтенда)
    if (auto extra_data = app_.GetExtraData()) {
        if (auto loot_types_opt = extra_data->GetLootTypes(*map.GetId())) {
            // Нужно преобразовать std::vector<json::value> в json::array
            json::array loot_types_array;
            for (const auto& loot_type_value : *loot_types_opt) {
                loot_types_array.push_back(loot_type_value);
            }
            result["lootTypes"] = std::move(loot_types_array);
        }
    }

    return result;
}

// Создание JSON для списка карт
json::array ApiHandler::CreateMapsJson() {
    json::array maps_json;
    const auto& maps = app_.GetMaps();
    
    for (const auto& map : maps) {
        maps_json.push_back(json::object{
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }
    
    return maps_json;
}

} // namespace api