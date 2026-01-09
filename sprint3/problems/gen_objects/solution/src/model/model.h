#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "utils/tagged.h"
#include "utils/tags.h"

#include "app/player.h"
#include "model/game_session.h"
#include "model/geom.h"

class Players;

namespace model {

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    static constexpr double HALF_WIDTH = 0.4;

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y}
        , m_min_X( start.x - Road::HALF_WIDTH)
        , m_max_X( start.x + Road::HALF_WIDTH)
        , m_min_Y( std::min(start.y, end_y) - Road::HALF_WIDTH)
        , m_max_Y( std::max(start.y, end_y) + Road::HALF_WIDTH) {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

    bool Contains(const Point& point) const;

    const double& GetMinX() const {return m_min_X;}
    const double& GetMaxX() const {return m_max_X;}
    const double& GetMinY() const {return m_min_Y;}
    const double& GetMaxY() const {return m_max_Y;}

private:
    Point start_;
    Point end_;

    double m_min_X;
    double m_max_X;
    double m_min_Y;
    double m_max_Y;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, double dog_speed) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , dog_speed_(dog_speed) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    void SetDogSpeed(double speed) noexcept {
        dog_speed_ = speed;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

    void SetLootTypesCount(size_t count) {
        loot_types_count_ = count;
    }
    
    size_t GetLootTypesCount() const {
        return loot_types_count_;
    }
    
    // Генерация случайной точки на дорогах карты
    Point GenerateRandomPosition() const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        if (roads_.empty()) {
            return {0, 0};
        }
        
        // Выбираем случайную дорогу
        std::uniform_int_distribution<size_t> road_dist(0, roads_.size() - 1);
        const Road& road = roads_[road_dist(gen)];
        
        // Генерируем случайную точку на дороге
        std::uniform_real_distribution<double> t_dist(0.0, 1.0);
        double t = t_dist(gen);
        
        return {
            static_cast<Coord>(road.GetStart().x + t * (road.GetEnd().x - road.GetStart().x)),
            static_cast<Coord>(road.GetStart().y + t * (road.GetEnd().y - road.GetStart().y))
        };
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    double dog_speed_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    size_t loot_types_count_ = 0;  // Количество типов лута на карте
};

class Game {
public:
    Game(const LootGeneratorConfig& loot_config = {5.0, 0.5});
    
    void SetLootConfig(const LootGeneratorConfig& config) {
        loot_config_ = config;
    }

    using Maps = std::vector<Map>;

     // запретить копирование
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // разрешить перемещение
    Game(Game&&) noexcept = default;
    Game& operator=(Game&&) noexcept = default;

    void AddMap(Map map);

    std::shared_ptr<GameSession> FindOrCreateSession(Map::Id id);

    std::shared_ptr<Player> AddPlayer(const std::string& name, Map::Id id, bool random_spawn);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    Players& GetPlayers() {
        return *players_;
    }

    const Players& GetPlayers() const {
        return *players_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    void Tick(int ms) {
        double dt_seconds = static_cast<double>(ms) / 1000.0;
        for (auto& session : sessions_) {
            if (session) {
                session->Tick(dt_seconds);
            }
        }
    }

    const std::vector<std::shared_ptr<GameSession>>& GetSessions() const {
        return sessions_;
    }

    ~Game();

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    std::vector<std::shared_ptr<GameSession>> sessions_;
    std::unique_ptr<Players> players_;
    uint32_t next_dog_id_ = 0;
    LootGeneratorConfig loot_config_;
};

}  // namespace model