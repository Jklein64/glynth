#include "outliner.h"

#include <freetype/ftoutln.h>
#include <optional>
#include <sstream>
#include <string>

namespace glynth {

// Move
Move::Move(FT_Vector p)
    : p(static_cast<float>(p.x) / 64, static_cast<float>(p.y) / 64) {}

float Move::length(float t) const { return 0.0f; }

glm::vec2 Move::sample(float t) const { return p; }

std::string Move::svg_str() const { return fmt::format("M {},{}", p.x, p.y); }

// Line
Line::Line(FT_Vector p0, FT_Vector p1)
    : p0(static_cast<float>(p0.x) / 64, static_cast<float>(p0.y) / 64),
      p1(static_cast<float>(p1.x) / 64, static_cast<float>(p1.y) / 64) {}

float Line::length(float t) const {
  assert(0 <= t && t <= 1);
  glm::vec2 p = t == 1 ? p1 : sample(t);
  return glm::length(p - p0);
}

glm::vec2 Line::sample(float t) const {
  assert(0 <= t && t <= 1);
  return (1 - t) * p0 + t * p1;
}

std::string Line::svg_str() const { return fmt::format("L {},{}", p1.x, p1.y); }

// Quadratic
Quadratic::Quadratic(FT_Vector p0, FT_Vector c0, FT_Vector p1)
    : p0(static_cast<float>(p0.x) / 64, static_cast<float>(p0.y) / 64),
      c0(static_cast<float>(c0.x) / 64, static_cast<float>(c0.y) / 64),
      p1(static_cast<float>(p1.x) / 64, static_cast<float>(p1.y) / 64) {}

float Quadratic::length(float t) const {
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

glm::vec2 Quadratic::sample(float t) const {
  // TODO normalize by arc length
  assert(0 <= t && t <= 1);
  return (1 - t) * (1 - t) * p0 + 2 * (1 - t) * t * c0 + t * t * p1;
}

std::string Quadratic::svg_str() const {
  return fmt::format("Q {},{} {},{}", c0.x, c0.y, p1.x, p1.y);
}

// Cubic
Cubic::Cubic(FT_Vector p0, FT_Vector c0, FT_Vector c1, FT_Vector p1)
    : p0(static_cast<float>(p0.x) / 64, static_cast<float>(p0.y) / 64),
      c0(static_cast<float>(c0.x) / 64, static_cast<float>(c0.y) / 64),
      c1(static_cast<float>(c1.x) / 64, static_cast<float>(c1.y) / 64),
      p1(static_cast<float>(p1.x) / 64, static_cast<float>(p1.y) / 64) {}

float Cubic::length(float t) const {
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

glm::vec2 Cubic::sample(float t) const {
  // TODO normalize by arc length
  assert(0 <= t && t <= 1);
  return (1 - t) * (1 - t) * (1 - t) * p0 + 3 * (1 - t) * (1 - t) * t * c0 +
         3 * (1 - t) * t * t * c1 + t * t * t * p1;
}

std::string Cubic::svg_str() const {
  return fmt::format("Q {},{} {},{} {},{} ", c0.x, c0.y, c1.x, c1.y, p1.x,
                     p1.y);
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
Outline::Outline(std::vector<std::unique_ptr<Segment>> &&segments,
                 BoundingBox bbox)
    : m_segments(std::move(segments)), m_bbox(bbox) {
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
      m_distances[i] += m_segments[k]->length();
    }
    // Add length of the part of segment j included by parameter
    m_distances[i] += m_segments[j]->length(j_decimal - j);
  }
}

const std::vector<std::unique_ptr<Segment>> &Outline::segments() const {
  return m_segments;
}

const BoundingBox &Outline::bbox() const { return m_bbox; }

glm::vec2 Outline::sample(float t) const {
  assert(0 <= t && t < 1);
  float total_length = 0.0f;
  for (auto &&segment : m_segments) {
    total_length += segment->length();
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
  return m_segments[j]->sample(j_decimal - j);
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
    ss << segment->svg_str() << " ";
  }
  return ss.str();
}

struct UserData {
  FT_Vector &pen;
  std::vector<std::unique_ptr<Segment>> &segments;
  std::optional<FT_Vector> p0;
};

FT_Outline_Funcs funcs = (FT_Outline_Funcs){
    .move_to =
        [](const FT_Vector *to, void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector p1 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            // Replace all moves but the first with lines so text is continuous
            u.segments.emplace_back(new Line(*u.p0, p1));
          } else {
            u.segments.emplace_back(new Move(p1));
          }
          u.p0 = p1;
          return 0;
        },
    .line_to =
        [](const FT_Vector *to, void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector p1 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            u.segments.emplace_back(new Line(*u.p0, p1));
          }
          u.p0 = p1;
          return 0;
        },
    .conic_to =
        [](const FT_Vector *c0, const FT_Vector *to, void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector c0_new = (FT_Vector){c0->x + u.pen.x, c0->y + u.pen.y};
          FT_Vector p1 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            u.segments.emplace_back(new Quadratic(*u.p0, c0_new, p1));
          }
          u.p0 = p1;
          return 0;
        },
    .cubic_to =
        [](const FT_Vector *c0, const FT_Vector *c1, const FT_Vector *to,
           void *user) {
          auto &u = *static_cast<UserData *>(user);
          FT_Vector c0_new = (FT_Vector){c0->x + u.pen.x, c0->y + u.pen.y};
          FT_Vector c1_new = (FT_Vector){c1->x + u.pen.x, c1->y + u.pen.y};
          FT_Vector p1 = (FT_Vector){to->x + u.pen.x, to->y + u.pen.y};
          if (u.p0.has_value()) {
            u.segments.emplace_back(new Cubic(*u.p0, c0_new, c1_new, p1));
          }
          u.p0 = p1;
          return 0;
        },
};

Outline Outliner::outline(std::string_view text) {
  std::vector<std::unique_ptr<Segment>> segments;
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

  // This is actually the cbox, which might be larger than the tight bbox
  return Outline(std::move(segments), bbox);
}

} // namespace glynth
