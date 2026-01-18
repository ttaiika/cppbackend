#include "model/game_session.h"
#include "model/model.h"

#include <algorithm>
#include <cmath>
#include <iostream>

GameSession::RoadSegment GameSession::RoadSegment::FromPoints(double p1, double p2) {
    return {std::min(p1, p2), std::max(p1, p2), (p1 + p2) / 2.0};
}

bool GameSession::RoadSegment::Contains(double point, double margin) const {
    return point >= min - margin - EPSILON && 
           point <= max + margin + EPSILON;
}

int GameSession::CoordinateToIndex(double coord) {
    return static_cast<int>(std::floor(coord));
}

bool GameSession::IsNearlyZero(double value) {
    return std::abs(value) < EPSILON;
}

bool GameSession::IsNearlyEqual(double a, double b) {
    return std::abs(a - b) < EPSILON;
}

bool GameSession::IsPointOnRoad(const model::Position& point, const model::Road& road) const {
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

void GameSession::AddRoadToIndex(const model::Road& road, RoadIndex& index, 
                                 int primary_coord, const RoadSegment& segment) {
    // Добавляем дорогу в индекс по основной координате
    index.by_primary_coord[primary_coord].push_back(&road);
    
    // Добавляем дорогу в индекс по вторичным координатам
    int min_secondary_index = CoordinateToIndex(segment.min - ROAD_HALF_WIDTH);
    int max_secondary_index = CoordinateToIndex(segment.max + ROAD_HALF_WIDTH);
    
    for (int secondary = min_secondary_index; secondary <= max_secondary_index; ++secondary) {
        index.by_secondary_coord[secondary].push_back(&road);
    }
}

const model::Road* GameSession::FindTransitionRoad(int coord_index,
                                                   double target_coord,
                                                   const RoadIndex& roads_index,
                                                   std::function<RoadSegment(const model::Road*)> get_segment) const {
    if (auto it = roads_index.by_primary_coord.find(coord_index);
        it != roads_index.by_primary_coord.end()) {
        for (const auto* road : it->second) {
            RoadSegment segment = get_segment(road);
            if (segment.Contains(target_coord, ROAD_HALF_WIDTH)) {
                return road;
            }
        }
    }
    return nullptr;
}

void GameSession::StopDogAtRoadBoundary(const model::Road* road, 
                                       model::Position& pos, 
                                       model::Speed& speed) const {
    if (road->IsHorizontal()) {
        RoadSegment x_segment = RoadSegment::FromPoints(
            road->GetStart().x, road->GetEnd().x);
        
        if (!IsNearlyZero(speed.x)) {
            pos.x = (speed.x > 0) ? 
                x_segment.max + ROAD_HALF_WIDTH : 
                x_segment.min - ROAD_HALF_WIDTH;
            speed.x = 0.0;
        }
        if (!IsNearlyZero(speed.y)) {
            pos.y = (speed.y > 0) ? 
                road->GetStart().y + ROAD_HALF_WIDTH :
                road->GetStart().y - ROAD_HALF_WIDTH;
            speed.y = 0.0;
        }
    } else {
        RoadSegment y_segment = RoadSegment::FromPoints(
            road->GetStart().y, road->GetEnd().y);
        
        if (!IsNearlyZero(speed.y)) {
            pos.y = (speed.y > 0) ?
                y_segment.max + ROAD_HALF_WIDTH :
                y_segment.min - ROAD_HALF_WIDTH;
            speed.y = 0.0;
        }
        if (!IsNearlyZero(speed.x)) {
            pos.x = (speed.x > 0) ?
                road->GetStart().x + ROAD_HALF_WIDTH :
                road->GetStart().x - ROAD_HALF_WIDTH;
            speed.x = 0.0;
        }
    }
}

GameSession::ItemCollisionProvider::ItemCollisionProvider(
    const std::vector<DogMovement>& movements,
    const std::unordered_map<model::LootId, model::LootObject>& loot_objects)
    : movements_(movements), loot_objects_(loot_objects) {}

size_t GameSession::ItemCollisionProvider::ItemsCount() const {
    return loot_objects_.size();
}

collision_detector::Item GameSession::ItemCollisionProvider::GetItem(size_t idx) const {
    auto it = loot_objects_.begin();
    std::advance(it, idx);
    return collision_detector::Item{
        .position = geom::Point2D{it->second.x, it->second.y},
        .width = ITEM_WIDTH
    };
}

size_t GameSession::ItemCollisionProvider::GatherersCount() const {
    return movements_.size();
}

collision_detector::Gatherer GameSession::ItemCollisionProvider::GetGatherer(size_t idx) const {
    const auto& movement = movements_[idx];
    return collision_detector::Gatherer{
        .start_pos = movement.start_pos,
        .end_pos = movement.end_pos,
        .width = DOG_WIDTH
    };
}

GameSession::OfficeCollisionProvider::OfficeCollisionProvider(
    const std::vector<DogMovement>& movements,
    const std::vector<model::Office>& offices)
    : movements_(movements), offices_(offices) {}

size_t GameSession::OfficeCollisionProvider::ItemsCount() const {
    return offices_.size();
}

collision_detector::Item GameSession::OfficeCollisionProvider::GetItem(size_t idx) const {
    const auto& office = offices_[idx];
    auto pos = office.GetPosition();
    return collision_detector::Item{
        .position = geom::Point2D{static_cast<double>(pos.x), static_cast<double>(pos.y)},
        .width = OFFICE_WIDTH
    };
}

size_t GameSession::OfficeCollisionProvider::GatherersCount() const {
    return movements_.size();
}

collision_detector::Gatherer GameSession::OfficeCollisionProvider::GetGatherer(size_t idx) const {
    const auto& movement = movements_[idx];
    return collision_detector::Gatherer{
        .start_pos = movement.start_pos,
        .end_pos = movement.end_pos,
        .width = DOG_WIDTH
    };
}

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
            RoadSegment x_segment = RoadSegment::FromPoints(
                road.GetStart().x, road.GetEnd().x);
            AddRoadToIndex(road, horizontal_roads_, y_index, x_segment);
        } else {
            int x_index = CoordinateToIndex(road.GetStart().x);
            RoadSegment y_segment = RoadSegment::FromPoints(
                road.GetStart().y, road.GetEnd().y);
            AddRoadToIndex(road, vertical_roads_, x_index, y_segment);
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
    if (!dog) {
        throw std::invalid_argument("Dog cannot be nullptr");
    }
    
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
    int loot_value = 0; 

    if (loot_types_count > 0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(0, loot_types_count - 1);
        loot_type = dis(gen);

        loot_value = map_->GetLootValue(loot_type);
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
        .y = pos_y,
        .value = loot_value
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
            transition_road = FindTransitionRoad(
                x_index, target_pos.y, vertical_roads_,
                [](const model::Road* road) {
                    return RoadSegment::FromPoints(road->GetStart().y, road->GetEnd().y);
                }
            );
        } else if (!current_road->IsHorizontal() && !IsNearlyZero(speed.x)) {
            // Ищем горизонтальную дорогу для перехода
            int y_index = CoordinateToIndex(pos.y);
            transition_road = FindTransitionRoad(
                y_index, target_pos.x, horizontal_roads_,
                [](const model::Road* road) {
                    return RoadSegment::FromPoints(road->GetStart().x, road->GetEnd().x);
                }
            );
        }
        
        if (transition_road) {
            // Успешный переход
            TryTransitionToRoad(pos, speed, transition_road, target_pos);
            pos = target_pos;
        } else {
            // Останавливаемся у границы текущей дороги
            StopDogAtRoadBoundary(current_road, pos, speed);
        }
    }
    
    dog.SetPosition(pos);
    dog.SetSpeed(speed);
}

std::vector<GameSession::DogMovement> GameSession::RecordDogMovements(double dt) {
    std::vector<DogMovement> movements;
    movements.reserve(dogs_.size());
    
    for (auto& dog : dogs_) {
        if (!dog) continue;
        
        auto start_pos = dog->GetPosition();
        auto speed = dog->GetSpeed();
        
        // Вычисляем конечную позицию, которую должна занять собака после движения
        geom::Point2D end_pos = {
            start_pos.x + speed.x * dt,
            start_pos.y + speed.y * dt
        };
        
        movements.push_back(DogMovement{
            .dog = dog.get(),
            .start_pos = geom::Point2D{start_pos.x, start_pos.y},
            .end_pos = end_pos
        });
    }
    
    return movements;
}

std::vector<GameSession::CollisionEvent> GameSession::FindItemCollisions(
    const std::vector<DogMovement>& movements) {
    
    if (movements.empty() || loot_objects_.empty()) {
        return {};
    }
    
    std::vector<CollisionEvent> events;
    
    // Используем collision_detector для поиска столкновений с предметами
    ItemCollisionProvider provider(movements, loot_objects_);
    auto gather_events = collision_detector::FindGatherEvents(provider);
    
    // Преобразуем события collision_detector в наши события
    for (const auto& gather_event : gather_events) {
        if (gather_event.gatherer_id < movements.size() && 
            gather_event.item_id < loot_objects_.size()) {
            
            // Находим соответствующий предмет
            auto loot_it = loot_objects_.begin();
            std::advance(loot_it, gather_event.item_id);
            
            events.push_back(CollisionEvent{
                .type = CollisionEvent::Type::ITEM_PICKUP,
                .dog = movements[gather_event.gatherer_id].dog,
                .object_id = *loot_it->first,  // LootId -> size_t
                .time = gather_event.time
            });
        }
    }
    
    return events;
}

std::vector<GameSession::CollisionEvent> GameSession::FindOfficeCollisions(
    const std::vector<DogMovement>& movements) {
    
    if (movements.empty()) {
        return {};
    }
    
    std::vector<CollisionEvent> events;
    const auto& offices = map_->GetOffices();
    
    if (offices.empty()) {
        return {};
    }
    
    // Используем collision_detector для поиска столкновений с офисами
    OfficeCollisionProvider provider(movements, offices);
    auto gather_events = collision_detector::FindGatherEvents(provider);
    
    // Преобразуем события collision_detector в наши события
    for (const auto& gather_event : gather_events) {
        if (gather_event.gatherer_id < movements.size() && 
            gather_event.item_id < offices.size()) {
            
            events.push_back(CollisionEvent{
                .type = CollisionEvent::Type::OFFICE_RETURN,
                .dog = movements[gather_event.gatherer_id].dog,
                .object_id = gather_event.item_id,  // Индекс офиса
                .time = gather_event.time
            });
        }
    }
    
    return events;
}

void GameSession::ProcessCollisions(const std::vector<DogMovement>& movements) {
    if (movements.empty()) {
        return;
    }
    
    // Находим все события коллизий
    auto item_events = FindItemCollisions(movements);
    auto office_events = FindOfficeCollisions(movements);
    
    // Объединяем все события
    std::vector<CollisionEvent> all_events;
    all_events.reserve(item_events.size() + office_events.size());
    all_events.insert(all_events.end(), item_events.begin(), item_events.end());
    all_events.insert(all_events.end(), office_events.begin(), office_events.end());
    
    // Сортируем по времени 
    std::sort(all_events.begin(), all_events.end(),
        [](const CollisionEvent& a, const CollisionEvent& b) {
            return a.time < b.time;
        });
    
    for (const auto& event : all_events) {
        if (!event.dog) continue;
        
        switch (event.type) {
            case CollisionEvent::Type::ITEM_PICKUP: {
                // Находим предмет по ID
                model::LootId loot_id{event.object_id};
                auto loot_it = loot_objects_.find(loot_id);
                
                if (loot_it != loot_objects_.end() && !event.dog->IsBagFull()) {
                    // Собака берет предмет
                    if (event.dog->TryAddToBag(*loot_it->first, loot_it->second.type, loot_it->second.value)) {
                        // Успешно взяли - удаляем предмет с карты
                        loot_objects_.erase(loot_it);
                    }
                }
                break;
            }
                
            case CollisionEvent::Type::OFFICE_RETURN: {
                // Собака возвращает предметы на базу
                auto returned_items = event.dog->ClearBag();

                // Начисляем очки за каждый возвращенный предмет
                for (const auto& item : returned_items) {
                    event.dog->AddScore(item.value);
                }

                break;
            }
        }
    }
}

void GameSession::Tick(double dt) {
    // Запоминаем движения собак (начальные и конечные позиции)
    auto movements = RecordDogMovements(dt);

    // Двигаем собак (обновляем их позиции)
    for (auto& dog : dogs_) {
        if (!dog) continue;  
        MoveDog(*dog, dt);
    }

    // Обрабатываем коллизии на основе запомненных движений
    ProcessCollisions(movements);

    // Обновляем лут
    UpdateLoot(dt);
}