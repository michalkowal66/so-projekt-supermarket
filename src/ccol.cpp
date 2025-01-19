#include "ccol.h"

std::ostream& warning(std::ostream& os) {
    os << "\033[33m"; // Yellow text
    return os;
}

std::ostream& success(std::ostream& os) {
    os << "\033[32m"; // Green text
    return os;
}

std::ostream& success_important(std::ostream& os) {
    os << "\033[1;32m"; // Bold green text
    return os;
}

std::ostream& error(std::ostream& os) {
    os << "\033[31m"; // Red text
    return os;
}

std::ostream& fatal(std::ostream& os) {
    os << "\033[1;41;37m"; // Red background bold white foreground text
    return os;
}

std::ostream& info(std::ostream& os) {
    os << "\033[36m"; // Cyan text
    return os;
}

std::ostream& info_alt(std::ostream& os) {
    os << "\033[35m"; // Magenta text
    return os;
}

std::ostream& info_important(std::ostream& os) {
    os << "\033[4;36m"; // Bold cyan text
    return os;
}

std::ostream& reset_color(std::ostream& os) {
    os << "\033[0m"; // Reset text color
    return os;
}