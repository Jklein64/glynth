#include "editor.h"
#include "fonts.h"
#include "processor.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glm/ext.hpp>
#include <regex>

class FreetypeError : public std::runtime_error {
public:
  explicit FreetypeError(const std::string& msg) : std::runtime_error(msg) {}
  explicit FreetypeError(char* msg) : std::runtime_error(msg) {}
};

FontManager::FontManager(juce::OpenGLContext& context) : m_context(context) {
  FT_Error err;
  if (err = FT_Init_FreeType(&m_library); err != 0) {
    throw FreetypeError(FT_Error_String(err));
  }
}

class FontManagerError : public std::runtime_error {
public:
  explicit FontManagerError(const std::string& msg) : std::runtime_error(msg) {}
  explicit FontManagerError(char* msg) : std::runtime_error(msg) {}
};

FontManager::~FontManager() { FT_Done_FreeType(m_library); }

void FontManager::addFace(std::string_view face_name, FT_UInt pixel_height) {
  assert(m_context.isAttached() && m_context.isActive());
  // Get display scale, which is needed to render Freetype fonts correctly.
  // Freetype doesn't distinguish between logical pixels and physical pixels,
  // so creates bitmaps at half the desired resolution on high-dpi devices
  m_message_lock.enter();
  auto& desktop = juce::Desktop::getInstance();
  auto* display = desktop.getDisplays().getPrimaryDisplay();
  m_display_scale = static_cast<float>(display->scale);
  m_message_lock.exit();
  // Load from binary data; assumes face_name is the non-extension filename
  std::string resource_name = std::string(face_name) + "_ttf";
  resource_name = std::regex_replace(resource_name, std::regex("-"), "");
  FT_Face face;
  int file_size;
  auto file_base = fonts::getNamedResource(resource_name.c_str(), file_size);
  if (file_base == nullptr) {
    throw FontManagerError(
        fmt::format(R"(No resource with name "{}")", resource_name));
  }
  if (auto err = FT_New_Memory_Face(m_library,
                                    reinterpret_cast<const FT_Byte*>(file_base),
                                    file_size, 0, &face)) {
    throw FreetypeError(FT_Error_String(err));
  }
  // Render face to bitmaps. Interpret height in logical pixels
  auto& charmap =
      m_character_maps[std::make_pair(std::string(face_name), pixel_height)];
  pixel_height = static_cast<FT_UInt>(pixel_height * m_display_scale);
  FT_Set_Pixel_Sizes(face, 0, pixel_height);
  for (size_t i = 0; i < charmap.size(); i++) {
    Character c(static_cast<FT_ULong>(i), face);
    // Apply display scaling so they're rendered with pixel_height pixels
    c.size /= m_display_scale;
    c.bearing /= m_display_scale;
    c.advance /= static_cast<float>(m_display_scale);
    charmap[i] = std::move(c);
  }
  // Cleanup
  FT_Done_Face(face);
}

const FontManager::Character&
FontManager::getCharacter(std::string_view face_name, char character,
                          FT_UInt pixel_height) {
  assert(m_context.isAttached() && m_context.isActive());
  auto key = std::make_pair(std::string(face_name), pixel_height);
  if (auto it = m_character_maps.find(key); it != m_character_maps.end()) {
    return it->second[static_cast<size_t>(character)];
  } else {
    throw FontManagerError(fmt::format(
        R"(Unable to find Character for '{}' in face "{}" with height {})",
        character, face_name, pixel_height));
  }
}

FontManager::Character::Character(FT_ULong code, FT_Face face) {
  if (auto err = FT_Load_Char(face, code, FT_LOAD_RENDER)) {
    throw FreetypeError(FT_Error_String(err));
  }
  // Read from glyph slot
  FT_GlyphSlot glyph = face->glyph;
  size = glm::vec2(glyph->bitmap.width, glyph->bitmap.rows);
  bearing = glm::vec2(glyph->bitmap_left, glyph->bitmap_top);
  advance = static_cast<float>(glyph->advance.x) / 64;
  // Create texture from bitmap
  using namespace juce::gl;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  // Disable byte-alignment restriction
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               static_cast<GLsizei>(glyph->bitmap.width),
               static_cast<GLsizei>(glyph->bitmap.rows), 0, GL_RED,
               GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), processor_ref(p), m_shader_manager(m_context),
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
  m_font_manager.addFace("SplineSansMono-Bold", 20);
  m_shader_manager.addProgram("bg", "ortho", "vt220");
  m_shader_manager.addProgram("rect", "rect", "rect");
  m_shader_manager.addProgram("knob", "rect", "knob");
  m_shader_manager.addProgram("char", "rect", "char");
  auto bg = std::make_unique<BackgroundComponent>(m_shader_manager, "bg");
  auto rect = std::make_unique<RectComponent>(m_shader_manager, "rect");
  auto knob = std::make_unique<KnobComponent>(m_shader_manager, "knob");
  auto text =
      std::make_unique<TextComponent>(m_shader_manager, "char", m_font_manager);
  // This callback is not run on the main (message) thread; JUCE requires lock
  m_message_lock.enter();
  addAndMakeVisible(bg.get());
  bg->setBounds(getLocalBounds());
  addAndMakeVisible(rect.get());
  rect->setBounds(100, 100, 100, 100);
  addAndMakeVisible(knob.get());
  knob->setBounds(200, 200, 100, 100);
  addAndMakeVisible(text.get());
  text->setBounds(325, 200, 100, 50);
  m_message_lock.exit();
  m_shader_components.push_back(std::move(bg));
  m_shader_components.push_back(std::move(rect));
  m_shader_components.push_back(std::move(knob));
  m_shader_components.push_back(std::move(text));
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
  auto parent_width = static_cast<float>(getParentWidth());
  auto parent_height = static_cast<float>(getParentHeight());
  float x = static_cast<float>(bounds.getX());
  float y = parent_height - static_cast<float>(bounds.getY() + height);
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
  glm::mat4 projection = glm::ortho(0.0f, parent_width, 0.0f, parent_height);
  m_shader_manager.setUniform(m_program_id, "u_projection", projection);
}

KnobComponent::KnobComponent(ShaderManager& shader_manager,
                             const std::string& program_id)
    : RectComponent(shader_manager, program_id) {}

void KnobComponent::renderOpenGL() {
  RectComponent::renderOpenGL();
  // Uniforms can only be updated from the OpenGL thread
  // useProgram() was already called by RectComponent::renderOpenGL()
  m_shader_manager.setUniform(m_program_id, "u_value", m_value.load());
}

void KnobComponent::mouseDown(const juce::MouseEvent& e) {
  m_down_y = e.position.y;
  m_down_value = m_value.load();
}

void KnobComponent::mouseDrag(const juce::MouseEvent& e) {
  if (e.mouseWasDraggedSinceMouseDown() && m_down_value && m_down_y) {
    // Flipped since JUCE inverts the y coordinate
    float delta = (m_down_y.value() - e.position.y) / 100;
    m_value = std::clamp(m_down_value.value() + delta, 0.0f, 1.0f);
  }
}

void KnobComponent::mouseUp(const juce::MouseEvent&) {
  m_down_y = std::nullopt;
  m_down_value = std::nullopt;
}

TextComponent::TextComponent(ShaderManager& shader_manager,
                             const std::string& program_id,
                             FontManager& font_manager)
    : ShaderComponent(shader_manager, program_id),
      m_font_manager(font_manager) {
  m_text = "Glynth";
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
  float w = static_cast<float>(getParentWidth());
  float h = static_cast<float>(getParentHeight());
  m_shader_manager.useProgram(m_program_id);
  glm::mat4 projection = glm::ortho(0.0f, w, 0.0f, h);
  m_shader_manager.setUniform(m_program_id, "u_projection", projection);
}
