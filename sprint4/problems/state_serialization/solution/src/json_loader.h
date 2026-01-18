#pragma once

#include <filesystem>
#include <boost/json.hpp>

#include "model/model.h"
#include "extra_data.h"

namespace json_loader {

namespace json = boost::json;

// Структура для возврата всех данных загрузки
struct GameLoadResult {
    model::Game game;
    std::shared_ptr<extra_data::MapExtraData> extra_data;
    model::LootGeneratorConfig loot_config;
};

GameLoadResult LoadGame(const std::filesystem::path& json_path);

void AddMapsToGame(model::Game& game, 
                   std::shared_ptr<extra_data::MapExtraData> extra_data,
                   const json::value& value,
                   const model::LootGeneratorConfig& loot_config);

void AddMapToGame(model::Game& game,
                  std::shared_ptr<extra_data::MapExtraData> extra_data,
                  const json::value& item,
                  const model::LootGeneratorConfig& loot_config,
                  double speed, int capacity);

void AddRoadsToMap(model::Map& map, const json::object& map_obj);
void AddBuildingsToMap(model::Map& map, const json::object& map_obj);
void AddOfficesToMap(model::Map& map, const json::object& map_obj);

}  // namespace json_loader