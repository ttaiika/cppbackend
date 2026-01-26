#pragma once

#include <filesystem>

namespace util {
namespace fs = std::filesystem;

bool IsSubPath(fs::path path, fs::path base);

std::string UrlDecode(const std::string &url);

}; // namespace util