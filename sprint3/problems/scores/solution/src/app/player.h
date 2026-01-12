#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cctype>

#include "model/dog.h"
#include "utils/tagged.h" 

class GameSession;

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerTokens {
public:
    Token GetToken() const {
        uint64_t num1 = generator1_();
        uint64_t num2 = generator2_();

        std::stringstream ss;
        ss << std::hex << std::setfill('0')
            << std::setw(16) << num1
            << std::setw(16) << num2;

        return Token{ss.str()};
    }

private:
    std::random_device random_device_;

    mutable std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    mutable std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

class Player {
public:
    Player(std::shared_ptr<model::Dog> dog, const std::shared_ptr<GameSession>& session);

    model::Dog& GetDog() const;

    std::shared_ptr<GameSession> GetSession() const;

    Token GetToken() const;

private:
    std::shared_ptr<model::Dog> dog_;
    std::shared_ptr<GameSession> session_;
    Token token_;

    // Метод для генерации токена
    static Token GenerateToken() {
        static PlayerTokens token_generator;  // Статический генератор
        return token_generator.GetToken();
    }
};