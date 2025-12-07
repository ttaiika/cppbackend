#include "game_session.h"
#include "model.h"   // здесь уже можно подключить полный Map

GameSession::GameSession(std::shared_ptr<model::Map> map)
    : map_(std::move(map)) {}

std::shared_ptr<model::Map> GameSession::GetMap() const {
    return map_;
}

Id GameSession::GetMapId() const {
    return map_->GetId();
}

Dog& GameSession::AddDog(std::unique_ptr<Dog> dog) {
    Dog* raw_ptr = dog.get();
    dogs_.push_back(std::move(dog));
    return *raw_ptr;
}