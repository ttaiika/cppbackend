#pragma once

#include "geometry.h"

namespace model {

class GameObject {
public:
  void SetPosition(Point2d const& position);
  const Point2d& GetPosition() const noexcept;

  void SetWidth(double width);
  double GetWidth() const noexcept;

  virtual ~GameObject() = default;
protected:
  GameObject(Point2d const& pos = Point2d(), double width = 0.0);
  Point2d m_position;
  double m_width;
};

}  // namespace model