#pragma once

#include <fmt/base.h>
#include <fmt/format.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

namespace glynth {

struct Move {
  inline static int degree = 0;
  Move(FT_Vector p0, FT_Vector p1);
  glm::vec2 sample(float t) const;
  glm::vec2 p0, p1;
};

struct Line {
  inline static int degree = 1;
  Line(FT_Vector p0, FT_Vector p1);
  glm::vec2 sample(float t) const;
  glm::vec2 p0, p1;
};

struct Quadratic {
  inline static int degree = 2;
  Quadratic(FT_Vector p0, FT_Vector c0, FT_Vector p1);
  glm::vec2 sample(float t) const;
  glm::vec2 p0, c0, p1;
};

struct Cubic {
  inline static int degree = 3;
  Cubic(FT_Vector p0, FT_Vector c0, FT_Vector c1, FT_Vector p1);
  glm::vec2 sample(float t) const;
  glm::vec2 p0, c0, c1, p1;
};

using Segment = std::variant<Move, Line, Quadratic, Cubic>;

struct BoundingBox {
  BoundingBox();
  BoundingBox(FT_BBox bbox);
  void expand(const BoundingBox &other);
  // a is at the bottom left, b is at the top right
  glm::vec2 min, max;
};

class Outline {
public:
  Outline(std::vector<Segment> segments, BoundingBox bbox);
  const std::vector<Segment> &segments() const;
  const BoundingBox &bbox() const;
  glm::vec2 sample(float t) const;

private:
  std::vector<Segment> m_segments;
  BoundingBox m_bbox;
};

class Outliner {
public:
  Outliner(const FT_Library &library);
  Outline outline(std::string_view text);

private:
  const FT_Library &m_library;
  FT_Face m_face;
};

} // namespace glynth
