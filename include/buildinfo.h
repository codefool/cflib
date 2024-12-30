#pragma once

#include <string>

namespace libcf {
struct build_info {
    std::string app_name = "libcf - library of codefool objects";
    int ver_major = 0;
    int ver_minor = 0;
    int ver_build = 42;
    friend std::ostream& operator<< (std::ostream& os, const build_info& bi);
};

extern const build_info bi;
}