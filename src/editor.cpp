#include "editor.h"
#include "processor.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glm/ext.hpp>

GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), m_processor_ref(p), m_shader_manager(m_context),
      m_font_manager(m_context) {
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
  m_message_lock.enter();
  // Get display scale, which is needed to render Freetype fonts correctly.
  // Freetype doesn't distinguish between logical pixels and physical pixels,
  // so creates bitmaps at half the desired resolution on high-dpi devices
  auto& displays = juce::Desktop::getInstance().getDisplays();
  auto* display = displays.getDisplayForRect(getScreenBounds());
  m_font_manager.setDisplayScale(display->scale);
  m_message_lock.exit();

  m_font_manager.addFace("SplineSansMono-Bold", 20);
  m_shader_manager.addProgram("bg", "ortho", "vt220");
  m_shader_manager.addProgram("rect", "rect", "rect");
  m_shader_manager.addProgram("knob", "rect", "knob");
  m_shader_manager.addProgram("char", "rect", "char");
  auto bg = std::make_unique<BackgroundComponent>(*this, "bg");
  auto rect = std::make_unique<RectComponent>(*this, "rect");
  auto knob = std::make_unique<KnobComponent>(
      *this, "knob", m_processor_ref.getLowCutoffParam());
  auto number = std::make_unique<NumberComponent>(
      *this, "char", m_processor_ref.getLowCutoffParam());

  m_message_lock.enter();
  addAndMakeVisible(bg.get());
  bg->setBounds(getLocalBounds());
  addAndMakeVisible(rect.get());
  rect->setBounds(100, 100, 100, 100);
  addAndMakeVisible(knob.get());
  knob->setBounds(200, 200, 100, 100);
  addAndMakeVisible(number.get());
  number->setBounds(325, 200, 100, 50);
  m_message_lock.exit();

  m_shader_components.push_back(std::move(bg));
  m_shader_components.push_back(std::move(rect));
  m_shader_components.push_back(std::move(knob));
  m_shader_components.push_back(std::move(number));
}

void GlynthEditor::renderOpenGL() {
  m_shader_manager.tryUpdateDirty();

  using namespace juce::gl;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  juce::OpenGLHelpers::clear(juce::Colours::black);
  for (auto& component : m_shader_components) {
    component->renderOpenGL();
  }
}

void GlynthEditor::openGLContextClosing() {}

ShaderComponent::ShaderComponent(GlynthEditor& editor_ref,
                                 const std::string& program_id)
    : m_editor_ref(editor_ref), m_shader_manager(editor_ref.m_shader_manager),
      m_font_manager(editor_ref.m_font_manager), m_program_id(program_id) {}

BackgroundComponent::BackgroundComponent(GlynthEditor& editor_ref,
                                         const std::string& program_id)
    : ShaderComponent(editor_ref, program_id) {
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

RectComponent::RectComponent(GlynthEditor& editor_ref,
                             const std::string& program_id)
    : ShaderComponent(editor_ref, program_id) {
  using namespace juce::gl;
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ebo);
  glGenVertexArrays(1, &m_vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  std::array indices = {0u, 1u, 2u, 0u, 2u, 3u};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(),
               GL_STATIC_DRAW);
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
  auto bounds = getBounds();
  auto width = bounds.getWidth();
  auto height = bounds.getHeight();
  float window_width = static_cast<float>(m_editor_ref.getWidth());
  float window_height = static_cast<float>(m_editor_ref.getHeight());
  float x = static_cast<float>(bounds.getX());
  float y = window_height - static_cast<float>(bounds.getY() + height);
  float w = static_cast<float>(width);
  float h = static_cast<float>(height);
  std::array<RectVertex, 4> vertices = {
      RectVertex{.pos = glm::vec2(x, y), .uv = glm::vec2(0, 0)},
      RectVertex{.pos = glm::vec2(x, y + h), .uv = glm::vec2(0, 1)},
      RectVertex{.pos = glm::vec2(x + w, y + h), .uv = glm::vec2(1, 1)},
      RectVertex{.pos = glm::vec2(x + w, y), .uv = glm::vec2(1, 0)}};
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(),
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
  auto resolution = glm::vec2(width, height);
  m_shader_manager.setUniform(m_program_id, "u_resolution", resolution);
  // Add projection matrix as uniform by getting parent (editor) bounds
  glm::mat4 projection = glm::ortho(0.0f, window_width, 0.0f, window_height);
  m_shader_manager.setUniform(m_program_id, "u_projection", projection);
}

KnobComponent::KnobComponent(GlynthEditor& editor_ref,
                             const std::string& program_id,
                             juce::AudioParameterFloat* param)
    : RectComponent(editor_ref, program_id), m_param(param) {
  m_range = m_param->getNormalisableRange();
}

void KnobComponent::renderOpenGL() {
  RectComponent::renderOpenGL();
  // Uniforms can only be updated from the OpenGL thread
  // useProgram() was already called by RectComponent::renderOpenGL()
  float value = m_range.convertTo0to1(m_param->get());
  m_shader_manager.setUniform(m_program_id, "u_value", value);
}

void KnobComponent::mouseDown(const juce::MouseEvent& e) {
  m_down_y = e.position.y;
  m_down_value = m_range.convertTo0to1(m_param->get());
}

void KnobComponent::mouseDrag(const juce::MouseEvent& e) {
  if (e.mouseWasDraggedSinceMouseDown() && m_down_value && m_down_y) {
    // Flipped since JUCE inverts the y coordinate
    float delta = (m_down_y.value() - e.position.y) / 100;
    float proportion = std::clamp(m_down_value.value() + delta, 0.0f, 1.0f);
    *m_param = m_range.convertFrom0to1(proportion);
  }
}

void KnobComponent::mouseUp(const juce::MouseEvent&) {
  m_down_y = std::nullopt;
  m_down_value = std::nullopt;
}

TextComponent::TextComponent(GlynthEditor& editor_ref,
                             const std::string& program_id)
    : ShaderComponent(editor_ref, program_id) {
  m_text = "20000.2Hz";
  // Initialize buffers
  using namespace juce::gl;
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ebo);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  // Reserve enough buffer space for an indexed quad
  glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(RectVertex), nullptr,
               GL_DYNAMIC_DRAW);
  std::array indices = {0u, 1u, 2u, 0u, 2u, 3u};
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex), nullptr);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex),
      reinterpret_cast<const void*>(offsetof(RectVertex, uv)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  // Unbind buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

TextComponent::~TextComponent() {
  using namespace juce::gl;
  glDeleteBuffers(1, &m_vbo);
  glDeleteVertexArrays(1, &m_vao);
}

void TextComponent::renderOpenGL() {
  using namespace juce::gl;
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_shader_manager.useProgram(m_program_id);
  glActiveTexture(GL_TEXTURE0);
  // TODO implement vertical and horizontal centering
  auto bounds = getBounds();
  auto parent_height = static_cast<float>(getParentHeight());
  float height = static_cast<float>(bounds.getHeight());
  // origin is where to start drawing text
  glm::vec2 origin(static_cast<float>(bounds.getX()),
                   parent_height - static_cast<float>(bounds.getY()) - height);
  for (char c_raw : m_text) {
    auto& c = m_font_manager.getCharacter("SplineSansMono-Bold", c_raw, 20);
    float x = origin.x + c.bearing.x;
    float y = origin.y - (c.size.y - c.bearing.y);
    float w = c.size.x;
    float h = c.size.y;
    std::array<RectVertex, 4> vertices = {
        RectVertex{.pos = glm::vec2(x, y), .uv = glm::vec2(0, 0)},
        RectVertex{.pos = glm::vec2(x, y + h), .uv = glm::vec2(0, 1)},
        RectVertex{.pos = glm::vec2(x + w, y + h), .uv = glm::vec2(1, 1)},
        RectVertex{.pos = glm::vec2(x + w, y), .uv = glm::vec2(1, 0)},
    };
    glBindTexture(GL_TEXTURE_2D, c.texture);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices.data());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    origin.x += c.advance;
  }
  // Unbind buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void TextComponent::paint(juce::Graphics& g) {
  g.setColour(juce::Colours::white);
  g.drawRect(getLocalBounds());
}

void TextComponent::resized() {
  // Add projection matrix as uniform by getting parent (editor) bounds
  float w = static_cast<float>(m_editor_ref.getWidth());
  float h = static_cast<float>(m_editor_ref.getHeight());
  m_shader_manager.useProgram(m_program_id);
  glm::mat4 projection = glm::ortho(0.0f, w, 0.0f, h);
  m_shader_manager.setUniform(m_program_id, "u_projection", projection);
}

NumberComponent::NumberComponent(GlynthEditor& editor_ref,
                                 const std::string& program_id,
                                 juce::AudioParameterFloat* param)
    : TextComponent(editor_ref, program_id), m_param(param) {}

void NumberComponent::renderOpenGL() {
  m_text = fmt::format("{: >7.1f}Hz", m_param->get());
  TextComponent::renderOpenGL();
}
