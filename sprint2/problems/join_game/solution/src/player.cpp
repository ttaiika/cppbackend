#pragma once

#include "player.h"
#include "game_session.h"

Player::Player(std::unique_ptr<Dog> dog, const std::shared_ptr<GameSession>& session)
        : dog_(std::move(dog)) 
        , session_(session)
        , token_(GenerateToken()) {
    }

    Dog& Player::GetDog() const {
        return *dog_;
    }

    std::shared_ptr<GameSession> Player::GetSession() const {
        return session_;
    }

    Token Player::GetToken() const {
        return token_;
    }