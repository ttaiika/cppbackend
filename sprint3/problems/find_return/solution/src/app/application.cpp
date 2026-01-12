#include "app/application.h"
#include <fstream>
#include <boost/json.hpp>

namespace json = boost::json;

// Обновленный конструктор Application
Application::Application(const std::filesystem::path& config)
    : manual_ticker_(true) {
    // Используем обновленную LoadGame, которая возвращает все необходимые данные
    auto result = json_loader::LoadGame(config);
    
    // Перемещаем игру
    game_ = std::move(result.game);
    
    // Сохраняем extra_data и конфигурацию лута
    extra_data_ = std::move(result.extra_data);
    loot_config_ = result.loot_config;
}

void Application::SetTickPeriod(std::chrono::milliseconds period) {
    manual_ticker_ = false;
    tick_period_ = period;
}

bool Application::IsManualTicker() const {
  return manual_ticker_;
}

void Application::SetRandomSpawn(bool enable){
    random_spawn_ = enable;
}

const model::Map* Application::FindMap(const model::Map::Id &id) const noexcept {
    return game_.FindMap(id);
}

const model::Game::Maps& Application::GetMaps() const noexcept {
    return game_.GetMaps();
}

std::shared_ptr<Player> Application::JoinGame(model::Map::Id id, const std::string &user_name) {
    return game_.AddPlayer(user_name, id, random_spawn_);
}

std::shared_ptr<Player> Application::FindPlayer(const Token &token) const {
    return GetPlayers().FindByToken(token);
}

const Players& Application::GetPlayers() const noexcept {
    return game_.GetPlayers();
}

void Application::Tick(std::chrono::milliseconds ms) {
    if(ms.count() > 30000ul){   // fail if more than 30s
      throw std::runtime_error("Time for aplication update state is very long");
    }

    double delta = std::chrono::duration<double>(ms).count();
    game_.Tick(delta);
}