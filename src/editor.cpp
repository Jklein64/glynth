#include "editor.h"
#include "processor.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), processor_ref(p), m_shader_manager(m_context) {
  // Must set size for window to show properly
  setSize(840, 473);
  setOpaque(true);
  m_context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
  m_context.setRenderer(this);
  m_context.setContinuousRepainting(true);
  m_context.attachTo(*this);
}

GlynthEditor::~GlynthEditor() { m_context.detach(); }

void GlynthEditor::paint(juce::Graphics&) {}

void GlynthEditor::newOpenGLContextCreated() {
  m_shader_manager.addProgram("bg", "ortho_vert", "vt220");
  m_shader_manager.addProgram("rect", "rect_vert", "rect_frag");
  m_shader_manager.addProgram("knob", "rect_vert", "knob_frag");
  auto bg = std::make_unique<BackgroundComponent>(m_shader_manager, "bg");
  auto rect = std::make_unique<RectComponent>(m_shader_manager, "rect");
  auto knob = std::make_unique<RectComponent>(m_shader_manager, "knob");
  // This callback is not run on the main (message) thread; JUCE requires lock
  m_message_lock.enter();
  addAndMakeVisible(bg.get());
  bg->setBounds(getLocalBounds());
  addAndMakeVisible(rect.get());
  rect->setBounds(100, 100, 100, 100);
  addAndMakeVisible(knob.get());
  knob->setBounds(200, 200, 100, 100);
  m_message_lock.exit();
  m_shader_components.push_back(std::move(bg));
  m_shader_components.push_back(std::move(rect));
  m_shader_components.push_back(std::move(knob));
}

void GlynthEditor::renderOpenGL() {
  m_shader_manager.tryUpdateDirty();

  using namespace juce::gl;
  juce::OpenGLHelpers::clear(juce::Colours::black);
  for (auto& component : m_shader_components) {
    component->renderOpenGL();
  }
}

void GlynthEditor::openGLContextClosing() {}

ShaderComponent::ShaderComponent(ShaderManager& shader_manager,
                                 const std::string& program_id)
    : m_shader_manager(shader_manager), m_program_id(program_id) {}

BackgroundComponent::BackgroundComponent(ShaderManager& shader_manager,
                                         const std::string& program_id)
    : ShaderComponent(shader_manager, program_id) {
  using namespace juce::gl;
  glGenBuffers(1, &m_vbo);
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  std::array<GLfloat, 6> vertices = {-1, -1, 3, -1, -1, 3};
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
}

BackgroundComponent::~BackgroundComponent() {
  using namespace juce::gl;
  glDeleteBuffers(1, &m_vbo);
  glDeleteVertexArrays(1, &m_vao);
}

void BackgroundComponent::renderOpenGL() {
  using namespace juce::gl;
  glBindVertexArray(m_vao); // Also binds m_ebo
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_shader_manager.useProgram(m_program_id);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

RectComponent::RectComponent(ShaderManager& shader_manager,
                             const std::string& program_id)
    : ShaderComponent(shader_manager, program_id) {
  using namespace juce::gl;
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ebo);
  glGenVertexArrays(1, &m_vao);
}

RectComponent::~RectComponent() {
  using namespace juce::gl;
  glDeleteBuffers(1, &m_vbo);
  glDeleteBuffers(1, &m_ebo);
  glDeleteVertexArrays(1, &m_vao);
}

void RectComponent::renderOpenGL() {
  using namespace juce::gl;
  glBindVertexArray(m_vao); // Also binds m_ebo
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_shader_manager.useProgram(m_program_id);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);
}

void RectComponent::paint(juce::Graphics& g) {
  g.setColour(juce::Colours::white);
  g.drawRect(getLocalBounds());
}

void RectComponent::resized() {
  using namespace juce::gl;
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  // Convert to the portion of the canonical view volume this takes up
  auto bounds = getBounds();
  auto width = bounds.getWidth();
  auto height = bounds.getHeight();
  auto parent_width = static_cast<float>(getParentWidth());
  auto parent_height = static_cast<float>(getParentHeight());
  float x = static_cast<float>(bounds.getX()) / parent_width * 2 - 1;
  float y = (1 - static_cast<float>(bounds.getY()) / parent_height) * 2 - 1;
  float w = static_cast<float>(width) / parent_width * 2;
  float h = static_cast<float>(height) / parent_height * 2;
  // This seems backward because the JUCE coordinates are y-down but the ones
  // OpenGL needs are y-up, and JUCE 0,0 is the top left
  // 1 --- 2    x,y ------ x+w,y
  // |  /  |     |     /      |
  // 0 --- 3    x,y-h ---- x+w,y-h
  std::array<RectVertex, 4> vertices = {
      RectVertex{.pos = glm::vec2(x, y - h), .uv = glm::vec2(0, 0)},
      RectVertex{.pos = glm::vec2(x, y), .uv = glm::vec2(0, 1)},
      RectVertex{.pos = glm::vec2(x + w, y), .uv = glm::vec2(1, 1)},
      RectVertex{.pos = glm::vec2(x + w, y - h), .uv = glm::vec2(1, 0)}};
  std::array indices = {0u, 1u, 2u, 0u, 2u, 3u};
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(),
               GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex), nullptr);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex),
      reinterpret_cast<const void*>(offsetof(RectVertex, uv)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  // Must use shader before setting uniforms
  m_shader_manager.useProgram(m_program_id);
  // Apply pixel scale so that units are physical pixels
  auto& desktop = juce::Desktop::getInstance();
  auto* display = desktop.getDisplays().getPrimaryDisplay();
  double scale = display ? display->scale : 1.0;
  auto resolution = glm::vec2(width * scale, height * scale);
  fmt::println("setting u_resolution = ({}, {})", resolution.x, resolution.y);
  m_shader_manager.setUniform(m_program_id, "u_resolution", resolution);
}
