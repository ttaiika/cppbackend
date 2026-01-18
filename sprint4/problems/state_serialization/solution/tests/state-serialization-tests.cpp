#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model/model.h"
#include "../src/model/dog.h"
#include "../src/model/loot.h"
#include "../src/serialization/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        // Используем конструктор Dog(uint32_t id, std::string name)
        Dog dog{42u, "Pluto"s};
        
        // Устанавливаем позицию
        dog.SetPosition(Position{42.2, 12.5});
        
        // Устанавливаем вместимость сумки
        dog.SetBagCapacity(3);
        
        // Добавляем очки
        dog.AddScore(42);
        
        // Пытаемся добавить предмет в сумку - используем TryAddToBag(size_t item_id, int type, int value)
        bool added = dog.TryAddToBag(10, 2, 1); // item_id=10, type=2, value=1
        CHECK(added);
        
        // Устанавливаем направление
        dog.SetDirection(Direction::EAST);
        
        // Устанавливаем скорость
        dog.SetSpeed(Speed{2.3, -1.2});

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetId() == restored->GetId());
                CHECK(dog.GetName() == restored->GetName());
                CHECK(dog.GetPosition() == restored->GetPosition());
                CHECK(dog.GetSpeed() == restored->GetSpeed());
                CHECK(dog.GetBagCapacity() == restored->GetBagCapacity());
                CHECK(dog.GetDirection() == restored->GetDirection());
                CHECK(dog.GetScore() == restored->GetScore());
                
                // Сравнение содержимого сумки
                auto dog_bag = dog.GetBagItems();
                auto restored_bag = restored->GetBagItems();
                CHECK(dog_bag.size() == restored_bag.size());
                if (!dog_bag.empty()) {
                    CHECK(dog_bag[0].item_id == restored_bag[0].item_id);
                    CHECK(dog_bag[0].type == restored_bag[0].type);
                    CHECK(dog_bag[0].value == restored_bag[0].value);
                }
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "BagItem serialization") {
    GIVEN("A BagItem") {
        BagItem item{10, 2, 1}; // item_id=10, type=2, value=1
        
        WHEN("BagItem is serialized") {
            output_archive << item;

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                BagItem restored_item;
                input_archive >> restored_item;
                
                CHECK(item.item_id == restored_item.item_id);
                CHECK(item.type == restored_item.type);
                CHECK(item.value == restored_item.value);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Position serialization") {
    GIVEN("A Position") {
        Position pos{42.5, 12.3};
        
        WHEN("position is serialized") {
            output_archive << pos;

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                Position restored_pos;
                input_archive >> restored_pos;
                
                CHECK(pos.x == restored_pos.x);
                CHECK(pos.y == restored_pos.y);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Speed serialization") {
    GIVEN("A Speed") {
        Speed speed{2.3, -1.2};
        
        WHEN("speed is serialized") {
            output_archive << speed;

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                Speed restored_speed;
                input_archive >> restored_speed;
                
                CHECK(speed.x == restored_speed.x);
                CHECK(speed.y == restored_speed.y);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Direction serialization") {
    GIVEN("A Direction") {
        Direction dir = Direction::EAST;
        
        WHEN("direction is serialized") {
            output_archive << dir;

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                Direction restored_dir;
                input_archive >> restored_dir;
                
                CHECK(dir == restored_dir);
            }
        }
    }
}