#include <fstream>
#include <sstream>

#include "app/players.h"
#include "json_loader.h"

namespace json_loader {

namespace json = boost::json;

GameLoadResult LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path
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

    const auto& obj = value.as_object();
    
    // Загружаем конфигурацию генератора лута
    model::LootGeneratorConfig loot_config{5.0, 0.5};
    if (obj.contains("lootGeneratorConfig")) {
        const auto& loot_gen_json = obj.at("lootGeneratorConfig").as_object();
        loot_config.period = loot_gen_json.at("period").as_double();
        loot_config.probability = loot_gen_json.at("probability").as_double();
    }
    
    // Создаем игру с конфигурацией лута
    model::Game game(loot_config);
    
    // Создаем extra_data для хранения полного описания lootTypes
    auto extra_data = std::make_shared<extra_data::MapExtraData>();
    
    AddMapsToGame(game, extra_data, value, loot_config);
    
    return {std::move(game), std::move(extra_data), loot_config};
}

void AddMapsToGame(model::Game& game, 
                   std::shared_ptr<extra_data::MapExtraData> extra_data,
                   const json::value& value,
                   const model::LootGeneratorConfig& loot_config) {
    // Получаем объект верхнего уровня
    json::object obj = value.as_object();

    double default_speed = 1.0;
    if (obj.contains("defaultDogSpeed")) {
        default_speed = obj.at("defaultDogSpeed").as_double();
    }

    int default_capacity = 3;
    if (obj.contains("defaultBagCapacity")) {
        default_capacity = obj.at("defaultBagCapacity").as_int64();
    }

    // получаем массив карт
    json::array maps = obj["maps"].as_array();
    for (const auto& item : maps) {
        AddMapToGame(game, extra_data, item, loot_config, default_speed, default_capacity);
    }
}

void AddMapToGame(model::Game& game,
                  std::shared_ptr<extra_data::MapExtraData> extra_data,
                  const json::value& item,
                  const model::LootGeneratorConfig& loot_config, double speed, int capacity) {
    const auto& map_obj = item.as_object();

    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();

    if (map_obj.contains("dogSpeed")) {
        speed = map_obj.at("dogSpeed").as_double();
    }

    if (map_obj.contains("bagCapacity")) {
        capacity = map_obj.at("bagCapacity").as_int64();
    }

    model::Map map(model::Map::Id{id}, name, speed, capacity);

    AddRoadsToMap(map, map_obj);
    AddBuildingsToMap(map, map_obj);
    AddOfficesToMap(map, map_obj);
    
    // Загружаем lootTypes для extra_data
    if (map_obj.contains("lootTypes")) {
        const auto& loot_types_json = map_obj.at("lootTypes").as_array();
        std::vector<json::value> loot_types;
        
        for (const auto& loot_type_json : loot_types_json) {
            loot_types.push_back(loot_type_json);
        }
        
        extra_data->SetLootTypes(id, std::move(loot_types));
        map.SetLootTypesCount(loot_types.size());
        map.SetExtraData(extra_data);
    }
    
    game.AddMap(std::move(map));
}

void AddRoadsToMap(model::Map& map, const json::object& map_obj) {
    json::array roads = map_obj.at("roads").as_array();
    for (const auto& road : roads) {
        auto& road_obj = road.as_object();
        auto x0 = static_cast<int>(road_obj.at("x0").as_int64());
        auto y0 = static_cast<int>(road_obj.at("y0").as_int64());
        if (road_obj.contains("x1")) {
            auto x1 = static_cast<int>(road_obj.at("x1").as_int64());
            model::Road r(model::Road::HORIZONTAL, {x0, y0}, x1);
            map.AddRoad(r);
        } else {
            auto y1 = static_cast<int>(road_obj.at("y1").as_int64());
            model::Road r(model::Road::VERTICAL, {x0, y0}, y1);
            map.AddRoad(r);
        }
    }
}

void AddBuildingsToMap(model::Map& map, const json::object& map_obj) {
    json::array buildings = map_obj.at("buildings").as_array();
    for (const auto& build : buildings) {
        const auto& build_obj = build.as_object();
        auto x = static_cast<int>(build_obj.at("x").as_int64());
        auto y = static_cast<int>(build_obj.at("y").as_int64());
        auto w = static_cast<int>(build_obj.at("w").as_int64());
        auto h = static_cast<int>(build_obj.at("h").as_int64());

        model::Building building(geom::Rectangle{ {x, y}, {w, h} });

        map.AddBuilding(building);
    }
}

void AddOfficesToMap(model::Map& map, const json::object& map_obj) {
    json::array offices = map_obj.at("offices").as_array();
    for (const auto& off : offices) {
        const auto& of = off.as_object();

        auto id = of.at("id").as_string().c_str();
        int x = of.at("x").as_int64();
        int y = of.at("y").as_int64();
        int offsetX = of.at("offsetX").as_int64();
        int offsetY = of.at("offsetY").as_int64();

        model::Office office(model::Office::Id{id}, {x, y}, {offsetX, offsetY});
        map.AddOffice(office);
    }
}

}  // namespace json_loader