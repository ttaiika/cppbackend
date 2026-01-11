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

// Основная функция загрузки (возвращает структуру с игрой, extra_data и конфигурацией лута)
GameLoadResult LoadGame(const std::filesystem::path& json_path);

// Старая функция для обратной совместимости (если нужно)
model::Game LoadGameLegacy(const std::filesystem::path& json_path);

// Вспомогательные функции
void AddMapsToGame(model::Game& game, 
                   std::shared_ptr<extra_data::MapExtraData> extra_data,
                   const json::value& value,
                   const model::LootGeneratorConfig& loot_config);

void AddMapToGame(model::Game& game,
                  std::shared_ptr<extra_data::MapExtraData> extra_data,
                  const json::value& item,
                  double default_speed,
                  const model::LootGeneratorConfig& loot_config);

void AddRoadsToMap(model::Map& map, const json::object& map_obj);
void AddBuildingsToMap(model::Map& map, const json::object& map_obj);
void AddOfficesToMap(model::Map& map, const json::object& map_obj);

}  // namespace json_loader