#include <iostream>
#include "test2.hpp"

using namespace geom;

template class geom::Point2D<int>;

int dot(Point2Di a, Point2Di b) {
  return a.x() * b.x() + a.y() * b.y();
}
