#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "utils/tagged.h"
#include "model/dog.h"
#include "loot_generator.h"
#include "model/loot.h" 
#include "collision_detector.h"

namespace model {
    class Map;
    class Road;
    class Office;
}

using Id = util::Tagged<std::string, model::Map>;

class GameSession {
public:
    explicit GameSession(std::shared_ptr<model::Map> map, 
                        const model::LootGeneratorConfig& loot_config);
    
    std::shared_ptr<model::Map> GetMap() const;
    Id GetMapId() const;
    model::Dog& AddDog(std::shared_ptr<model::Dog> dog);
    void Tick(double dt);
    const std::vector<std::shared_ptr<model::Dog>>& GetDogs() const;

    const std::unordered_map<model::LootId, model::LootObject>& GetLootObjects() const;
    void UpdateLoot(double dt);

private:
    // Внутренняя структура для индексации дорог
    struct RoadIndex {
        std::unordered_map<int, std::vector<const model::Road*>> by_primary_coord;
        std::unordered_map<int, std::vector<const model::Road*>> by_secondary_coord;
    };
    
    // Вспомогательная структура для работы с сегментами дорог
    struct RoadSegment {
        double min;
        double max;
        double center;
        
        static RoadSegment FromPoints(double p1, double p2);
        bool Contains(double point, double margin = 0.0) const;
    };
    
    // Структуры для обработки коллизий
    struct DogMovement {
        model::Dog* dog;
        geom::Point2D start_pos;
        geom::Point2D end_pos;
    };
    
    struct CollisionEvent {
        enum class Type { ITEM_PICKUP, OFFICE_RETURN };
        Type type;
        model::Dog* dog;
        size_t object_id;  
        double time;
    };

    // Провайдеры для collision_detector
    class ItemCollisionProvider : public collision_detector::ItemGathererProvider {
    public:
        ItemCollisionProvider(const std::vector<DogMovement>& movements,
                             const std::unordered_map<model::LootId, model::LootObject>& loot_objects);
        
        size_t ItemsCount() const override;
        collision_detector::Item GetItem(size_t idx) const override;
        size_t GatherersCount() const override;
        collision_detector::Gatherer GetGatherer(size_t idx) const override;
        
    private:
        const std::vector<DogMovement>& movements_;
        const std::unordered_map<model::LootId, model::LootObject>& loot_objects_;
    };
    
    class OfficeCollisionProvider : public collision_detector::ItemGathererProvider {
    public:
        OfficeCollisionProvider(const std::vector<DogMovement>& movements,
                               const std::vector<model::Office>& offices);
        
        size_t ItemsCount() const override;
        collision_detector::Item GetItem(size_t idx) const override;
        size_t GatherersCount() const override;
        collision_detector::Gatherer GetGatherer(size_t idx) const override;
        
    private:
        const std::vector<DogMovement>& movements_;
        const std::vector<model::Office>& offices_;
    };
    
    void AddRoadToIndex(const model::Road& road, RoadIndex& index, 
                        int primary_coord, const RoadSegment& segment);
    const model::Road* FindTransitionRoad(int coord_index,
                                         double target_coord,
                                         const RoadIndex& roads_index,
                                         std::function<RoadSegment(const model::Road*)> get_segment) const;
    void StopDogAtRoadBoundary(const model::Road* road, 
                              model::Position& pos, 
                              model::Speed& speed) const;
    bool IsPointOnRoad(const model::Position& point, const model::Road& road) const;
    static int CoordinateToIndex(double coord);
    static bool IsNearlyZero(double value);
    static bool IsNearlyEqual(double a, double b);
   
    void BuildRoadIndices();
    const model::Road* FindCurrentRoad(const model::Position& pos, const model::Speed& speed) const;
    bool TryTransitionToRoad(const model::Position& pos, model::Speed& speed, 
                           const model::Road* target_road, model::Position& new_pos) const;
    void MoveDog(model::Dog& dog, double dt);
    
    void AddRandomLoot();
    unsigned GetLootersCount() const;  // Количество мародёров (собак)

    std::vector<DogMovement> RecordDogMovements(double dt);
    void ProcessCollisions(const std::vector<DogMovement>& movements);
    std::vector<CollisionEvent> FindItemCollisions(const std::vector<DogMovement>& movements);
    std::vector<CollisionEvent> FindOfficeCollisions(const std::vector<DogMovement>& movements);
    
    std::vector<std::shared_ptr<model::Dog>> dogs_;
    std::shared_ptr<model::Map> map_;
    RoadIndex horizontal_roads_;
    RoadIndex vertical_roads_;

    std::unordered_map<model::LootId, model::LootObject> loot_objects_;
    loot_gen::LootGenerator loot_generator_;
    size_t next_loot_id_ = 0;
    
    static constexpr double ROAD_WIDTH = 0.8;
    static constexpr double ROAD_HALF_WIDTH = ROAD_WIDTH / 2.0;
    static constexpr double EPSILON = 1e-9;
    static constexpr double DOG_WIDTH = 0.6;
    static constexpr double ITEM_WIDTH = 0.0;
    static constexpr double OFFICE_WIDTH = 0.5;
};