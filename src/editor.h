#pragma once

#include "processor.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
#include <optional>
#include <string_view>
#include <vector>
class OpenGlComponent final : public juce::Component,
                              public juce::OpenGLRenderer {
public:
  explicit OpenGlComponent();
  ~OpenGlComponent() override;

  void paint(juce::Graphics&) override;
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;

private:
  inline static auto s_shader_dir =
      std::filesystem::path(__FILE__).parent_path().parent_path().append(
          "shader");
  inline static auto s_vert_file =
      std::filesystem::path(s_shader_dir).append("vert.glsl");
  inline static auto s_frag_file =
      std::filesystem::path(s_shader_dir).append("frag.glsl");

  std::optional<std::string> readFile(std::string_view filename) const;

  juce::OpenGLContext m_context;

  struct Vertex {
    float position[2];
    float color[4];
  };

  std::vector<Vertex> m_vertex_buffer;
  std::vector<uint32_t> m_index_buffer;
  GLuint m_vbo;
  GLuint m_ibo;
  std::unique_ptr<juce::OpenGLShaderProgram> m_shader_program;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
};

//==============================================================================
class GlynthEditor final : public juce::AudioProcessorEditor {
public:
  explicit GlynthEditor(GlynthProcessor&);
  ~GlynthEditor() override;

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;

private:
  GlynthProcessor& processor_ref;
  OpenGlComponent m_opengl_component;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
