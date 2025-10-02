#pragma once

#include <boost/json.hpp>
#include <filesystem>

#include "model.h"

namespace json_loader {
namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path);

// добавляем все карты в игру
void AddMapsToGame(model::Game& game, const json::value& value);

// вспомогательная ф-я добавления одной игры
void AddMapToGame(model::Game& game, const boost::json::value& item);

// добавляем на карту дороги
void AddRoadsToMap(model::Map& map, const json::object& map_obj);

// добавляем на карту здания
void AddBuildingsToMap(model::Map& map, const json::object& map_obj);

// добавляем на карту офисы
void AddOfficesToMap(model::Map& map, const json::object& map_obj);

}  // namespace json_loader
