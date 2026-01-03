#pragma once

#include "player.h"
#include "game_session.h"

Player::Player(std::shared_ptr<model::Dog> dog, const std::shared_ptr<GameSession>& session)
        : dog_(dog) 
        , session_(session)
        , token_(GenerateToken()) {
    }

    model::Dog& Player::GetDog() const {
        return *dog_;
    }

    std::shared_ptr<GameSession> Player::GetSession() const {
        return session_;
    }

    Token Player::GetToken() const {
        return token_;
    }