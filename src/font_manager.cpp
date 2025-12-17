
#include "font_manager.h"
#include "error.h"
#include "fonts.h"

#include <fmt/format.h>
#include <regex>

FontManager::FontManager() {
  FT_Error err;
  if (err = FT_Init_FreeType(&m_library); err != 0) {
    throw FreetypeError(FT_Error_String(err));
  }

  // Get max display scale, which is needed to render Freetype fonts correctly.
  // Freetype doesn't distinguish between logical pixels and physical pixels,
  // so creates bitmaps at half the desired resolution on high-dpi devices.
  // Iterating over all display rects gets the max scale for any display, since
  // scale could differ significantly between computer monitors, for example
  m_display_scale = std::numeric_limits<decltype(m_display_scale)>::min();
  auto& displays = juce::Desktop::getInstance().getDisplays();
  auto rectangle_list = displays.getRectangleList(true);
  for (int i = 0; i < rectangle_list.getNumRectangles(); i++) {
    auto rect = rectangle_list.getRectangle(i);
    auto* display = displays.getDisplayForRect(rect);
    if (display && display->scale > m_display_scale) {
      m_display_scale = display->scale;
    }
  }
}

class FontManagerError : public std::runtime_error {
public:
  explicit FontManagerError(const std::string& msg) : std::runtime_error(msg) {}
  explicit FontManagerError(char* msg) : std::runtime_error(msg) {}
};

FontManager::~FontManager() { FT_Done_FreeType(m_library); }

void FontManager::setContext(juce::OpenGLContext& context) {
  m_context = context;
}

void FontManager::addFace(std::string_view face_name) {
  // Load from binary data; assumes face_name is the non-extension filename
  std::string resource_name = std::string(face_name) + "_ttf";
  resource_name = std::regex_replace(resource_name, std::regex("-"), "");
  int file_size;
  auto file_base = fonts::getNamedResource(resource_name.c_str(), file_size);
  if (file_base == nullptr) {
    throw FontManagerError(
        fmt::format(R"(No resource with name "{}")", resource_name));
  }
  if (auto err = FT_New_Memory_Face(
          m_library, reinterpret_cast<const FT_Byte*>(file_base), file_size, 0,
          &m_faces[std::string(face_name)])) {
    throw FreetypeError(FT_Error_String(err));
  }
}

FT_Face FontManager::getFace(std::string_view face_name) {
  if (auto it = m_faces.find(std::string(face_name)); it != m_faces.end()) {
    return it->second;
  } else {
    throw GlynthError(
        fmt::format(R"(No face found with name "{}")", face_name));
  }
}

void FontManager::buildBitmaps(std::string_view face_name,
                               FT_UInt pixel_height) {
  assert(m_context.has_value());
  auto& face = m_faces.at(std::string(face_name));
  // Render face to bitmaps. Interpret height in logical pixels
  auto& charmap =
      m_character_maps[std::make_pair(std::string(face_name), pixel_height)];
  pixel_height = static_cast<FT_UInt>(pixel_height * m_display_scale);
  FT_Set_Pixel_Sizes(face, 0, pixel_height);
  for (size_t i = 0; i < charmap.size(); i++) {
    Character c(static_cast<FT_ULong>(i), face);
    // Apply display scaling so they're rendered with pixel_height pixels
    c.size /= m_display_scale;
    c.bearing /= m_display_scale;
    c.advance /= static_cast<float>(m_display_scale);
    charmap[i] = std::move(c);
  }
}

const FontManager::Character&
FontManager::getCharacter(std::string_view face_name, char character,
                          FT_UInt pixel_height) {
  assert(m_context.has_value());
  auto key = std::make_pair(std::string(face_name), pixel_height);
  if (auto it = m_character_maps.find(key); it != m_character_maps.end()) {
    return it->second[static_cast<size_t>(character)];
  } else {
    throw FontManagerError(fmt::format(
        R"(Unable to find Character for '{}' in face "{}" with height {})",
        character, face_name, pixel_height));
  }
}

FontManager::Character::Character(FT_ULong code, FT_Face face) {
  if (auto err = FT_Load_Char(face, code, FT_LOAD_RENDER)) {
    throw FreetypeError(FT_Error_String(err));
  }
  // Read from glyph slot
  FT_GlyphSlot glyph = face->glyph;
  size = glm::vec2(glyph->bitmap.width, glyph->bitmap.rows);
  bearing = glm::vec2(glyph->bitmap_left, glyph->bitmap_top);
  advance = static_cast<float>(glyph->advance.x) / 64;
  // Create texture from bitmap
  using namespace juce::gl;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  // Disable byte-alignment restriction
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  // Some characters, like ' ', are actually zero-width, which
  // confuses OpenGL, since all textures must be at least 1x1. This
  // snapping is going to fill a single pixel with a garbage value, but
  // the computed width will be zero, so nothing will be displayed
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               static_cast<GLsizei>(std::max(glyph->bitmap.width, 1u)),
               static_cast<GLsizei>(std::max(glyph->bitmap.rows, 1u)), 0,
               GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
