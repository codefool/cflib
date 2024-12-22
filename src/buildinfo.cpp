#include <ostream>
#include "buildinfo.h"

namespace libcf {

const build_info bi;

std::ostream& operator<<(std::ostream& os, const build_info& bi) {
    os << bi.app_name << ' ' << bi.ver_major << '.' << bi.ver_minor << '.' << bi.ver_build;
    return os;
}

}
