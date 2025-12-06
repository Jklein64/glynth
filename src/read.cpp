#include "error.h"
#include "fonts.h"
#include "outliner.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/std.h>
#include <freetype/freetype.h>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include <fstream>
#include <npy/npy.h>
#include <npy/tensor.h>
#include <string>

int main() {
  FT_Error err;
  FT_Library library;
  if ((err = FT_Init_FreeType(&library))) {
    throw FreetypeError(FT_Error_String(err));
  }

  std::vector<FT_Byte> data(fonts::SplineSansMonoMedium_ttfSize);
  std::memcpy(data.data(), fonts::SplineSansMonoMedium_ttf, data.size());

  FT_Face face;
  if ((err = FT_New_Memory_Face(library, data.data(), data.size(), 0, &face))) {
    throw FreetypeError(FT_Error_String(err));
  }

  Outline outline("Glynth", face, 16);
  // Save to svg file for preview
  std::ofstream svg_file("./out/outline.svg");
  auto bbox = outline.bbox();
  fmt::print(
      svg_file,
      R"(<svg viewBox="{} {} {} {}" xmlns="http://www.w3.org/2000/svg">)"
      R"(<path stroke="black" stroke-width="0.1" fill="none" d="{}" /></svg>)",
      bbox.min.x, bbox.min.y, bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y,
      outline.svg_str());

  // Save trace to numpy file for reconstruction
  // One cycle of an 80 Hz wave at 44.1 kHz is around 550 samples
  size_t num_samples = 550;
  auto points = outline.sample(num_samples);
  npy::tensor<float> points_npy(std::vector<size_t>{num_samples, 2});
  // Valid because glm vectors are densely packed and npy data is row-major
  points_npy.copy_from(reinterpret_cast<float*>(points.data()),
                       points.size() * 2);
  npy::save("./out/outline.npy", points_npy);
}
