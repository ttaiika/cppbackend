#include "model/model.h"
#include "model/game_session.h"
#include "model/dog.h"
#include "app/players.h"

#include <stdexcept>
#include <random>

namespace model {
    
using namespace std::literals;

bool Road::Contains(const Point& point) const {
  return point.x >= m_min_X && point.x <= m_max_X && point.y >= m_min_Y && point.y <= m_max_Y;
}

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

Game::Game(const LootGeneratorConfig& loot_config)
    : players_(std::make_unique<Players>())
    , loot_config_(loot_config) {
}

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
    auto session = std::make_shared<GameSession>(map_ptr, loot_config_);
    sessions_.push_back(session);
    return session;
}

double GetRandomDouble(double min, double max) {
    static std::random_device rd;       // системное случайное зерно
    static std::mt19937 gen(rd());      // генератор Мерсенна
    std::uniform_real_distribution<double> dis(min, max);
    return dis(gen);
}

Position GetRandomPointOnRoad(const model::Road& road) {
    Point start = road.GetStart();
    Point end = road.GetEnd();

    double t = GetRandomDouble(0.0, 1.0); // число от 0 до 1

    return {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t
    };
}

Position GetRandomSpawnPoint(const model::Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        throw std::runtime_error("Map has no roads");
    }

    // случайная дорога
    size_t idx = static_cast<size_t>(GetRandomDouble(0, roads.size()));
    if (idx >= roads.size()) idx = roads.size() - 1; // защита на границе

    return GetRandomPointOnRoad(roads[idx]);
}

std::shared_ptr<Player> Game::AddPlayer(const std::string& name, Map::Id id, bool random_spawn) {
    uint32_t dog_id = next_dog_id_++;

    auto dog = std::make_shared<Dog>(dog_id, name);

    const Map* map = FindMap(id);
    if (!map) {
        throw std::runtime_error("Map not found");
    }

    const auto& roads = map->GetRoads();
    if (roads.empty()) throw std::runtime_error("Map has no roads");

    // Создаем или находим сессию и добавляем собаку
    auto session = FindOrCreateSession(id);
    session->AddDog(dog);

    auto player = players_->AddPlayer(dog, session);

    double x = 0.0, y = 0.0;

    if (random_spawn) {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);

        const auto& road = roads[road_dist(gen)];
        std::uniform_real_distribution<double> t_dist(0.0, 1.0);
        double t = t_dist(gen);

        x = static_cast<double>(road.GetStart().x) + t * (road.GetEnd().x - road.GetStart().x);
        y = static_cast<double>(road.GetStart().y) + t * (road.GetEnd().y - road.GetStart().y);
    } else {
        const auto& road = roads.front();
        x = static_cast<double>(road.GetStart().x);
        y = static_cast<double>(road.GetStart().y);
    }

    player->GetDog().SetPosition({ x, y });

    return player;
}

Game::~Game() = default;

}  // namespace model
