#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "utils/tagged.h"
#include "model/dog.h"
#include "loot_generator.h"
#include "model/loot.h" 

namespace model {
    class Map;
    class Road;
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
    struct RoadIndex {
        std::unordered_map<int, std::vector<const model::Road*>> by_primary_coord;
        std::unordered_map<int, std::vector<const model::Road*>> by_secondary_coord;
    };
    
    void BuildRoadIndices();
    const model::Road* FindCurrentRoad(const model::Position& pos, const model::Speed& speed) const;
    bool TryTransitionToRoad(const model::Position& pos, model::Speed& speed, 
                           const model::Road* target_road, model::Position& new_pos) const;
    void MoveDog(model::Dog& dog, double dt);
    
    void AddRandomLoot();
    unsigned GetLootersCount() const;  // Количество мародёров (собак)
    
    std::vector<std::shared_ptr<model::Dog>> dogs_;
    std::shared_ptr<model::Map> map_;
    RoadIndex horizontal_roads_;
    RoadIndex vertical_roads_;

    std::unordered_map<model::LootId, model::LootObject> loot_objects_;
    loot_gen::LootGenerator loot_generator_;
    size_t next_loot_id_ = 0;
};