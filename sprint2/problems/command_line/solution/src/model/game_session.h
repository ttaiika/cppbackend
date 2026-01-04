#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "utils/tagged.h"
#include "model/dog.h"

namespace model {
    class Map;
    class Road;
}

using Id = util::Tagged<std::string, model::Map>;

class GameSession {
public:
    explicit GameSession(std::shared_ptr<model::Map> map);
    
    std::shared_ptr<model::Map> GetMap() const;
    Id GetMapId() const;
    model::Dog& AddDog(std::shared_ptr<model::Dog> dog);
    void Tick(double dt);
    const std::vector<std::shared_ptr<model::Dog>>& GetDogs() const;

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
    
    std::vector<std::shared_ptr<model::Dog>> dogs_;
    std::shared_ptr<model::Map> map_;
    RoadIndex horizontal_roads_;
    RoadIndex vertical_roads_;
};