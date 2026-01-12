#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <memory>

#include "model/model.h"
#include "app/players.h"
#include "json_loader.h"
#include "extra_data.h"
#include "model/loot.h"

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

    const extra_data::MapExtraData* GetExtraData() const {
        return extra_data_.get();
    }

    const model::LootGeneratorConfig& GetLootConfig() const {
        return loot_config_;
    }

    std::shared_ptr<GameSession> FindSession(const model::Map::Id& map_id) const {
        auto sessions = game_.GetSessions();
        for (const auto& session : sessions) {
            if (session->GetMapId() == map_id) {
                return session;
            }
        }
        return nullptr;
    }

private:
    model::Game game_;
    bool manual_ticker_;
    bool random_spawn_;
    std::chrono::milliseconds tick_period_{0};
    std::shared_ptr<extra_data::MapExtraData> extra_data_;
    model::LootGeneratorConfig loot_config_;
};