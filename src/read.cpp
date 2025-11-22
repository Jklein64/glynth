#include <fmt/base.h>
#include <fmt/format.h>
#include <freetype/freetype.h>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include <ft2build.h>
#include <sstream>
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

  std::string text = "Glynth";
  std::stringstream path_str_stream;
  FT_Vector pen{0, 0};
  FT_BBox text_cbox{
      .xMin = std::numeric_limits<FT_Pos>::max(),
      .yMin = std::numeric_limits<FT_Pos>::max(),
      .xMax = std::numeric_limits<FT_Pos>::min(),
      .yMax = std::numeric_limits<FT_Pos>::min(),
  };
  for (size_t i = 0; i < text.size(); i++) {
    error = FT_Load_Char(face, text[i], FT_LOAD_DEFAULT);
    if (error) {
      fmt::println(stderr, "Failed to load glyph");
      exit(1);
    }
    if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
      fmt::println(stderr, "Glyph was not an outline");
      exit(1);
    }

    FT_Outline outline = face->glyph->outline;
    struct user_t {
      std::stringstream &ss;
      FT_Vector &pen;
    };
    user_t user{path_str_stream, pen};

    FT_Outline_Funcs funcs = (FT_Outline_Funcs){
        .move_to =
            [](const FT_Vector *to, void *user) {
              auto &u = *static_cast<user_t *>(user);
              auto data =
                  fmt::format("M {},{} ", to->x + u.pen.x, to->y + u.pen.y);
              u.ss.write(data.c_str(), data.size());
              return 0;
            },
        .line_to =
            [](const FT_Vector *to, void *user) {
              auto &u = *static_cast<user_t *>(user);
              auto data =
                  fmt::format("L {},{} ", to->x + u.pen.x, to->y + u.pen.y);
              u.ss.write(data.c_str(), data.size());
              return 0;
            },
        // Note: "conic" refers to a second-order Bezier
        .conic_to =
            [](const FT_Vector *control, const FT_Vector *to, void *user) {
              auto &u = *static_cast<user_t *>(user);
              auto data = fmt::format("Q {},{} {},{} ", control->x + u.pen.x,
                                      control->y + u.pen.y, to->x + u.pen.x,
                                      to->y + u.pen.y);
              u.ss.write(data.c_str(), data.size());
              return 0;
            },
        .cubic_to =
            [](const FT_Vector *control1, const FT_Vector *control2,
               const FT_Vector *to, void *user) {
              auto &u = *static_cast<user_t *>(user);
              auto data = fmt::format(
                  "C {},{} {},{} {},{} ", control1->x + u.pen.x,
                  control1->y + u.pen.y, control2->x + u.pen.x,
                  control2->y + u.pen.y, to->x + u.pen.x, to->y + u.pen.y);
              u.ss.write(data.c_str(), data.size());
              return 0;
            },
    };
    error = FT_Outline_Decompose(&outline, &funcs, &user);
    if (error) {
      fmt::println(stderr, "Failed to decompose outline");
      exit(1);
    }

    FT_BBox cbox;
    FT_Outline_Get_CBox(&outline, &cbox);
    text_cbox.xMin = std::min(text_cbox.xMin, cbox.xMin + pen.x);
    text_cbox.yMin = std::min(text_cbox.yMin, cbox.yMin + pen.y);
    text_cbox.xMax = std::max(text_cbox.xMax, cbox.xMax + pen.x);
    text_cbox.yMax = std::max(text_cbox.yMax, cbox.yMax + pen.y);
    pen.x += face->glyph->advance.x;
    pen.y += face->glyph->advance.y;
  }
  fmt::println("{}", path_str_stream.str());
  fmt::println("viewBox = {} {} {} {}", text_cbox.xMin, text_cbox.yMin,
               text_cbox.xMax - text_cbox.xMin,
               text_cbox.yMax - text_cbox.yMin);
}