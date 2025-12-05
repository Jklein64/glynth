#pragma once

#include "font_manager.h"
#include "processor.h"
#include "shader_manager.h"

#include <efsw/efsw.hpp>
#include <fmt/base.h>
#include <glm/glm.hpp>
#include <juce_events/juce_events.h>
#include <juce_opengl/juce_opengl.h>
#include <vector>

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
  GlynthProcessor& m_processor_ref;
  juce::MessageManager::Lock m_message_lock;
  juce::OpenGLContext m_context;
  ShaderManager m_shader_manager;
  FontManager m_font_manager;
  std::vector<std::unique_ptr<ShaderComponent>> m_shader_components;

  friend class ShaderComponent;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};

class ShaderComponent : public juce::Component {
public:
  ShaderComponent(GlynthEditor& editor_ref, const std::string& program_id);
  virtual void renderOpenGL() = 0;

protected:
  GlynthEditor& m_editor_ref;
  GlynthProcessor& m_processor_ref;
  ShaderManager& m_shader_manager;
  FontManager& m_font_manager;
  // ID of the shader program associated with this component
  std::string m_program_id;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaderComponent)
};

class BackgroundComponent : public ShaderComponent {
public:
  BackgroundComponent(GlynthEditor& editor_ref, const std::string& program_id);
  ~BackgroundComponent() override;
  void renderOpenGL() override;

private:
  GLuint m_vbo = 0, m_vao = 0;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackgroundComponent)
};

class RectComponent : public ShaderComponent {
public:
  RectComponent(GlynthEditor& editor_ref, const std::string& program_id);
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
  KnobComponent(GlynthEditor& editor_ref, const std::string& program_id,
                std::string_view param_id);
  void renderOpenGL() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

private:
  juce::AudioParameterFloat& m_param;
  juce::NormalisableRange<float> m_range;
  std::optional<float> m_down_value = std::nullopt;
  std::optional<float> m_down_y = std::nullopt;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobComponent)
};

class TextComponent : public ShaderComponent {
public:
  TextComponent(GlynthEditor& editor_ref, const std::string& program_id,
                std::string_view text);
  ~TextComponent() override;
  void renderOpenGL() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  void setFontFace(std::string_view face_name, FT_UInt pixel_height);

protected:
  std::string m_text;
  std::string m_face_name;
  FT_UInt m_pixel_height;

private:
  GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
  struct RectVertex {
    glm::vec2 pos;
    glm::vec2 uv;
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextComponent)
};

class NumberComponent : public TextComponent {
public:
  NumberComponent(GlynthEditor& editor_ref, const std::string& program_id,
                  std::string_view param_id, std::string_view format);
  void renderOpenGL() override;

private:
  juce::AudioParameterFloat& m_param;
  std::string m_format;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NumberComponent)
};

class ParameterComponent : public RectComponent {
public:
  ParameterComponent(GlynthEditor& editor_ref, const std::string& program_id,
                     std::string_view param_id, std::string_view format);
  void renderOpenGL() override;
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
  juce::AudioParameterFloat& m_param;
  NumberComponent m_number;
  KnobComponent m_knob;
  TextComponent m_label;
  juce::MessageManager::Lock m_message_lock;
  glm::vec2 m_offset;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterComponent)
};

class LissajousComponent : public RectComponent {
public:
  LissajousComponent(GlynthEditor& editor_ref, const std::string& program_id);
  ~LissajousComponent() override;
  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void focusGained(FocusChangeType cause) override;
  void focusLost(FocusChangeType cause) override;
  bool keyPressed(const juce::KeyPress& key) override;
  void renderOpenGL() override;

private:
  static inline std::array s_defocusing_keys = {juce::KeyPress::returnKey,
                                                juce::KeyPress::escapeKey,
                                                juce::KeyPress::tabKey};

  float getTimeUniform();

  juce::MessageManager::Lock m_message_lock;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_last_focus_time;
  bool m_focused = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LissajousComponent)
};
