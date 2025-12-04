#pragma once

#include "processor.h"
#include "shader_manager.h"

#include <efsw/efsw.hpp>
#include <fmt/base.h>
#include <freetype/freetype.h>
#include <glm/glm.hpp>
#include <juce_events/juce_events.h>
#include <juce_opengl/juce_opengl.h>
#include <vector>

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
  void addFace(std::string_view face_name, FT_UInt pixel_height);
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
  // Maps the pair (face_name, pixel_height) -> charmap
  std::unordered_map<std::pair<std::string, FT_UInt>,
                     std::array<Character, 128>, pair_hash>
      m_character_maps;
  // For fetching display scale
  double m_display_scale;
  juce::MessageManager::Lock m_message_lock;
};

class ShaderComponent;
class GlynthEditor final : public juce::AudioProcessorEditor,
                           public juce::OpenGLRenderer {
public:
  explicit GlynthEditor(GlynthProcessor&);
  ~GlynthEditor() override;

  void paint(juce::Graphics&) override;
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;

private:
  GlynthProcessor& processor_ref;
  juce::MessageManager::Lock m_message_lock;
  juce::OpenGLContext m_context;
  ShaderManager m_shader_manager;
  FontManager m_font_manager;
  std::vector<std::unique_ptr<ShaderComponent>> m_shader_components;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};

class ShaderComponent : public juce::Component {
public:
  ShaderComponent(ShaderManager& shader_manager, const std::string& program_id);
  virtual void renderOpenGL() = 0;

protected:
  ShaderManager& m_shader_manager;
  // ID of the shader program associated with this component
  std::string m_program_id;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaderComponent)
};

class BackgroundComponent : public ShaderComponent {
public:
  BackgroundComponent(ShaderManager& shader_manager,
                      const std::string& program_id);
  ~BackgroundComponent() override;
  void renderOpenGL() override;

private:
  GLuint m_vbo = 0, m_vao = 0;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackgroundComponent)
};

class RectComponent : public ShaderComponent {
public:
  RectComponent(ShaderManager& shader_manager, const std::string& program_id);
  ~RectComponent() override;
  void renderOpenGL() override;
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  struct RectVertex {
    glm::vec2 pos;
    glm::vec2 uv;
  };

  GLuint m_vbo = 0, m_vao = 0, m_ebo = 0;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RectComponent)
};

class KnobComponent : public RectComponent {
public:
  KnobComponent(ShaderManager& shader_manager, const std::string& program_id);
  void renderOpenGL() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

private:
  // For syncing across UI/OpenGL threads
  std::atomic<float> m_value = 0.0;
  std::optional<float> m_down_value = std::nullopt;
  std::optional<float> m_down_y = std::nullopt;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobComponent)
};

class TextComponent : public ShaderComponent {
public:
  TextComponent(ShaderManager& shader_manager, const std::string& program_id,
                FontManager& font_manager);
  ~TextComponent() override;
  void renderOpenGL() override;
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  FontManager& m_font_manager;
  std::string m_text;
  GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
  struct RectVertex {
    glm::vec2 pos;
    glm::vec2 uv;
  };
};
