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
  for (auto &segment : outline.segments()) {
    fmt::print("{} ", segment->svg_str());
  }

  fmt::print("\n\n");
  for (size_t i = 0; i < 1000; i++) {
    float t = static_cast<float>(i) / 1000;
    auto sample = outline.sample(t);
    fmt::println("{}, {}", sample.x, sample.y);
  }
}