#include <fstream>
#include <sstream>

#include "json_loader.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла

    model::Game game;

    std::ifstream ifs(json_path); 
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string json_str = buffer.str();

    json::value value;
    try {
        value = boost::json::parse(json_str);
    } catch (const boost::json::system_error& e) {
        throw std::runtime_error(
            "JSON parsing failed in file " + json_path.string() + ": " + e.what()
        );
    }

    AddMapsToGame(game, value);

    return game;
}

void AddMapsToGame(model::Game& game, const json::value& value) {
    // Получаем объект верхнего уровня
    json::object obj = value.as_object();

    // получаем массив карт
    json::array maps = obj["maps"].as_array();
    for (auto& item : maps) {
        AddMapToGame(game, item);
    }
}

void AddMapToGame(model::Game& game, const boost::json::value& item) {
    auto& map_obj = item.as_object();
    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();
    // создали карту
    model::Map map(model::Map::Id{id}, name);

    AddRoadsToMap(map, map_obj);
    AddBuildingsToMap(map, map_obj);
    AddOfficesToMap(map, map_obj);
    
    game.AddMap(map);
}

// добавляем на карту дороги
void AddRoadsToMap(model::Map& map, const json::object& map_obj) {
    json::array roads = map_obj.at("roads").as_array();
    for (auto& road : roads) {
        auto& road_obj = road.as_object();
        auto x0 = static_cast<int>(road_obj["x0"].as_int64());
        auto y0 = static_cast<int>(road_obj["y0"].as_int64());
        if (road_obj.contains("x1")) {
            auto x1 = static_cast<int>(road_obj["x1"].as_int64());
            model::Road r(model::Road::HORIZONTAL, {x0, y0}, x1);
            map.AddRoad(r);
        } else {
            auto y1 = static_cast<int>(road_obj["y1"].as_int64());
            model::Road r(model::Road::VERTICAL, {x0, y0}, y1);
            map.AddRoad(r);
        }
    }
}


// добавляем на карту здания
void AddBuildingsToMap(model::Map& map, const json::object& map_obj) {
    json::array buildings = map_obj.at("buildings").as_array();
    for (auto& build : buildings) {
        auto& build_obj = build.as_object();
        auto x = static_cast<int>(build_obj["x"].as_int64());
        auto y = static_cast<int>(build_obj["y"].as_int64());
        auto w = static_cast<int>(build_obj["w"].as_int64());
        auto h = static_cast<int>(build_obj["h"].as_int64());

        model::Building building(model::Rectangle{ {x, y}, {w, h} });

        map.AddBuilding(building);
    }
}


// добавляем на карту офисы
void AddOfficesToMap(model::Map& map, const json::object& map_obj) {
    json::array offices = map_obj.at("offices").as_array();
    for (auto& off : offices) {
        auto& of = off.as_object();
        auto id = of["id"].as_string().c_str();
        int x = of["x"].as_int64();
        int y = of["y"].as_int64();
        int offsetX = of["offsetX"].as_int64();
        int offsetY = of["offsetY"].as_int64();

        model::Office office(model::Office::Id{id}, {x, y}, {offsetX, offsetY});
        map.AddOffice(office);
    }
}

}  // namespace json_loader
