#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <memory>

#include "model/model.h"
#include "app/players.h"
#include "json_loader.h"

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

private:
    model::Game game_;
    bool manual_ticker_;
    bool random_spawn_;
    std::chrono::milliseconds tick_period_{0};
};