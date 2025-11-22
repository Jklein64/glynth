#include <fmt/base.h>
#include <freetype/freetype.h>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include <ft2build.h>
#include <iostream>

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

  error = FT_Set_Char_Size(face, 0, 16 * 64, 300, 300);
  if (error) {
    fmt::println(stderr, "Failed to set char size");
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

  // Get glyph for letter J
  error = FT_Load_Char(face, 0x004A, FT_LOAD_DEFAULT);
  //   FT_UInt glyph_index = FT_Get_Char_Index(face, 0x004A);
  //   error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
  if (error) {
    fmt::println(stderr, "Failed to load glyph");
    exit(1);
  }

  fmt::println("\nGlyph Info:");
  fmt::println("face.glyph.format = {}",
               static_cast<size_t>(face->glyph->format));
  fmt::println(" - none: {}", static_cast<size_t>(FT_GLYPH_FORMAT_NONE));
  fmt::println(" - composite: {}",
               static_cast<size_t>(FT_GLYPH_FORMAT_COMPOSITE));
  fmt::println(" - bitmap: {}", static_cast<size_t>(FT_GLYPH_FORMAT_BITMAP));
  fmt::println(" - outline: {}", static_cast<size_t>(FT_GLYPH_FORMAT_OUTLINE));
  fmt::println(" - plotter: {}", static_cast<size_t>(FT_GLYPH_FORMAT_PLOTTER));
  fmt::println(" - svg: {}", static_cast<size_t>(FT_GLYPH_FORMAT_SVG));

  // Walk the outline
  auto outline = face->glyph->outline;
  auto svg_funcs = (FT_Outline_Funcs){
      .move_to =
          [](const FT_Vector *to, void *user) {
            fmt::print("M {},{} ", to->x, to->y);
            return 0;
          },
      .line_to =
          [](const FT_Vector *to, void *user) {
            fmt::print("L {},{} ", to->x, to->y);
            return 0;
          },
      // Note: "conic" refers to a second-order Bezier
      .conic_to =
          [](const FT_Vector *control, const FT_Vector *to, void *user) {
            fmt::print("Q {},{} {},{} ", control->x, control->y, to->x, to->y);
            return 0;
          },
      .cubic_to =
          [](const FT_Vector *control1, const FT_Vector *control2,
             const FT_Vector *to, void *user) {
            fmt::print("C {},{} {},{} {},{} ", control1->x, control1->y,
                       control2->x, control2->y, to->x, to->y);
            return 0;
          },
  };
  fmt::println("\nOutline Info:");
  fmt::println("n_points = {}", outline.n_points);
  fmt::println("n_contours = {}", outline.n_contours);
  fmt::println("decomposing outline...");
  error = FT_Outline_Decompose(&outline, &svg_funcs, nullptr);
  fmt::println("");
  if (error) {
    fmt::println(stderr, "Failed to decompose outline");
    exit(1);
  }

  FT_BBox bbox;
  FT_Outline_Get_CBox(&outline, &bbox);

  fmt::println("viewBox = {} {} {} {}", bbox.xMin, bbox.yMin,
               bbox.xMax - bbox.xMin, bbox.yMax - bbox.yMin);
}