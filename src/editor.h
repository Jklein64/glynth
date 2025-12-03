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
  TextComponent(ShaderManager& shader_manager, const std::string& program_id);
  void renderOpenGL() override;
  void paint(juce::Graphics& g) override;

private:
  FT_Library m_ft_library;
  std::string m_text;
};
