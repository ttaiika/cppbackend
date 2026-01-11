#pragma once

namespace geom {

using Dimension = int;
using Coord = Dimension;

struct Point2D {
    double x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point2D position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

}  // namespace geom