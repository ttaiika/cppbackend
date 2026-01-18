#include "serialization/model_serialization.h"
#include "model/dog.h"
#include "model/loot.h"
#include "model/game_session.h"
#include "app/player.h"
#include "app/players.h"
#include <memory>

namespace serialization {

using namespace model;

DogRepr::DogRepr(const model::Dog& dog)
    : id_(dog.GetId())
    , name_(dog.GetName())
    , pos_(dog.GetPosition())
    , bag_capacity_(dog.GetBagCapacity())
    , speed_(dog.GetSpeed())
    , direction_(dog.GetDirection())
    , score_(dog.GetScore()) {
    
    auto bag_items = dog.GetBagItems();
    for (const auto& item : bag_items) {
        bag_content_.push_back(item);
    }
}

std::shared_ptr<Dog> DogRepr::Restore() const {
    auto dog = std::make_shared<Dog>(id_, name_);
    dog->SetPosition(pos_);
    dog->SetBagCapacity(bag_capacity_);
    dog->SetSpeed(speed_);
    dog->SetDirection(direction_);
    dog->AddScore(score_);
    
    for (const auto& item : bag_content_) {
        dog->TryAddToBag(item.item_id, item.type, item.value);
    }
    
    return dog;
}

LootRepr::LootRepr(const model::LootObject& loot)
    : id_(*loot.id)
    , type_(loot.type)
    , x_(loot.x)
    , y_(loot.y)
    , value_(loot.value) {
}

LootObject LootRepr::Restore() const {
    return LootObject{LootId{id_}, type_, x_, y_, value_};
}

PlayerRepr::PlayerRepr(const std::shared_ptr<Player>& player)
    : dog_repr_(player->GetDog())
    , token_str_(*player->GetToken()) 
    , session_map_id_str_(*player->GetSession()->GetMapId()) {
}

std::tuple<std::shared_ptr<Dog>, Token, Map::Id> PlayerRepr::Restore() const {
    auto dog = dog_repr_.Restore();
    
    Token token(token_str_);
    
    Map::Id map_id{session_map_id_str_};
    return std::make_tuple(dog, token, map_id);
}

SessionRepr::SessionRepr(const std::shared_ptr<GameSession>& session)
    : map_id_str_(*session->GetMapId()) {
    
    const auto& loot_objects = session->GetLootObjects();
    for (const auto& [id, loot] : loot_objects) {
        loot_.emplace_back(loot);
    }
}

std::pair<Map::Id, std::unordered_map<LootId, LootObject>> 
SessionRepr::Restore() const {
    std::unordered_map<LootId, LootObject> loot_objects;
    
    for (const auto& loot_repr : loot_) {
        auto loot = loot_repr.Restore();
        loot_objects[loot.id] = loot;
    }
    
    return std::make_pair(Map::Id(map_id_str_), std::move(loot_objects));
}

GameStateRepr::GameStateRepr(const Game& game, const Players& players) 
    : next_dog_id_(game.GetNextDogId()) {
    
    const auto& sessions = game.GetSessions();
    for (const auto& session : sessions) {
        sessions_.emplace_back(session);
    }
    
    auto all_players = players.GetAllPlayers();
    for (const auto& player : all_players) {
        players_.emplace_back(player);
    }
}

void GameStateRepr::Restore(Game& game, Players& players) const {
    game.ClearSessions();
    players.Clear();
    
    // Сначала создаем все сессии
    for (const auto& session_repr : sessions_) {
        auto [map_id, loot_objects] = session_repr.Restore();
        
        const Map* map = game.FindMap(map_id);
        if (!map) {
            throw std::runtime_error("Map not found: " + *map_id);
        }
        
        auto map_ptr = std::shared_ptr<Map>(std::shared_ptr<void>(), const_cast<Map*>(map));
        auto session = std::make_shared<GameSession>(map_ptr, game.GetLootConfig());
        
        for (const auto& [loot_id, loot] : loot_objects) {
            session->AddLootObject(loot);
        }
        
        game.RestoreSession(session);
    }
    
    // Затем восстанавливаем игроков
    for (const auto& player_repr : players_) {
        auto [dog, token, map_id] = player_repr.Restore();
        
        // Находим сессию по map_id
        std::shared_ptr<GameSession> session;
        for (const auto& s : game.GetSessions()) {
            if (s->GetMapId() == map_id) {
                session = s;
                break;
            }
        }
        
        if (!session) {
            throw std::runtime_error("Session not found for map: " + *map_id);
        }
        
        // Добавляем собаку в сессию
        session->AddDog(dog);
        
        // Создаем игрока с собакой и сессией
        auto player = std::make_shared<Player>(dog, session);
        
        // Устанавливаем восстановленный токен
        player->SetToken(token);
        
        // Добавляем игрока в хранилище
        players.AddExistingPlayer(player);
    }
    
    game.SetNextDogId(next_dog_id_);
}
}