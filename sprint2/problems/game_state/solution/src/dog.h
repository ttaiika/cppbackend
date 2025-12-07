#pragma once
#include <string>
#include <cstdint>

namespace model {

struct Position {
    double x = 0.0; 
    double y = 0.0; 
};

struct Speed {
    double x = 0.0;  // скорость по оси X
    double y = 0.0;  // скорость по оси Y
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

class Dog {
public:
    Dog(uint32_t id, std::string name)
        : id_(id)
        , name_(std::move(name)) {
    }

    uint32_t GetId() const { return id_; }

    std::string GetName() const { return name_;}

    void SetPosition(Position pos) { pos_ = pos; }
    const Position& GetPosition() const { return pos_; }

    void SetSpeed(Speed speed) { speed_ = speed; }
    const Speed& GetSpeed() const { return speed_; }

    void SetDirection(Direction dir) { direction_ = dir; }
    Direction GetDirection() const { return direction_; }

private:
    uint32_t id_;
    std::string name_;
    Position pos_;
    Speed speed_;
    Direction direction_ = Direction::NORTH; 
};

} // end namespace model