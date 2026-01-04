#include "app/players.h"
#include "model/model.h" // полный доступ к model::Map и Map::Id

std::shared_ptr<Player> Players::AddPlayer(std::shared_ptr<model::Dog> dog, std::shared_ptr<GameSession> session) {
        // Создаем игрока
        auto player = std::make_shared<Player>(dog, session);

        // Получаем токен игрока
        Token token = player->GetToken();

        uint32_t dog_id = player->GetDog().GetId(); 
        model::Map::Id map_id = player->GetSession()->GetMapId(); 

        players_by_key_[{dog_id, map_id}] = player;
        players_by_token_[token] = player;
        
        return player;
    }

    std::shared_ptr<Player> Players::FindByDogIdAndMapId(uint32_t dog_id, const model::Map::Id& map_id) const {
        auto key = std::make_pair(dog_id, map_id);
        auto it = players_by_key_.find(key);
        return it != players_by_key_.end() ? it->second : nullptr;
    }

    std::shared_ptr<Player> Players::FindByToken(const Token& token) const {
        auto it = players_by_token_.find(token);
        return it != players_by_token_.end() ? it->second : nullptr;
    }

    std::vector<std::shared_ptr<Player>> Players::GetPlayersOnMap(const model::Map::Id& map_id) const {
        std::vector<std::shared_ptr<Player>> result;
        for (auto& [key, player] : players_by_key_) {
            if (key.second == map_id) {
                result.push_back(player);
            }
        }
        return result;
    }

    size_t Players::GetPlayersCount() const {
        return players_by_token_.size();
    }