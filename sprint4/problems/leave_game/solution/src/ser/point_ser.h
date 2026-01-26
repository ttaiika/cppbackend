#pragma once

#include "../model.h"

namespace model{

template <typename Archive, typename T>
void serialize(Archive& ar, Point2<T>& point, [[maybe_unused]] const unsigned version) {
  ar& point.x;
  ar& point.y;
}

} // namespace model