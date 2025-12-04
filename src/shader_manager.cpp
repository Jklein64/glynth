#include "shader_manager.h"
#include "shaders.h"

#include <cassert>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>
#ifdef GLYNTH_HSR
#include <fstream>
#include <sstream>
#endif

ShaderManager::ProgramMetadata::ProgramMetadata(const std::string& vname,
                                                const std::string& fname)
    : vert_name(vname), frag_name(fname) {
  vert_res_name = fmt::format("{}_vert", vert_name);
  frag_res_name = fmt::format("{}_frag", frag_name);
  const char* tmp;
  tmp = shaders::getNamedResourceOriginalFilename(vert_res_name.c_str());
  assert(tmp != nullptr);
  vert_filename = tmp;
  tmp = shaders::getNamedResourceOriginalFilename(frag_res_name.c_str());
  assert(tmp != nullptr);
  frag_filename = tmp;
}

ShaderManager::ShaderManager(juce::OpenGLContext& context, FILE* log_file)
    : m_context(context), m_log_file(log_file) {
#ifdef GLYNTH_HSR
  m_file_watcher.addWatch(s_shader_dir, this);
  m_file_watcher.watch();
#endif
}

bool ShaderManager::addProgram(const ProgramId& id, const ShaderName& vert_name,
                               const ShaderName& frag_name) {
  assert(m_context.isAttached() && m_context.isActive());
  assert(!m_programs.contains(id));
  int size; // JUCE's `dataSizeInBytes` field doesn't accept nullptr
  std::string vert_res_name = fmt::format("{}_vert", vert_name);
  std::string frag_res_name = fmt::format("{}_frag", frag_name);
  auto vert_source =
      m_vert_sources.contains(vert_name)
          ? m_vert_sources.at(vert_name).c_str()
          : shaders::getNamedResource(vert_res_name.c_str(), size);
  auto frag_source =
      m_frag_sources.contains(frag_name)
          ? m_frag_sources.at(frag_name).c_str()
          : shaders::getNamedResource(frag_res_name.c_str(), size);
  if (vert_source == nullptr) {
    fmt::println(m_log_file,
                 R"(Error loading shader with name "{}" (resource name "{}"))",
                 vert_name, vert_res_name);
    return false;
  }
  if (frag_source == nullptr) {
    fmt::println(m_log_file,
                 R"(Error loading shader with name "{}" (resource name "{}"))",
                 frag_name, frag_res_name);
    return false;
  }

  ProgramMetadata metadata(vert_name, frag_name);
  auto program = createProgram(metadata, vert_source, frag_source);
  if (program == nullptr) {
    return false;
  }

  // Don't insert if it's already there
  m_vert_sources.insert(std::make_pair(vert_name, vert_source));
  m_frag_sources.insert(std::make_pair(frag_name, frag_source));
  m_metadata.insert(std::make_pair(id, metadata));
  m_programs.insert(std::make_pair(id, std::move(program)));
  return true;
}

bool ShaderManager::useProgram(const ProgramId& id) {
  assert(m_context.isAttached() && m_context.isActive());
  if (auto it = m_programs.find(id); it != m_programs.end()) {
    it->second->use();
    return true;
  } else {
    fmt::println(m_log_file, R"(No program found with id "{}")", id);
    return false;
  }
}

bool ShaderManager::setUniform(const ProgramId& id, const std::string& name,
                               Uniform value) {
  assert(m_context.isAttached() && m_context.isActive());
  auto uniform_refresher = [this, id, name, value] {
    if (auto it = m_programs.find(id); it != m_programs.end()) {
      juce::OpenGLShaderProgram& program = *it->second;
      std::visit(
          [&program, &name](auto& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, float>) {
              program.setUniform(name.c_str(), arg);
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
              program.setUniform(name.c_str(), arg.x, arg.y);
            } else if constexpr (std::is_same_v<T, glm::mat4>) {
              program.setUniformMat4(name.c_str(), glm::value_ptr(arg), 1,
                                     false);
            }
          },
          value);
      return true;
    } else {
      fmt::println(m_log_file, R"(No program found with id "{}")", id);
      return false;
    }
  };
  bool result = uniform_refresher();
#ifdef GLYNTH_HSR
  m_uniform_refreshers[id].push_back(std::move(uniform_refresher));
#endif
  return result;
}

void ShaderManager::markDirty(const ProgramId& id) {
#ifdef GLYNTH_HSR
  std::lock_guard<std::mutex> lk(m_mutex);
  m_dirty.push_back(id);
#else
  juce::ignoreUnused(id);
#endif
}

void ShaderManager::tryUpdateDirty() {
#ifdef GLYNTH_HSR
  std::lock_guard<std::mutex> lk(m_mutex);
  if (!m_dirty.empty()) {
    for (auto& id : m_dirty) {
      ProgramMetadata& metadata = m_metadata.at(id);
      auto vert_source = readFile(metadata.vert_filename);
      auto frag_source = readFile(metadata.frag_filename);
      auto program =
          createProgram(metadata, vert_source.c_str(), frag_source.c_str());
      if (program != nullptr) {
        program->use();
        if (m_uniform_refreshers.contains(id)) {
          for (auto& refresher : m_uniform_refreshers.at(id)) {
            refresher();
          }
        }
        m_programs.insert_or_assign(id, std::move(program));
        m_vert_sources.insert_or_assign(id, std::move(vert_source));
        m_frag_sources.insert_or_assign(id, std::move(frag_source));
        fmt::println(m_log_file, R"(Updated shader program "{}")", id);
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
    fmt::println(m_log_file, "Error compiling vertex shader {}: {}",
                 metadata.vert_filename, program->getLastError().toStdString());
    return nullptr;
  }
  if (!program->addFragmentShader(frag_source)) {
    fmt::println(m_log_file, "Error compiling fragment shader {}: {}",
                 metadata.frag_filename, program->getLastError().toStdString());
    return nullptr;
  }
  if (!program->link()) {
    fmt::println(m_log_file, "Error linking shaders: {}",
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
    for (const auto& [id, metadata] : m_metadata) {
      if (name == metadata.frag_name || name == metadata.vert_name) {
        markDirty(id);
        fmt::println(m_log_file, R"(Marked "{}" as dirty)", filename);
      }
    }
  }

  else if (action == efsw::Actions::Moved) {
    std::string name = std::filesystem::path(old_filename).stem();
    fmt::println(
        m_log_file,
        R"(Warning: rename "{}" -> "{}" might invalidate hot reloading)",
        old_filename, filename);
  }

  else if (action == efsw::Actions::Delete) {
    std::string name = std::filesystem::path(filename).stem();
    fmt::println(m_log_file,
                 R"(Warning: deletion of "{}" might invalidate hot reloading)",
                 filename);
  }
}
