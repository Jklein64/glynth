#pragma once

#include <fmt/base.h>
#include <fmt/format.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <span>
#include <string_view>
#include <vector>

struct Segment {
  Segment(FT_Vector p0);
  Segment(FT_Vector p0, FT_Vector p1);
  Segment(FT_Vector p0, FT_Vector p1, FT_Vector p2);
  Segment(FT_Vector p0, FT_Vector p1, FT_Vector p2, FT_Vector p3);

  bool operator==(const Segment& other) const;

  float length(float t = 1) const;
  glm::vec2 sample(float t) const;
  void flip(float y_min, float y_max);
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
  float width() const;
  float height() const;
  // a is at the bottom left, b is at the top right
  glm::vec2 min, max;
};

class Outline {
public:
  Outline(std::string_view text, FT_Face face, FT_UInt pixel_height,
          bool invert_y = false, size_t arc_length_samples = 10000);

  bool operator==(const Outline& other) const;

  const std::span<const Segment> segments() const;
  const BoundingBox& bbox() const;
  std::vector<glm::vec2> sample(size_t n) const;
  // Note: expects the parameter values to be increasing
  std::vector<glm::vec2> sample(std::span<float> t) const;
  std::string svg_str() const;

private:
  std::string m_text;
  std::vector<Segment> m_segments;
  BoundingBox m_bbox;
  // For arc-length parameterization
  size_t m_num_param_samples;
  std::vector<float> m_parameters;
  std::vector<float> m_distances;
};
