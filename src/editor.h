#pragma once

#include "processor.h"
#include "shader_manager.h"

#include <efsw/efsw.hpp>
#include <glm/glm.hpp>
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
  juce::OpenGLContext m_context;
  ShaderManager m_shader_manager;
  std::vector<std::unique_ptr<ShaderComponent>> m_shader_components;

  // GLuint m_vbo;
  // GLuint m_vao;
  // std::vector<float> m_vertex_buffer;
  // std::vector<uint32_t> m_index_buffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};

class ShaderComponent {
public:
  ShaderComponent(ShaderManager& shader_manager, const std::string& program_id);
  virtual ~ShaderComponent();

  void renderOpenGL();
  // Do not call render() directly in the renderOpenGL method of
  // `juce::OpenGLRenderer`. Call `renderOpenGL()` instead.
  virtual void render() = 0;

protected:
  ShaderManager& m_shader_manager;
  // ID of the shader program associated with this component
  std::string m_program_id;
  // OpenGL objects (assumes everything is an indexed mesh)
  GLuint m_vbo = 0, m_vao = 0, m_ebo = 0;
};

class BackgroundComponent : public ShaderComponent {
public:
  BackgroundComponent(ShaderManager& shader_manager,
                      const std::string& program_id);

  void render() override;
};
