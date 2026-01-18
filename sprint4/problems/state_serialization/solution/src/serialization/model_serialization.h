#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "model/model.h"
#include "app/player.h"
#include "model/game_session.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

// Сериализация для Position
template <typename Archive>
void serialize(Archive& ar, Position& pos, [[maybe_unused]] const unsigned version) {
    ar& pos.x;
    ar& pos.y;
}

// Сериализация для Speed
template <typename Archive>
void serialize(Archive& ar, Speed& speed, [[maybe_unused]] const unsigned version) {
    ar& speed.x;
    ar& speed.y;
}

// Сериализация для Direction
template <typename Archive>
void serialize(Archive& ar, Direction& dir, [[maybe_unused]] const unsigned version) {
    int dir_int = static_cast<int>(dir);
    ar& dir_int;
    dir = static_cast<Direction>(dir_int);
}

// Сериализация для BagItem
template <typename Archive>
void serialize(Archive& ar, BagItem& item, [[maybe_unused]] const unsigned version) {
    ar& item.item_id;
    ar& item.type;
    ar& item.value;
}

}  // namespace model

namespace serialization {

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;
    explicit DogRepr(const model::Dog& dog);
    
    [[nodiscard]] std::shared_ptr<model::Dog> Restore() const;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& pos_;
        ar& bag_capacity_;
        ar& speed_;
        ar& direction_;
        ar& score_;
        ar& bag_content_;
    }

private:
    uint32_t id_ = 0;
    std::string name_;
    model::Position pos_;
    int bag_capacity_ = 0;
    model::Speed speed_;
    model::Direction direction_ = model::Direction::NORTH;
    int score_ = 0;
    std::vector<model::BagItem> bag_content_;
};

// LootRepr - представление для лута
class LootRepr {
public:
    LootRepr() = default;
    explicit LootRepr(const model::LootObject& loot);
    
    [[nodiscard]] model::LootObject Restore() const;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& type_;
        ar& x_;
        ar& y_;
        ar& value_;
    }

private:
    size_t id_ = 0;
    size_t type_ = 0;
    double x_ = 0.0;
    double y_ = 0.0;
    int value_ = 0;
};

// PlayerRepr - представление для игрока
class PlayerRepr {
public:
    PlayerRepr() = default;
    explicit PlayerRepr(const std::shared_ptr<Player>& player);
    
    [[nodiscard]] std::tuple<std::shared_ptr<model::Dog>, Token, model::Map::Id> Restore() const;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& dog_repr_;
        ar& token_str_;
        ar& session_map_id_str_;
    }

private:
    DogRepr dog_repr_;
    std::string token_str_;            // Храним как string
    std::string session_map_id_str_;   // Храним как string
};

// SessionRepr - представление для игровой сессии
class SessionRepr {
public:
    SessionRepr() = default;
    explicit SessionRepr(const std::shared_ptr<GameSession>& session);
    
    [[nodiscard]] std::pair<model::Map::Id, std::unordered_map<model::LootId, model::LootObject>> Restore() const;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_str_;
        ar& loot_;
    }

private:
    std::string map_id_str_;           // Храним как string
    std::vector<LootRepr> loot_;
};

// GameStateRepr - представление для всего состояния игры
class GameStateRepr {
public:
    GameStateRepr() = default;
    explicit GameStateRepr(const model::Game& game, const Players& players);
    
    void Restore(model::Game& game, Players& players) const;
    
    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& sessions_;
        ar& players_;
        ar& next_dog_id_;
    }

private:
    std::vector<SessionRepr> sessions_;
    std::vector<PlayerRepr> players_;
    uint32_t next_dog_id_ = 0;
};

}  // namespace serialization