#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>

#include "dog.h"
#include "tagged.h"
#include "player.h"
#include "model.h"

class GameSession;

namespace std {
    template<>
    struct hash<std::pair<unsigned int, util::Tagged<std::string, model::Map>>> {
        size_t operator()(const std::pair<unsigned int, util::Tagged<std::string, model::Map>>& p) const noexcept {
            return std::hash<unsigned int>{}(p.first) ^ (std::hash<std::string>{}(*p.second) << 1);
        }
    };
}

class Players {
public:
    Players() = default;

    std::shared_ptr<Player> AddPlayer(std::shared_ptr<model::Dog> dog, std::shared_ptr<GameSession> session);

    std::shared_ptr<Player> FindByDogIdAndMapId(uint32_t dog_id, const model::Map::Id& map_id) const;

    std::shared_ptr<Player> FindByToken(const Token& token) const;

    std::vector<std::shared_ptr<Player>> GetPlayersOnMap(const model::Map::Id& map_id) const;

    size_t GetPlayersCount() const;

private:
    using Key = std::pair<uint32_t, model::Map::Id>;

    std::unordered_map<Key, std::shared_ptr<Player>> players_by_key_;
    std::unordered_map<Token, std::shared_ptr<Player>, util::TaggedHasher<Token>> players_by_token_;

    //std::shared_ptr<GameSession> session_;
};