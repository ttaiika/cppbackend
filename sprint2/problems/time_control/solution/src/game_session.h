#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include "tagged.h"
#include "dog.h"
#include "map_fwd.h" // forward declaration

// forward declaration breaks the cycle
class Player;

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

    std::vector<std::shared_ptr<model::Dog>> dogs_;
    std::shared_ptr<model::Map> map_;

    // Вспомогательные структуры для быстрого поиска дорог
    // ключ — целочисленная координата y для горизонтальных, x для вертикальных
    using RoadsByCoord = std::unordered_map<int, std::vector<const model::Road*>>;

    // индексы дорог
    RoadsByCoord horizontal_by_y_;
    RoadsByCoord vertical_by_x_;
};