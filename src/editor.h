#pragma once

#include "processor.h"

#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
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
