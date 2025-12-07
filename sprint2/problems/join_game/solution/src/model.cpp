#include "model.h"
#include "game_session.h"
#include "dog.h"
#include "players.h"

#include <stdexcept>

namespace model {
    
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

Game::Game() : players_(std::make_unique<Players>()) {}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::shared_ptr<GameSession> Game::FindOrCreateSession(Map::Id id) {
    for (auto& session : sessions_) {
        if (session->GetMapId() == id) {
            return session;
        }
    }

    auto map_ptr = std::make_shared<Map>(*FindMap(id));
    auto session = std::make_shared<GameSession>(map_ptr);
    sessions_.push_back(session);
    return session;
}

std::shared_ptr<Player> Game::AddPlayer(const std::string& name, Map::Id id) {
    uint32_t dog_id = next_dog_id_++;

    auto dog = std::make_unique<Dog>(dog_id, name);
    auto session = FindOrCreateSession(id);

    session->AddDog(std::make_unique<Dog>(*dog));

    return players_->AddPlayer(std::move(dog), session);
}

Game::~Game() = default;

}  // namespace model
