#pragma once

#include <fmt/base.h>
#include <fmt/format.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

namespace glynth {

struct Segment {
  virtual ~Segment() = default;
  virtual float length(float t = 1) const = 0;
  virtual glm::vec2 sample(float t) const = 0;
  virtual std::string svg_str() const = 0;
};

struct Move : public Segment {
  Move(FT_Vector p);
  float length(float t = 1) const override;
  glm::vec2 sample(float t) const override;
  std::string svg_str() const override;

  glm::vec2 p;
};

struct Line : public Segment {
  Line(FT_Vector p0, FT_Vector p1);
  float length(float t = 1) const override;
  glm::vec2 sample(float t) const override;
  std::string svg_str() const override;

  glm::vec2 p0, p1;
};

struct Quadratic : public Segment {
  Quadratic(FT_Vector p0, FT_Vector c0, FT_Vector p1);
  float length(float t = 1) const override;
  glm::vec2 sample(float t) const override;
  std::string svg_str() const override;

  glm::vec2 p0, c0, p1;
};

struct Cubic : public Segment {
  Cubic(FT_Vector p0, FT_Vector c0, FT_Vector c1, FT_Vector p1);
  float length(float t = 1) const override;
  glm::vec2 sample(float t) const override;
  std::string svg_str() const override;

  glm::vec2 p0, c0, c1, p1;
};

struct BoundingBox {
  BoundingBox();
  BoundingBox(FT_BBox bbox);
  void expand(const BoundingBox &other);
  // a is at the bottom left, b is at the top right
  glm::vec2 min, max;
};

class Outline {
public:
  Outline(std::vector<std::unique_ptr<Segment>> &&segments, BoundingBox bbox);
  const std::vector<std::unique_ptr<Segment>> &segments() const;
  const BoundingBox &bbox() const;
  // t must be within [0, 1)
  glm::vec2 sample(float t) const;
  std::string svg_str() const;

private:
  std::vector<std::unique_ptr<Segment>> m_segments;
  BoundingBox m_bbox;
  // For arc-length parameterization
  inline static constexpr size_t m_samples = 10000;
  std::vector<float> m_parameters;
  std::vector<float> m_distances;
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
