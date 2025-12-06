#include "outliner.h"
#include "error.h"

#include <freetype/ftoutln.h>
#include <juce_core/juce_core.h>
#include <optional>
#include <sstream>
#include <string>

// Segment
Segment::Segment(FT_Vector p0) : m_order(0) {
  m_points.emplace_back(static_cast<float>(p0.x) / 64,
                        static_cast<float>(p0.y) / 64);
  m_length = 0.0f;
}

Segment::Segment(FT_Vector p0, FT_Vector p1) : m_order(1) {
  m_points.emplace_back(static_cast<float>(p0.x) / 64,
                        static_cast<float>(p0.y) / 64);
  m_points.emplace_back(static_cast<float>(p1.x) / 64,
                        static_cast<float>(p1.y) / 64);
  m_length = glm::length(m_points[1] - m_points[0]);
}

Segment::Segment(FT_Vector p0, FT_Vector p1, FT_Vector p2) : m_order(2) {
  m_points.emplace_back(static_cast<float>(p0.x) / 64,
                        static_cast<float>(p0.y) / 64);
  m_points.emplace_back(static_cast<float>(p1.x) / 64,
                        static_cast<float>(p1.y) / 64);
  m_points.emplace_back(static_cast<float>(p2.x) / 64,
                        static_cast<float>(p2.y) / 64);
  glm::vec2 prev = m_points[0];
  glm::vec2 curr;
  m_length = 0.0f;
  for (size_t k = 1; k < 10; k++) {
    // Travel from s=0 to s=1 in 10 steps
    curr = sample((static_cast<float>(k) / (10 - 1)));
    m_length += glm::length(curr - prev);
    prev = curr;
  }
}

Segment::Segment(FT_Vector p0, FT_Vector p1, FT_Vector p2, FT_Vector p3)
    : m_order(3) {
  m_points.emplace_back(static_cast<float>(p0.x) / 64,
                        static_cast<float>(p0.y) / 64);
  m_points.emplace_back(static_cast<float>(p1.x) / 64,
                        static_cast<float>(p1.y) / 64);
  m_points.emplace_back(static_cast<float>(p2.x) / 64,
                        static_cast<float>(p2.y) / 64);
  m_points.emplace_back(static_cast<float>(p3.x) / 64,
                        static_cast<float>(p3.y) / 64);
  glm::vec2 prev = m_points[0];
  glm::vec2 curr;
  m_length = 0.0f;
  for (size_t k = 1; k < 10; k++) {
    // Travel from s=0 to s=1 in 10 steps
    curr = sample((static_cast<float>(k) / (10 - 1)));
    m_length += glm::length(curr - prev);
    prev = curr;
  }
}

float Segment::length(float t) const {
  if (juce::exactlyEqual(t, 1.0f)) {
    return m_length;
  }

  if (m_order == 0) {
    // Move
    return 0.0f;
  } else {
    // Line, Quadratic, or Cubic
    auto& p0 = m_points[0];
    if (m_order == 1) {
      // Line
      glm::vec2 p = sample(t);
      return glm::length(p - p0);
    } else {
      // Quadratic or Cubic
      float length = 0.0f;
      glm::vec2 prev = p0;
      glm::vec2 curr;
      for (size_t i = 1; i < 10; i++) {
        // Travel from s=0 to s=t in 10 steps
        curr = sample((static_cast<float>(i) / (10 - 1)) * t);
        length += glm::length(curr - prev);
        prev = curr;
      }
      return length;
    }
  }
}

glm::vec2 Segment::sample(float t) const {
  auto& p0 = m_points[0];
  if (m_order == 0) {
    // Move
    return p0;
  } else {
    auto& p1 = m_points[1];
    if (m_order == 1) {
      // Line
      return (1 - t) * p0 + t * p1;
    } else {
      auto& p2 = m_points[2];
      if (m_order == 2) {
        // Quadratic
        return (1 - t) * (1 - t) * p0 + 2 * (1 - t) * t * p1 + t * t * p2;
      } else {
        // Cubic
        auto& p3 = m_points[3];
        return (1 - t) * (1 - t) * (1 - t) * p0 +
               3 * (1 - t) * (1 - t) * t * p1 + 3 * (1 - t) * t * t * p2 +
               t * t * t * p3;
      }
    }
  }
}

void Segment::flip(float y_min, float y_max) {
  for (auto& point : m_points) {
    point.y = y_max - (point.y - y_min);
  }
}

std::string Segment::svg_str() const {
  auto& p0 = m_points[0];
  if (m_order == 0) {
    return fmt::format("M {},{}", p0.x, p0.y);
  } else {
    auto& p1 = m_points[1];
    if (m_order == 1) {
      return fmt::format("L {},{}", p1.x, p1.y);
    } else {
      auto& p2 = m_points[2];
      if (m_order == 2) {
        return fmt::format("Q {},{} {},{}", p1.x, p1.y, p2.x, p2.y);
      } else {
        auto& p3 = m_points[3];
        return fmt::format("Q {},{} {},{} {},{} ", p1.x, p1.y, p2.x, p2.y, p3.x,
                           p3.y);
      }
    }
  }
}

// BoundingBox
BoundingBox::BoundingBox()
    : min(std::numeric_limits<float>::infinity(),
          std::numeric_limits<float>::infinity()),
      max(-std::numeric_limits<float>::infinity(),
          -std::numeric_limits<float>::infinity()) {}

BoundingBox::BoundingBox(FT_BBox bbox)
    : min(static_cast<float>(bbox.xMin) / 64,
          static_cast<float>(bbox.yMin) / 64),
      max(static_cast<float>(bbox.xMax) / 64,
          static_cast<float>(bbox.yMax) / 64) {}

void BoundingBox::expand(const BoundingBox& other) {
  min = glm::min(min, other.min);
  max = glm::max(max, other.max);
}

float BoundingBox::width() const { return max.x - min.x; }
float BoundingBox::height() const { return max.y - min.y; }

// Outline

struct UserData {
  FT_Vector& pen;
  std::vector<Segment>& segments;
  std::optional<FT_Vector> p0;
};

FT_Outline_Funcs funcs = {
    .move_to =
        [](const FT_Vector* to, void* user) {
          auto& u = *static_cast<UserData*>(user);
          FT_Vector p0 = {to->x + u.pen.x, to->y + u.pen.y};
          u.segments.emplace_back(p0);
          u.p0 = p0;
          return 0;
        },
    .line_to =
        [](const FT_Vector* to, void* user) {
          auto& u = *static_cast<UserData*>(user);
          FT_Vector p1 = {to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            u.segments.emplace_back(*u.p0, p1);
          }
          u.p0 = p1;
          return 0;
        },
    .conic_to =
        [](const FT_Vector* c0, const FT_Vector* to, void* user) {
          auto& u = *static_cast<UserData*>(user);
          FT_Vector p2 = {to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            FT_Vector p1 = {c0->x + u.pen.x, c0->y + u.pen.y};
            u.segments.emplace_back(*u.p0, p1, p2);
          }
          u.p0 = p2;
          return 0;
        },
    .cubic_to =
        [](const FT_Vector* c0, const FT_Vector* c1, const FT_Vector* to,
           void* user) {
          auto& u = *static_cast<UserData*>(user);
          FT_Vector p3 = {to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            FT_Vector p1 = {c0->x + u.pen.x, c0->y + u.pen.y};
            FT_Vector p2 = {c1->x + u.pen.x, c1->y + u.pen.y};
            u.segments.emplace_back(*u.p0, p1, p2, p3);
          }
          u.p0 = p3;
          return 0;
        },
    .shift = 0,
    .delta = 0,
};

Outline::Outline(std::string_view text, FT_Face face, FT_UInt pixel_height,
                 bool invert_y, size_t num_param_samples)
    : m_num_param_samples(num_param_samples) {
  FT_Error err;
  FT_Vector pen{.x = 0, .y = 0};
  UserData user = {
      .pen = pen,
      .segments = m_segments,
      .p0 = std::nullopt,
  };

  if ((err = FT_Set_Pixel_Sizes(face, 0, pixel_height))) {
    throw FreetypeError(FT_Error_String(err));
  }

  for (size_t i = 0; i < text.size(); i++) {
    FT_ULong char_code = static_cast<FT_ULong>(text[i]);
    if ((err = FT_Load_Char(face, char_code, FT_LOAD_DEFAULT))) {
      throw FreetypeError(FT_Error_String(err));
    }

    FT_GlyphSlot glyph = face->glyph;
    if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
      throw GlynthError(
          fmt::format("(Glyph for '{}' was not an outline)", text[i]));
    }

    if ((err = FT_Outline_Decompose(&glyph->outline, &funcs, &user))) {
      throw FreetypeError(FT_Error_String(err));
    }

    FT_BBox cbox;
    FT_Outline_Get_CBox(&glyph->outline, &cbox);
    cbox.xMin += pen.x;
    cbox.xMax += pen.x;
    cbox.yMin += pen.y;
    cbox.yMax += pen.y;
    m_bbox.expand(BoundingBox(cbox));
    pen.x += glyph->advance.x;
    pen.y += glyph->advance.y;
  }

  if (invert_y) {
    // Flip vertically so origin is in top right
    for (auto&& segment : m_segments) {
      segment.flip(m_bbox.min.y, m_bbox.max.y);
    }
  }

  // See https://pomax.github.io/bezierinfo/#tracing
  m_parameters.resize(m_num_param_samples);
  m_distances.resize(m_num_param_samples, 0.0f);
  for (size_t i = 0; i < m_num_param_samples; i++) {
    float i_float = static_cast<float>(i);
    m_parameters[i] = i_float / static_cast<float>(m_num_param_samples - 1);
    // Clamp to within [0, 1) to ensure index is always valid
    m_parameters[i] = std::min(m_parameters[i], std::nextafter(1.0f, 0.0f));
    float j_decimal = m_parameters[i] * static_cast<float>(m_segments.size());
    size_t j = static_cast<size_t>(j_decimal);
    // Add length of all segments coming before segment j
    for (size_t k = 0; k < j; k++) {
      m_distances[i] += m_segments[k].length();
    }
    // Add length of the part of segment j included by parameter
    float j_whole = static_cast<float>(j);
    m_distances[i] += m_segments[j].length(j_decimal - j_whole);
  }
}

const std::span<const Segment> Outline::segments() const { return m_segments; }

const BoundingBox& Outline::bbox() const { return m_bbox; }

std::vector<glm::vec2> Outline::sample(size_t n) const {
  std::vector<glm::vec2> samples(n);
  std::vector<float> ts(n);
  for (size_t i = 0; i < n; i++) {
    float t = static_cast<float>(i) / static_cast<float>(n - 1);
    // Clamp to within [0, 1) to ensure index is always valid
    ts[i] = std::clamp(t, 0.0f, std::nextafterf(1.0f, 0.0f));
  }
  return sample(ts);
}

std::vector<glm::vec2> Outline::sample(std::span<float> ts) const {
  std::vector<glm::vec2> samples(ts.size());
  size_t j_best = 0;
  for (size_t i = 0; i < samples.size(); i++) {
    float t = ts[i];
    assert(0 <= t && t < 1);
    float total_length = 0.0f;
    for (auto&& segment : m_segments) {
      total_length += segment.length();
    }
    // Find the j such that distances[j] = t * total_length. Assuming ts is
    // increasing, j_best should always be at least the previous j_best
    float v_best = std::abs(m_distances[j_best] - t * total_length);
    for (size_t j = 1; j < m_num_param_samples; j++) {
      float v = std::abs(m_distances[j] - t * total_length);
      if (v < v_best) {
        j_best = j;
        v_best = v;
      }
    }
    // Do the naive sampling with t = parameters[j_best]
    float j_decimal =
        m_parameters[j_best] * static_cast<float>(m_segments.size());
    size_t j = static_cast<size_t>(j_decimal);
    float j_whole = static_cast<float>(j);
    samples[i] = m_segments[j].sample(j_decimal - j_whole);
  }
  return samples;
}

std::string Outline::svg_str() const {
  std::stringstream ss;
  for (auto&& segment : m_segments) {
    ss << segment.svg_str() << " ";
  }
  return ss.str();
}
