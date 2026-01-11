#pragma once

#include "utils/tagged.h"
#include "utils/tags.h"
#include "model/geom.h"

#include <boost/json.hpp>

namespace model {

using LootId = util::Tagged<size_t, tags::Loot>;

struct LootObject {
    LootId id;  // Добавьте ID объекта
    size_t type;  // 0..N-1, где N - количество типов лута на карте
    double x;    // Координата X как double
    double y;    // Координата Y как double
};

struct LootGeneratorConfig {
    double period;       // в секундах
    double probability;
};

struct LootType {
    std::string name;
    std::string file;
    std::string type;
    double rotation;
    std::string color;
    double scale;
};

}  // namespace model