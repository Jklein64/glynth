#pragma once

#include <string>

class FreetypeError : public std::runtime_error {
public:
  explicit FreetypeError(const std::string& msg) : std::runtime_error(msg) {}
  explicit FreetypeError(char* msg) : std::runtime_error(msg) {}
};

class GlynthError : public std::runtime_error {
public:
  explicit GlynthError(const std::string& msg) : std::runtime_error(msg) {}
  explicit GlynthError(char* msg) : std::runtime_error(msg) {}
};
