#include "editor.h"
#include "processor.h"
#include "shaders.h"

#include <fmt/base.h>
#include <fstream>
#include <sstream>

GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), processor_ref(p),
      m_vert_source(shaders::vert_glsl), m_frag_source(shaders::frag_glsl) {
  // Must set size for window to show properly
  setSize(400, 300);
  setOpaque(true);
  m_context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
  m_context.setRenderer(this);
  m_context.setContinuousRepainting(true);
  m_context.attachTo(*this);
  m_file_watcher.addWatch(s_shader_dir, this);
  m_file_watcher.watch();
}

GlynthEditor::~GlynthEditor() { m_context.detach(); }

void GlynthEditor::paint(juce::Graphics&) {}

void GlynthEditor::newOpenGLContextCreated() {
  using namespace juce::gl;
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_vertex_buffer = {                 // x y r g b
                     -1, -1, 1, 0, 0, //
                     3,  -1, 0, 1, 0, //
                     -1, 3,  0, 0, 1};
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_vertex_buffer.size() * sizeof(float)),
               m_vertex_buffer.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        reinterpret_cast<GLvoid*>(sizeof(float) * 2));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  reloadShaders();
}

void GlynthEditor::renderOpenGL() {
  using namespace juce::gl;

  {
    std::unique_lock<std::mutex> lk{m_mutex, std::defer_lock};
    if (lk.try_lock() && m_dirty_shader.has_value()) {
      std::ifstream shader_source(*m_dirty_shader);
      std::stringstream buffer;
      buffer << shader_source.rdbuf();
      if (*m_dirty_shader == "vert.glsl") {
        m_vert_source = buffer.str();
      } else {
        m_frag_source = buffer.str();
      }
      reloadShaders();
      fmt::println("Updated shaders");
      m_dirty_shader = std::nullopt;
    }
  }

  juce::OpenGLHelpers::clear(juce::Colours::black);
  m_program->use();
  glBindVertexArray(m_vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GlynthEditor::openGLContextClosing() {}

void GlynthEditor::handleFileAction(efsw::WatchID, const std::string& dir,
                                    const std::string& filename, efsw::Action,
                                    std::string) {
  if (filename == "frag.glsl" || filename == "vert.glsl") {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_dirty_shader = dir + filename;
  }
}

void GlynthEditor::reloadShaders() {
  m_program.reset(new juce::OpenGLShaderProgram(m_context));
  if (m_program->addVertexShader(m_vert_source) &&
      m_program->addFragmentShader(m_frag_source) && m_program->link()) {
    m_program->use();
  } else {
    fmt::println(stderr, "Failed to load shaders");
  }
}
