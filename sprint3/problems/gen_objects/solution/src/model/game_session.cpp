#include "model/game_session.h"
#include "model/model.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

constexpr double ROAD_WIDTH = 0.8;
constexpr double ROAD_HALF_WIDTH = ROAD_WIDTH / 2.0;
constexpr double EPSILON = 1e-9;

bool IsNearlyZero(double value) {
    return std::abs(value) < EPSILON;
}

bool IsNearlyEqual(double a, double b) {
    return std::abs(a - b) < EPSILON;
}

struct RoadSegment {
    double min;
    double max;
    double center;
    
    static RoadSegment FromPoints(double p1, double p2) {
        return {std::min(p1, p2), std::max(p1, p2), (p1 + p2) / 2.0};
    }
    
    bool Contains(double point, double margin = 0.0) const {
        return point >= min - margin - EPSILON && 
               point <= max + margin + EPSILON;
    }
};

bool IsPointOnRoad(const model::Position& point, const model::Road& road) {
    if (road.IsHorizontal()) {
        RoadSegment x_segment = RoadSegment::FromPoints(
            road.GetStart().x, road.GetEnd().x);
        RoadSegment y_margin = RoadSegment::FromPoints(
            road.GetStart().y - ROAD_HALF_WIDTH,
            road.GetStart().y + ROAD_HALF_WIDTH);
        
        return x_segment.Contains(point.x, ROAD_HALF_WIDTH) &&
               y_margin.Contains(point.y);
    } else {
        RoadSegment y_segment = RoadSegment::FromPoints(
            road.GetStart().y, road.GetEnd().y);
        RoadSegment x_margin = RoadSegment::FromPoints(
            road.GetStart().x - ROAD_HALF_WIDTH,
            road.GetStart().x + ROAD_HALF_WIDTH);
        
        return y_segment.Contains(point.y, ROAD_HALF_WIDTH) &&
               x_margin.Contains(point.x);
    }
}

int CoordinateToIndex(double coord) {
    return static_cast<int>(std::floor(coord));
}

} // namespace

GameSession::GameSession(std::shared_ptr<model::Map> map, 
                         const model::LootGeneratorConfig& loot_config)
    : map_(std::move(map))
    , loot_generator_(
    loot_gen::LootGenerator::TimeInterval(
        static_cast<int>(loot_config.period * 1000)),
    loot_config.probability,
    []() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 1.0);
        return dis(gen);
    })
{
    BuildRoadIndices();
}

const std::vector<std::shared_ptr<model::Dog>>& GameSession::GetDogs() const {
    return dogs_;
}

const std::unordered_map<model::LootId, model::LootObject>& GameSession::GetLootObjects() const {
    return loot_objects_;
}

void GameSession::BuildRoadIndices() {
    for (const auto& road : map_->GetRoads()) {
        if (road.IsHorizontal()) {
            int y_index = CoordinateToIndex(road.GetStart().y);
            horizontal_roads_.by_primary_coord[y_index].push_back(&road);
            
            RoadSegment x_segment = RoadSegment::FromPoints(
                road.GetStart().x, road.GetEnd().x);
            int min_x_index = CoordinateToIndex(x_segment.min - ROAD_HALF_WIDTH);
            int max_x_index = CoordinateToIndex(x_segment.max + ROAD_HALF_WIDTH);
            
            for (int x = min_x_index; x <= max_x_index; ++x) {
                horizontal_roads_.by_secondary_coord[x].push_back(&road);
            }
        } else {
            int x_index = CoordinateToIndex(road.GetStart().x);
            vertical_roads_.by_primary_coord[x_index].push_back(&road);
            
            RoadSegment y_segment = RoadSegment::FromPoints(
                road.GetStart().y, road.GetEnd().y);
            int min_y_index = CoordinateToIndex(y_segment.min - ROAD_HALF_WIDTH);
            int max_y_index = CoordinateToIndex(y_segment.max + ROAD_HALF_WIDTH);
            
            for (int y = min_y_index; y <= max_y_index; ++y) {
                vertical_roads_.by_secondary_coord[y].push_back(&road);
            }
        }
    }
}

std::shared_ptr<model::Map> GameSession::GetMap() const {
    return map_;
}

Id GameSession::GetMapId() const {
    return map_->GetId();
}

model::Dog& GameSession::AddDog(std::shared_ptr<model::Dog> dog) {
    dogs_.push_back(dog);
    
    if (loot_objects_.size() < GetLootersCount()) {
        // Генерируем лут для нового игрока
        AddRandomLoot();
    }
    
    return *dog;
}

unsigned GameSession::GetLootersCount() const {
    return static_cast<unsigned>(dogs_.size());
}

void GameSession::AddRandomLoot() {
    // Генерируем случайный тип лута
    size_t loot_types_count = map_->GetLootTypesCount();
    
    size_t loot_type = 0;
    if (loot_types_count > 0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(0, loot_types_count - 1);
        loot_type = dis(gen);
    }
    
    const auto& roads = map_->GetRoads();
    
    double pos_x = 0.0, pos_y = 0.0;
    
    if (!roads.empty()) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> road_dis(0, roads.size() - 1);
        const auto& road = roads[road_dis(gen)];
        
        std::uniform_real_distribution<double> t_dis(0.0, 1.0);
        double t = t_dis(gen);
        
        pos_x = static_cast<double>(road.GetStart().x) + t * (road.GetEnd().x - road.GetStart().x);
        pos_y = static_cast<double>(road.GetStart().y) + t * (road.GetEnd().y - road.GetStart().y);
    }
    
    model::LootId id{next_loot_id_++};
    model::LootObject loot{
        .id = id,
        .type = loot_type,
        .x = pos_x,
        .y = pos_y
    };
    
    loot_objects_.emplace(id, loot);
}

void GameSession::UpdateLoot(double dt) {
    // НЕ генерируем лут, если нет игроков
    if (GetLootersCount() == 0) {
        return;
    }
    
    auto delta_ms = loot_gen::LootGenerator::TimeInterval(
        static_cast<int>(dt * 1000));

    // Получаем количество нового лута от генератора
    unsigned new_loot_count = loot_generator_.Generate(
        delta_ms,
        static_cast<unsigned>(loot_objects_.size()),
        GetLootersCount()
    );

    // Добавляем новые предметы
    for (unsigned i = 0; i < new_loot_count; ++i) {
        AddRandomLoot();
    }
    
    if (loot_objects_.size() < GetLootersCount()) {
        // Добавляем недостающий лут
        unsigned loot_needed = GetLootersCount() - static_cast<unsigned>(loot_objects_.size());
        for (unsigned i = 0; i < loot_needed; ++i) {
            AddRandomLoot();
        }
    }
}

const model::Road* GameSession::FindCurrentRoad(
    const model::Position& pos, 
    const model::Speed& speed) const {
    
    // Ищем дороги, на которых находится собака
    std::vector<const model::Road*> candidate_roads;
    
    int y_index = CoordinateToIndex(pos.y);
    if (auto it = horizontal_roads_.by_primary_coord.find(y_index); 
        it != horizontal_roads_.by_primary_coord.end()) {
        for (const auto* road : it->second) {
            if (IsPointOnRoad(pos, *road)) {
                candidate_roads.push_back(road);
            }
        }
    }
    
    int x_index = CoordinateToIndex(pos.x);
    if (auto it = vertical_roads_.by_primary_coord.find(x_index);
        it != vertical_roads_.by_primary_coord.end()) {
        for (const auto* road : it->second) {
            if (IsPointOnRoad(pos, *road)) {
                candidate_roads.push_back(road);
            }
        }
    }
    
    if (candidate_roads.empty()) {
        return nullptr;
    }
    
    // Если собака на перекрестке, выбираем дорогу по направлению движения
    if (candidate_roads.size() > 1) {
        auto it = std::find_if(
            candidate_roads.begin(),
            candidate_roads.end(),
            [&](const model::Road* road) {
                return (!IsNearlyZero(speed.x) && road->IsHorizontal()) ||
                   (!IsNearlyZero(speed.y) && !road->IsHorizontal());
            }
        );

        if (it != candidate_roads.end()) {
            return *it;
        }
    }
    
    return candidate_roads.front();
}

bool GameSession::TryTransitionToRoad(
    const model::Position& pos,
    model::Speed& speed,
    const model::Road* target_road,
    model::Position& new_pos) const {
    
    if (!target_road) {
        return false;
    }
    
    if (target_road->IsHorizontal()) {
        // Переход на горизонтальную дорогу
        double road_y = target_road->GetStart().y;
        RoadSegment x_segment = RoadSegment::FromPoints(
            target_road->GetStart().x, target_road->GetEnd().x);
        
        new_pos.y = road_y;
        new_pos.x = std::clamp(new_pos.x, 
            x_segment.min - ROAD_HALF_WIDTH, 
            x_segment.max + ROAD_HALF_WIDTH);
        
        // Проверяем выход за границы
        if (IsNearlyEqual(new_pos.x, x_segment.min - ROAD_HALF_WIDTH) ||
            IsNearlyEqual(new_pos.x, x_segment.max + ROAD_HALF_WIDTH)) {
            speed.x = 0.0;
        }
        return true;
    } else {
        // Переход на вертикальную дорогу
        double road_x = target_road->GetStart().x;
        RoadSegment y_segment = RoadSegment::FromPoints(
            target_road->GetStart().y, target_road->GetEnd().y);
        
        new_pos.x = road_x;
        new_pos.y = std::clamp(new_pos.y,
            y_segment.min - ROAD_HALF_WIDTH,
            y_segment.max + ROAD_HALF_WIDTH);
        
        // Проверяем выход за границы
        if (IsNearlyEqual(new_pos.y, y_segment.min - ROAD_HALF_WIDTH) ||
            IsNearlyEqual(new_pos.y, y_segment.max + ROAD_HALF_WIDTH)) {
            speed.y = 0.0;
        }
        return true;
    }
}

void GameSession::MoveDog(model::Dog& dog, double dt) {
    auto pos = dog.GetPosition();
    auto speed = dog.GetSpeed();
    
    if (IsNearlyZero(speed.x) && IsNearlyZero(speed.y)) {
        return;
    }
    
    const model::Road* current_road = FindCurrentRoad(pos, speed);
    if (!current_road) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }
    
    model::Position target_pos = {
        pos.x + speed.x * dt,
        pos.y + speed.y * dt
    };
    
    // Проверяем, остаемся ли на текущей дороге
    if (IsPointOnRoad(target_pos, *current_road)) {
        pos = target_pos;
    } else {
        // Пытаемся перейти на другую дорогу
        const model::Road* transition_road = nullptr;
        
        if (current_road->IsHorizontal() && !IsNearlyZero(speed.y)) {
            // Ищем вертикальную дорогу для перехода
            int x_index = CoordinateToIndex(pos.x);
            if (auto it = vertical_roads_.by_primary_coord.find(x_index);
                it != vertical_roads_.by_primary_coord.end()) {
                for (const auto* road : it->second) {
                    RoadSegment y_segment = RoadSegment::FromPoints(
                        road->GetStart().y, road->GetEnd().y);
                    if (y_segment.Contains(target_pos.y, ROAD_HALF_WIDTH)) {
                        transition_road = road;
                        break;
                    }
                }
            }
        } else if (!current_road->IsHorizontal() && !IsNearlyZero(speed.x)) {
            // Ищем горизонтальную дорогу для перехода
            int y_index = CoordinateToIndex(pos.y);
            if (auto it = horizontal_roads_.by_primary_coord.find(y_index);
                it != horizontal_roads_.by_primary_coord.end()) {
                for (const auto* road : it->second) {
                    RoadSegment x_segment = RoadSegment::FromPoints(
                        road->GetStart().x, road->GetEnd().x);
                    if (x_segment.Contains(target_pos.x, ROAD_HALF_WIDTH)) {
                        transition_road = road;
                        break;
                    }
                }
            }
        }
        
        if (transition_road) {
            // Успешный переход
            TryTransitionToRoad(pos, speed, transition_road, target_pos);
            pos = target_pos;
        } else {
            // Останавливаемся у границы текущей дороги
            if (current_road->IsHorizontal()) {
                RoadSegment x_segment = RoadSegment::FromPoints(
                    current_road->GetStart().x, current_road->GetEnd().x);
                
                if (!IsNearlyZero(speed.x)) {
                    pos.x = (speed.x > 0) ? 
                        x_segment.max + ROAD_HALF_WIDTH : 
                        x_segment.min - ROAD_HALF_WIDTH;
                    speed.x = 0.0;
                }
                if (!IsNearlyZero(speed.y)) {
                    pos.y = (speed.y > 0) ? 
                        current_road->GetStart().y + ROAD_HALF_WIDTH :
                        current_road->GetStart().y - ROAD_HALF_WIDTH;
                    speed.y = 0.0;
                }
            } else {
                RoadSegment y_segment = RoadSegment::FromPoints(
                    current_road->GetStart().y, current_road->GetEnd().y);
                
                if (!IsNearlyZero(speed.y)) {
                    pos.y = (speed.y > 0) ?
                        y_segment.max + ROAD_HALF_WIDTH :
                        y_segment.min - ROAD_HALF_WIDTH;
                    speed.y = 0.0;
                }
                if (!IsNearlyZero(speed.x)) {
                    pos.x = (speed.x > 0) ?
                        current_road->GetStart().x + ROAD_HALF_WIDTH :
                        current_road->GetStart().x - ROAD_HALF_WIDTH;
                    speed.x = 0.0;
                }
            }
        }
    }
    
    dog.SetPosition(pos);
    dog.SetSpeed(speed);
}

void GameSession::Tick(double dt) {
    for (auto& dog : dogs_) {
        MoveDog(*dog, dt);
    }
    // Обновляем лут
    UpdateLoot(dt);
}
