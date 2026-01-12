#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace model {

struct Position {
    double x = 0.0; 
    double y = 0.0; 
};

struct Speed {
    double x = 0.0;  // скорость по оси X
    double y = 0.0;  // скорость по оси Y

    bool operator==(const Speed& other) const {
        return std::abs(x - other.x) < 1e-6 && std::abs(y - other.y) < 1e-6;
    }
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct BagItem {
    size_t item_id;
    int type;
    int value;
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
    Position GetPosition() const { return pos_; }

    void SetSpeed(Speed speed) { speed_ = speed; }
    Speed GetSpeed() const { return speed_; }

    void SetBagCapacity(int bag_capacity) { bag_capacity_ = bag_capacity; }
    int GetBagCapacity() const { return bag_capacity_; }

    void SetDirection(Direction dir) { direction_ = dir; }
    Direction GetDirection() const { return direction_; }

    int GetScore() const { return score_; }
    void AddScore(int points) { score_ += points; }

    bool TryAddToBag(size_t item_id, int type, int value);
    std::vector<BagItem> ClearBag();
    bool IsBagFull() const;
    const std::vector<BagItem>& GetBagItems() const;

private:
    uint32_t id_;
    std::string name_;
    Position pos_;
    Speed speed_;
    int bag_capacity_;
    std::vector<BagItem> bag_;
    Direction direction_ = Direction::NORTH; 
    int score_ = 0;
};

} // end namespace model