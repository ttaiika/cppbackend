#include "app/application.h"
#include <fstream>
#include <boost/json.hpp>

namespace json = boost::json;

Application::Application(const std::filesystem::path& config)
    : manual_ticker_(true), random_spawn_(false) { 
    auto result = json_loader::LoadGame(config);
    game_ = std::move(result.game);
    
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
    
    // Уведомляем всех слушателей о тике
    for (auto listener : listeners_) {
        listener->OnTick(ms);
    }
}

const extra_data::MapExtraData* Application::GetExtraData() const {
    return extra_data_.get();
}

const model::LootGeneratorConfig& Application::GetLootConfig() const {
    return loot_config_;
}

std::shared_ptr<GameSession> Application::FindSession(const model::Map::Id& map_id) const {
    auto sessions = game_.GetSessions();
    for (const auto& session : sessions) {
        if (session->GetMapId() == map_id) {
            return session;
        }
    }
    return nullptr;
}

void Application::AddListener(ApplicationListener& listener) {
    listeners_.push_back(&listener);
}

model::Game& Application::GetGame() {
    return game_;
}

const model::Game& Application::GetGame() const {
    return game_;
}

const std::vector<std::shared_ptr<GameSession>>& Application::GetSessions() const {
    return game_.GetSessions();
}