#pragma once

#include <fmt/base.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>

class FontManager {
public:
  struct Character {
    Character() = default;
    Character(FT_ULong code, FT_Face face);
    glm::vec2 size;
    glm::vec2 bearing;
    float advance;
    GLuint texture;
  };

  FontManager(juce::OpenGLContext& context);
  ~FontManager();
  void addFace(std::string_view face_name);
  void buildBitmaps(std::string_view face_name, FT_UInt pixel_height);
  const Character& getCharacter(std::string_view face_name, char character,
                                FT_UInt pixel_height);

private:
  struct pair_hash final {
  public:
    // See https://stackoverflow.com/a/55083395
    template <typename T, typename U>
    size_t operator()(const std::pair<T, U>& p) const noexcept {
      uintmax_t hash = std::hash<T>{}(p.first);
      hash <<= sizeof(uintmax_t) * 4;
      hash ^= std::hash<U>{}(p.second);
      return std::hash<uintmax_t>{}(hash);
    }
  };

  juce::OpenGLContext& m_context;
  FT_Library m_library;
  std::unordered_map<std::string, FT_Face> m_faces;
  // Maps the pair (face_name, pixel_height) -> charmap
  std::unordered_map<std::pair<std::string, FT_UInt>,
                     std::array<Character, 128>, pair_hash>
      m_character_maps;
  // For fetching display scale
  double m_display_scale;
  juce::MessageManager::Lock m_message_lock;
};
