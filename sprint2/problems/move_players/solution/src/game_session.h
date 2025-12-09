#pragma once

#include <memory>
#include <vector>

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

    model::Dog& AddDog(std::unique_ptr<model::Dog> dog);

private:
    std::vector<std::unique_ptr<model::Dog>> dogs_;
    std::shared_ptr<model::Map> map_;
};