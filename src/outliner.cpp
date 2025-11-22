#include "outliner.h"

#include <freetype/ftoutln.h>
#include <optional>
#include <string>

namespace glynth {

// Move
Move::Move(FT_Vector p) : p(p.x, p.y) {}

glm::vec2 Move::sample(float t) const { return p; }

std::string Move::svg_str() const { return fmt::format("M {},{}", p.x, p.y); }

// Line
Line::Line(FT_Vector p0, FT_Vector p1) : p0(p0.x, p0.y), p1(p1.x, p1.y) {}

glm::vec2 Line::sample(float t) const { return (1 - t) * p0 + t * p1; }

std::string Line::svg_str() const { return fmt::format("L {},{}", p1.x, p1.y); }

// Quadratic
Quadratic::Quadratic(FT_Vector p0, FT_Vector c0, FT_Vector p1)
    : p0(p0.x, p0.y), c0(c0.x, c0.y), p1(p1.x, p1.y) {}

glm::vec2 Quadratic::sample(float t) const {
  // TODO normalize by arc length
  return (1 - t) * (1 - t) * p0 + 2 * (1 - t) * t * c0 + t * t * p1;
}

std::string Quadratic::svg_str() const {
  return fmt::format("Q {},{} {},{}", c0.x, c0.y, p1.x, p1.y);
}

// Cubic
Cubic::Cubic(FT_Vector p0, FT_Vector c0, FT_Vector c1, FT_Vector p1)
    : p0(p0.x, p0.y), c0(c0.x, c0.y), c1(c1.x, c1.y), p1(p1.x, p1.y) {}

glm::vec2 Cubic::sample(float t) const {
  // TODO normalize by arc length
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
    : min(bbox.xMin, bbox.yMin), max(bbox.xMax, bbox.yMax) {}

void BoundingBox::expand(const BoundingBox &other) {
  min = glm::min(min, other.min);
  max = glm::max(max, other.max);
}

// Outline
Outline::Outline(std::vector<std::unique_ptr<Segment>> &&segments,
                 BoundingBox bbox)
    : m_segments(std::move(segments)), m_bbox(bbox) {}

const std::vector<std::unique_ptr<Segment>> &Outline::segments() const {
  return m_segments;
}

const BoundingBox &Outline::bbox() const { return m_bbox; }

glm::vec2 Outline::sample(float t) const {
  t *= m_segments.size();
  size_t i = static_cast<size_t>(t);
  return m_segments[i]->sample(t - i);
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
