#include "outliner.h"

#include <freetype/ftoutln.h>
#include <optional>
#include <sstream>
#include <string>

namespace glynth {

// Segment
Segment::Segment(std::initializer_list<FT_Vector> points)
    : m_order(points.size() - 1) {
  assert(0 < points.size() && points.size() <= 4);
  m_points.reserve(points.size());
  for (auto &&point : points) {
    float x = static_cast<float>(point.x) / 64;
    float y = static_cast<float>(point.y) / 64;
    m_points.emplace_back(x, y);
  }
}

float Segment::length(float t) const {
  if (m_order == 0) {
    // Move
    return 0.0f;
  } else {
    // Line, Quadratic, or Cubic
    auto &p0 = m_points[0];
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
  auto &p0 = m_points[0];
  if (m_order == 0) {
    // Move
    return p0;
  } else {
    auto &p1 = m_points[1];
    if (m_order == 1) {
      // Line
      return (1 - t) * p0 + t * p1;
    } else {
      auto &p2 = m_points[2];
      if (m_order == 2) {
        // Quadratic
        return (1 - t) * (1 - t) * p0 + 2 * (1 - t) * t * p1 + t * t * p2;
      } else {
        // Cubic
        auto &p3 = m_points[3];
        return (1 - t) * (1 - t) * (1 - t) * p0 +
               3 * (1 - t) * (1 - t) * t * p1 + 3 * (1 - t) * t * t * p2 +
               t * t * t * p3;
      }
    }
  }
}

std::string Segment::svg_str() const {
  auto &p0 = m_points[0];
  if (m_order == 0) {
    return fmt::format("M {},{}", p0.x, p0.y);
  } else {
    auto &p1 = m_points[1];
    if (m_order == 1) {
      return fmt::format("L {},{}", p1.x, p1.y);
    } else {
      auto &p2 = m_points[2];
      if (m_order == 2) {
        return fmt::format("Q {},{} {},{}", p1.x, p1.y, p2.x, p2.y);
      } else {
        auto &p3 = m_points[3];
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

void BoundingBox::expand(const BoundingBox &other) {
  min = glm::min(min, other.min);
  max = glm::max(max, other.max);
}

// Outline
Outline::Outline(std::vector<Segment> &&segments, BoundingBox bbox)
    : m_segments(segments), m_bbox(bbox) {
  // See https://pomax.github.io/bezierinfo/#tracing
  m_parameters.resize(m_samples);
  m_distances.resize(m_samples, 0.0f);
  for (size_t i = 0; i < m_samples; i++) {
    m_parameters[i] = static_cast<float>(i) / (m_samples - 1);
    // Clamp to within [0, 1) to ensure index is always valid
    m_parameters[i] = std::min(m_parameters[i], std::nextafter(1.0f, 0.0f));
    float j_decimal = m_parameters[i] * m_segments.size();
    size_t j = static_cast<size_t>(j_decimal);
    // Add length of all segments coming before segment j
    for (size_t k = 0; k < j; k++) {
      m_distances[i] += m_segments[k].length();
    }
    // Add length of the part of segment j included by parameter
    m_distances[i] += m_segments[j].length(j_decimal - j);
  }
}

const std::vector<Segment> &Outline::segments() const { return m_segments; }

const BoundingBox &Outline::bbox() const { return m_bbox; }

glm::vec2 Outline::sample(float t) const {
  assert(0 <= t && t < 1);
  float total_length = 0.0f;
  for (auto &&segment : m_segments) {
    total_length += segment.length();
  }
  // Find the i such that distances[i] = t * total_length
  size_t i_best = 0;
  float v_best = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < m_samples; i++) {
    float v = std::abs(m_distances[i] - t * total_length);
    if (v < v_best) {
      i_best = i;
      v_best = v;
    }
  }
  // Do the naive sampling with t = parameters[i_best]
  float j_decimal = m_parameters[i_best] * m_segments.size();
  size_t j = static_cast<size_t>(j_decimal);
  return m_segments[j].sample(j_decimal - j);
}

// Outliner
Outliner::Outliner(const FT_Library &library) : m_library(library) {
  std::string font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
  FT_Error error = FT_New_Face(library, font_path.c_str(), 0, &m_face);
  if (error) {
    fmt::println(stderr, "Failed to read font");
    exit(1);
  }
}

std::string Outline::svg_str() const {
  std::stringstream ss;
  for (auto &&segment : m_segments) {
    ss << segment.svg_str() << " ";
  }
  return ss.str();
}

struct UserData {
  FT_Vector &pen;
  std::vector<Segment> &segments;
  std::optional<FT_Vector> p0;
};

FT_Outline_Funcs funcs = (FT_Outline_Funcs){
    .move_to =
        [](const FT_Vector *to, void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector p1 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            // Replace all moves but the first with lines so text is continuous
            u.segments.push_back(Segment({*u.p0, p1}));
          } else {
            u.segments.push_back(Segment({p1}));
          }
          u.p0 = p1;
          return 0;
        },
    .line_to =
        [](const FT_Vector *to, void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector p1 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            u.segments.push_back(Segment({*u.p0, p1}));
          }
          u.p0 = p1;
          return 0;
        },
    .conic_to =
        [](const FT_Vector *c0, const FT_Vector *to, void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector p2 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            FT_Vector p1 = (FT_Vector){c0->x + u.pen.x, c0->y + u.pen.y};
            u.segments.push_back(Segment({*u.p0, p1, p2}));
          }
          u.p0 = p2;
          return 0;
        },
    .cubic_to =
        [](const FT_Vector *c0, const FT_Vector *c1, const FT_Vector *to,
           void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector p3 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            FT_Vector p1 = (FT_Vector){c0->x + u.pen.x, c0->y + u.pen.y};
            FT_Vector p2 = (FT_Vector){c1->x + u.pen.x, c1->y + u.pen.y};
            u.segments.push_back(Segment({*u.p0, p1, p2, p3}));
          }
          u.p0 = p3;
          return 0;
        },
};

Outline Outliner::outline(std::string_view text) {
  std::vector<Segment> segments;
  FT_Vector pen{.x = 0, .y = 0};
  BoundingBox bbox;
  UserData user = {
      .pen = pen,
      .segments = segments,
      .p0 = std::nullopt,
  };

  for (size_t i = 0; i < text.size(); i++) {
    FT_Error error = FT_Load_Char(m_face, text[i], FT_LOAD_DEFAULT);
    FT_GlyphSlot glyph = m_face->glyph;
    if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
      fmt::println(stderr, "Glyph was not an outline");
      exit(1);
    }

    error = FT_Outline_Decompose(&glyph->outline, &funcs, &user);
    if (error) {
      fmt::println(stderr, "Failed to decompose outline");
      exit(1);
    }

    FT_BBox cbox;
    FT_Outline_Get_CBox(&glyph->outline, &cbox);
    cbox.xMin += pen.x;
    cbox.xMax += pen.x;
    cbox.yMin += pen.y;
    cbox.yMax += pen.y;
    bbox.expand(BoundingBox(cbox));
    pen.x += glyph->advance.x;
    pen.y += glyph->advance.y;
  }

  // Flip vertically so origin is in top right
  for (auto &&segment : segments) {
    for (auto &&point : segment.m_points) {
      point.y = bbox.max.y - (point.y - bbox.min.y);
    }
  }

  // This is actually the cbox, which might be larger than the tight bbox
  return Outline(std::move(segments), bbox);
}

} // namespace glynth
