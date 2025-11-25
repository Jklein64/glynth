#include "shader_manager.h"
#include "shaders.h"

#include <cassert>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/format.h>
#ifdef GLYNTH_HSR
#include <fstream>
#include <sstream>
#endif

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
    : m_context(context) {
#ifdef GLYNTH_HSR
  m_file_watcher.addWatch(s_shader_dir, this);
  m_file_watcher.watch();
#endif
}

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
#ifdef GLYNTH_HSR
  auto id = m_name_to_id.at(name);
  std::lock_guard<std::mutex> lk(m_mutex);
  m_dirty.push_back(id);
#else
  juce::ignoreUnused(name);
#endif
}

void ShaderManager::tryUpdateDirty() {
#ifdef GLYNTH_HSR
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
        fmt::println(R"(Updated shader program "{}")", id);
      }
    }
    m_dirty.clear();
  }
#endif
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

#ifdef GLYNTH_HSR
std::string ShaderManager::readFile(const std::string& filename) const {
  auto path = std::filesystem::path(s_shader_dir).append(filename);
  std::ifstream infile(path.c_str());
  std::stringstream buffer;
  buffer << infile.rdbuf();
  return buffer.str();
}
#endif

void ShaderManager::handleFileAction(efsw::WatchID, const std::string&,
                                     const std::string& filename,
                                     efsw::Action action,
                                     std::string old_filename) {
  if (action == efsw::Actions::Modified) {
    std::string name = std::filesystem::path(filename).stem();
    if (m_name_to_id.contains(name)) {
      markDirty(name);
      fmt::println(R"(Marked "{}" as dirty)", filename);
    }
  }

  else if (action == efsw::Actions::Moved) {
    std::string name = std::filesystem::path(old_filename).stem();
    if (m_name_to_id.contains(name)) {
      fmt::println(stderr,
                   R"(Warning: rename "{}" -> "{}" invalidates hot reloading)",
                   old_filename, filename);
    }
  }

  else if (action == efsw::Actions::Delete) {
    std::string name = std::filesystem::path(filename).stem();
    if (m_name_to_id.contains(name)) {
      fmt::println(stderr,
                   R"(Warning: deletion of "{}" invalidates hot reloading)",
                   filename);
    }
  }
}
