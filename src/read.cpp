#include "outliner.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/std.h>
#include <freetype/freetype.h>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include <string>

int main() {
  FT_Error error;
  FT_Library library;
  error = FT_Init_FreeType(&library);
  if (error) {
    fmt::println(stderr, "Error configuring freetype");
    exit(1);
  }

  FT_Face face;
  std::string font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
  error = FT_New_Face(library, font_path.c_str(), 0, &face);
  if (error) {
    fmt::println(stderr, "Failed to read font");
    exit(1);
  }

  // Print some metadata about the face
  fmt::println("num_faces = {}", face->num_faces);
  fmt::println("num_glyphs = {}", face->num_glyphs);
  fmt::println("face_flags = {}", face->face_flags);
  fmt::println("family_name = {}", face->family_name);
  fmt::println("style_name = {}", face->style_name);
  fmt::println("size.x_ppem = {}", face->size->metrics.x_ppem);
  fmt::println("size.y_ppem = {}", face->size->metrics.y_ppem);

  glynth::Outliner outliner(library);
  auto outline = outliner.outline("Glynth");
  bool started = false;
  for (auto &segment : outline.segments()) {
    std::visit(
        [&started](auto &arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, glynth::Move>) {
            auto move = static_cast<glynth::Move>(arg);
            if (started) {
              // Connect subsetquent glyphs
              fmt::print("L {},{} ", move.p1.x, move.p1.y);
            } else {
              fmt::print("M {},{} ", move.p1.x, move.p1.y);
            }
          }

          else if constexpr (std::is_same_v<T, glynth::Line>) {
            auto line = static_cast<glynth::Line>(arg);
            fmt::print("L {},{} ", line.p1.x, line.p1.y);
          }

          else if constexpr (std::is_same_v<T, glynth::Quadratic>) {
            auto spline = static_cast<glynth::Quadratic>(arg);
            fmt::print("Q {},{} {},{} ", spline.c0.x, spline.c0.y, spline.p1.x,
                       spline.p1.y);
          }

          else if constexpr (std::is_same_v<T, glynth::Cubic>) {
            auto spline = static_cast<glynth::Cubic>(arg);
            fmt::print("Q {},{} {},{} {},{} ", spline.c0.x, spline.c0.y,
                       spline.c1.x, spline.c1.y, spline.p1.x, spline.p1.y);
          }
        },
        segment);
    started = true;
  }
  fmt::print("\n\n");
  for (size_t i = 0; i < 1000; i++) {
    float t = static_cast<float>(i) / 1000;
    auto sample = outline.sample(t);
    fmt::println("{}, {}", sample.x, sample.y);
  }
}