#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <memory>
#include <vector>

#include "model/model.h"
#include "app/players.h"
#include "json_loader.h"
#include "extra_data.h"
#include "model/loot.h"
#include "app/application_listener.h"

class Application {
public:
    explicit Application(const std::filesystem::path& config);

    Application() = delete;
    Application(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;

    void SetTickPeriod(std::chrono::milliseconds period);

    bool IsManualTicker() const;

    void SetRandomSpawn(bool enable = true);

    const model::Map* FindMap(const model::Map::Id& id) const noexcept;

    const model::Game::Maps& GetMaps() const noexcept;

    std::shared_ptr<Player> JoinGame(model::Map::Id id, const std::string& user_name);

    std::shared_ptr<Player> FindPlayer(const Token& token) const;

    const Players& GetPlayers() const noexcept;

    void Tick(std::chrono::milliseconds ms);

    const extra_data::MapExtraData* GetExtraData() const;

    const model::LootGeneratorConfig& GetLootConfig() const;

    std::shared_ptr<GameSession> FindSession(const model::Map::Id& map_id) const;

    void AddListener(ApplicationListener& listener);
    
    model::Game& GetGame();

    const model::Game& GetGame() const; 

    const std::vector<std::shared_ptr<GameSession>>& GetSessions() const;

private:
    model::Game game_;
    bool manual_ticker_;
    bool random_spawn_;
    std::chrono::milliseconds tick_period_{0};
    std::shared_ptr<extra_data::MapExtraData> extra_data_;
    model::LootGeneratorConfig loot_config_;
    
    // Список слушателей для уведомления о тиках
    std::vector<ApplicationListener*> listeners_;
};