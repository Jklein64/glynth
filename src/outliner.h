#pragma once

#include <fmt/base.h>
#include <fmt/format.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace glynth {

struct Segment {
  friend class Outliner;

  Segment(FT_Vector p0);
  Segment(FT_Vector p0, FT_Vector p1);
  Segment(FT_Vector p0, FT_Vector p1, FT_Vector p2);
  Segment(FT_Vector p0, FT_Vector p1, FT_Vector p2, FT_Vector p3);
  float length(float t = 1) const;
  glm::vec2 sample(float t) const;
  std::string svg_str() const;

private:
  float m_length;
  size_t m_order;
  std::vector<glm::vec2> m_points;
};

struct BoundingBox {
  BoundingBox();
  BoundingBox(FT_BBox bbox);
  void expand(const BoundingBox& other);
  // a is at the bottom left, b is at the top right
  glm::vec2 min, max;
};

class Outline {
public:
  Outline(std::vector<Segment>&& segments, BoundingBox bbox);
  const std::vector<Segment>& segments() const;
  const BoundingBox& bbox() const;
  // t must be within [0, 1)
  glm::vec2 sample(float t) const;
  std::string svg_str() const;

private:
  std::vector<Segment> m_segments;
  BoundingBox m_bbox;
  // For arc-length parameterization
  inline static constexpr size_t s_samples = 10000;
  std::vector<float> m_parameters;
  std::vector<float> m_distances;
};

class Outliner {
public:
  Outliner(std::string_view font_path);
  Outliner(std::span<const std::byte> font_data);
  Outline outline(std::string_view text, uint32_t pixel_height);

private:
  // Must explicitly initialize or linking fails
  inline static FT_Library s_library{nullptr};
  FT_Face m_face;
};

} // namespace glynth
