
#include "font_manager.h"
#include "fonts.h"

#include <fmt/format.h>
#include <regex>

class FreetypeError : public std::runtime_error {
public:
  explicit FreetypeError(const std::string& msg) : std::runtime_error(msg) {}
  explicit FreetypeError(char* msg) : std::runtime_error(msg) {}
};

FontManager::FontManager(juce::OpenGLContext& context) : m_context(context) {
  FT_Error err;
  if (err = FT_Init_FreeType(&m_library); err != 0) {
    throw FreetypeError(FT_Error_String(err));
  }
}

class FontManagerError : public std::runtime_error {
public:
  explicit FontManagerError(const std::string& msg) : std::runtime_error(msg) {}
  explicit FontManagerError(char* msg) : std::runtime_error(msg) {}
};

FontManager::~FontManager() { FT_Done_FreeType(m_library); }

void FontManager::setDisplayScale(double display_scale) {
  m_display_scale = display_scale;
}

void FontManager::addFace(std::string_view face_name, FT_UInt pixel_height) {
  assert(m_context.isAttached() && m_context.isActive() && m_display_scale > 0);
  // Load from binary data; assumes face_name is the non-extension filename
  std::string resource_name = std::string(face_name) + "_ttf";
  resource_name = std::regex_replace(resource_name, std::regex("-"), "");
  FT_Face face;
  int file_size;
  auto file_base = fonts::getNamedResource(resource_name.c_str(), file_size);
  if (file_base == nullptr) {
    throw FontManagerError(
        fmt::format(R"(No resource with name "{}")", resource_name));
  }
  if (auto err = FT_New_Memory_Face(m_library,
                                    reinterpret_cast<const FT_Byte*>(file_base),
                                    file_size, 0, &face)) {
    throw FreetypeError(FT_Error_String(err));
  }
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
  // Cleanup
  FT_Done_Face(face);
}

const FontManager::Character&
FontManager::getCharacter(std::string_view face_name, char character,
                          FT_UInt pixel_height) {
  assert(m_context.isAttached() && m_context.isActive());
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               static_cast<GLsizei>(glyph->bitmap.width),
               static_cast<GLsizei>(glyph->bitmap.rows), 0, GL_RED,
               GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
