#include "util.h"

namespace util {

bool IsSubPath(fs::path path, fs::path base) {
  // Приводим оба пути к каноничному виду (без . и ..)
  path = fs::weakly_canonical(path);
  base = fs::weakly_canonical(base);

  // Проверяем, что все компоненты base содержатся внутри path
  for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
    if (p == path.end() || *p != *b) {
      return false;
    }
  }
  return true;
}

std::string UrlDecode(const std::string &s) {
  std::string str;
  str.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      const std::string r = s.substr(i + 1, 2);
      str.push_back(std::stoul(r, nullptr, 16));
      i += 2;
      continue;
    } else if (s[i] == '+') {
      str.push_back(' ');
    } else {
      str.push_back(s[i]);
    }
  }
  return str;
}

};  // namespace util