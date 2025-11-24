#include "editor.h"
#include "processor.h"
#include "shaders.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fstream>
#include <sstream>

ShaderManager::ProgramMetadata::ProgramMetadata(const std::string& vname,
                                                const std::string& fname)
    : vert_name(vname), frag_name(fname) {
  vert_res_name = fmt::format("{}_glsl", vert_name);
  frag_res_name = fmt::format("{}_glsl", frag_name);
  const char* tmp;
  tmp = shaders::getNamedResourceOriginalFilename(vert_res_name.c_str());
  assert(tmp != nullptr);
  vert_filename = tmp;
  tmp = shaders::getNamedResourceOriginalFilename(frag_res_name.c_str());
  assert(tmp != nullptr);
  frag_filename = tmp;
}

ShaderManager::ShaderManager(juce::OpenGLContext& context)
    : m_context(context) {}

bool ShaderManager::addProgram(const ProgramId& id, const ShaderName& vert_name,
                               const ShaderName& frag_name) {
  assert(m_context.isAttached() && m_context.isActive());
  int size; // JUCE's `dataSizeInBytes` field doesn't accept nullptr
  std::string vert_res_name = fmt::format("{}_glsl", vert_name);
  std::string frag_res_name = fmt::format("{}_glsl", frag_name);
  auto vert_source = shaders::getNamedResource(vert_res_name.c_str(), size);
  auto frag_source = shaders::getNamedResource(frag_res_name.c_str(), size);
  if (vert_source == nullptr) {
    fmt::println(stderr,
                 R"(Error loading shader with name "{}" (resource name "{}"))",
                 vert_name, vert_res_name);
    return false;
  }
  if (frag_source == nullptr) {
    fmt::println(stderr,
                 R"(Error loading shader with name "{}" (resource name "{}"))",
                 frag_name, frag_res_name);
    return false;
  }

  ProgramMetadata metadata(vert_name, frag_name);
  auto program = createProgram(metadata, vert_source, frag_source);
  if (program == nullptr) {
    return false;
  }

  m_vert_sources[vert_name] = std::string(vert_source);
  m_frag_sources[frag_name] = std::string(frag_source);
  m_name_to_id[vert_name] = id;
  m_name_to_id[frag_name] = id;
  m_metadata.insert_or_assign(id, metadata);
  // m_id_to_vert_name[id] = vert_name;
  // m_id_to_frag_name[id] = frag_name;
  m_programs[id] = std::move(program);
  return true;
}

bool ShaderManager::useProgram(const ProgramId& id) {
  assert(m_context.isAttached() && m_context.isActive());
  if (auto it = m_programs.find(id); it != m_programs.end()) {
    it->second->use();
    return true;
  } else {
    fmt::println(stderr, R"(No program found with id "{}")", id);
    return false;
  }
}

void ShaderManager::markDirty(const ShaderName& name) {
  if (auto it = m_name_to_id.find(name); it != m_name_to_id.end()) {
    // std::lock_guard<std::mutex> lk(m_mutex);
    m_dirty.push_back(it->second);
  } else {
    fmt::println(stderr, R"(No shader found with name "{}")", name);
  }
}

void ShaderManager::tryUpdateDirty() {
  if (!m_dirty.empty()) {
    for (auto& id : m_dirty) {
      ProgramMetadata& metadata = m_metadata.at(id);
      auto vert_source = readFile(metadata.vert_filename);
      auto frag_source = readFile(metadata.frag_filename);
      auto program =
          createProgram(metadata, vert_source.c_str(), frag_source.c_str());
      if (program != nullptr) {
        m_programs.insert_or_assign(id, std::move(program));
        m_vert_sources.insert_or_assign(id, std::move(vert_source));
        m_frag_sources.insert_or_assign(id, std::move(frag_source));
      }
    }
    m_dirty.clear();
  }
}

std::unique_ptr<juce::OpenGLShaderProgram>
ShaderManager::createProgram(const ProgramMetadata& metadata,
                             const char* vert_source, const char* frag_source) {
  assert(m_context.isAttached() && m_context.isActive());
  auto program = std::make_unique<juce::OpenGLShaderProgram>(m_context);
  if (!program->addVertexShader(vert_source)) {
    fmt::println(stderr, "Error compiling vertex shader {}: {}",
                 metadata.vert_filename, program->getLastError().toStdString());
    return nullptr;
  }
  if (!program->addFragmentShader(frag_source)) {
    fmt::println(stderr, "Error compiling fragment shader {}: {}",
                 metadata.frag_filename, program->getLastError().toStdString());
    return nullptr;
  }
  if (!program->link()) {
    fmt::println(stderr, "Error linking shaders: {}",
                 program->getLastError().toStdString());
    return nullptr;
  }
  return program;
}

std::string ShaderManager::readFile(const std::string& filename) const {
  auto path = std::filesystem::path(s_shader_dir).append(filename);
  std::ifstream infile(path.c_str());
  std::stringstream buffer;
  buffer << infile.rdbuf();
  return buffer.str();
}

GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), processor_ref(p), m_shader_manager(m_context),
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
  m_shader_manager.addProgram("bg", "vert", "frag");
  m_shader_manager.markDirty("vert");
  // reloadShaders();
}

void GlynthEditor::renderOpenGL() {
  using namespace juce::gl;

  // {
  //   std::unique_lock<std::mutex> lk{m_mutex, std::defer_lock};
  //   if (lk.try_lock() && m_dirty_shader.has_value()) {
  //     std::ifstream shader_source(*m_dirty_shader);
  //     std::stringstream buffer;
  //     buffer << shader_source.rdbuf();
  //     if (*m_dirty_shader == "vert.glsl") {
  //       m_vert_source = buffer.str();
  //     } else {
  //       m_frag_source = buffer.str();
  //     }
  //     reloadShaders();
  //     fmt::println("Updated shaders");
  //     m_dirty_shader = std::nullopt;
  //   }
  // }
  m_shader_manager.tryUpdateDirty();

  juce::OpenGLHelpers::clear(juce::Colours::black);
  // m_program->use();
  m_shader_manager.useProgram("bg");
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
