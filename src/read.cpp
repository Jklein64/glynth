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
  // See https://codereview.stackexchange.com/a/22907
  std::ifstream font_stream("/System/Library/Fonts/Supplemental/Arial.ttf",
                            std::ios::binary | std::ios::ate);
  // Get the current position of the stream pointer, which is at the end
  std::ifstream::pos_type pos = font_stream.tellg();
  std::vector<std::byte> font_data(pos);
  font_stream.seekg(std::ifstream::beg);
  font_stream.read(reinterpret_cast<char*>(font_data.data()), pos);
  glynth::Outliner outliner(font_data);
  auto outline = outliner.outline("Glynth", 16);
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
  npy::tensor<float> points(std::vector<size_t>{550, 2});
  for (int i = 0; i < points.shape(0); i++) {
    float t = static_cast<float>(i) / (points.shape(0) - 1);
    // Clamp to within [0, 1) to ensure index is always valid
    t = std::clamp(t, 0.0f, std::nextafterf(1.0f, 0.0f));
    glm::vec2 point = outline.sample(t);
    points(i, 0) = point.x;
    points(i, 1) = point.y;
  }
  npy::save("./out/outline.npy", points);
}