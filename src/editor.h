#pragma once

#include "processor.h"
#include "shader_manager.h"

#include <efsw/efsw.hpp>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
#include <vector>

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
  juce::OpenGLContext m_context;
  ShaderManager m_shader_manager;

  GLuint m_vbo;
  GLuint m_vao;
  std::vector<float> m_vertex_buffer;
  std::vector<uint32_t> m_index_buffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
