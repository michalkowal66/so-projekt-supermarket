#ifndef CCOL_H
#define CCOL_H

#include <iostream>

// Console styles
std::ostream& warning(std::ostream& os);
std::ostream& success(std::ostream& os);
std::ostream& success_important(std::ostream& os);
std::ostream& error(std::ostream& os);
std::ostream& fatal(std::ostream& os);
std::ostream& info(std::ostream& os);
std::ostream& info_alt(std::ostream& os);
std::ostream& info_important(std::ostream& os);

// Reset color
std::ostream& reset_color(std::ostream& os);

#endif