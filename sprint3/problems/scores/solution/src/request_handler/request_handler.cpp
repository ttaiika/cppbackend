#include "request_handler/request_handler.h"

#include <cctype>  // для isxdigit

namespace http_handler {

// Возвращает true, если каталог p содержится внутри base_path
bool RequestHandler::IsSubPath(fs::path path, fs::path base) {
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

// Функция для перевода hex-пары в символ
char FromHex(const std::string& hex) {
    unsigned int value;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> value;
    return static_cast<char>(value);
}

std::string RequestHandler::URLDecode(const std::string& str) const {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i+2 < str.size() && std::isxdigit(str[i+1]) && std::isxdigit(str[i+2])) {
                std::string hex = str.substr(i+1, 2);
                result += FromHex(hex);
                i+=2;
            } else {
                result+= '%';
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

}  // namespace http_handler