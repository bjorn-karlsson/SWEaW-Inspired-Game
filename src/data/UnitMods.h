#pragma once

#include <string>

#include "data/Database.h"

namespace gc {

/// Units edited or created in the in-game designer are written to a plain text
/// file next to the executable and folded back over the built-in roster the
/// next time the game starts. Anything the designer can change round-trips
/// through here.
namespace unitmods {

/// Default file name, relative to the working directory.
extern const char* kDefaultPath;

/// Applies a file of unit definitions on top of the database. Unknown keys
/// create brand new units. Returns the number of units touched, or -1 when the
/// file could not be read.
int load(Database& d, const std::string& path = kDefaultPath);

/// Writes every unit marked as custom. Returns the number written, or -1 on a
/// write error.
int save(const Database& d, const std::string& path = kDefaultPath);

/// Serialises one unit, used by the designer's "copy to clipboard" and by save.
std::string serialise(const UnitDef& u);

}  // namespace unitmods
}  // namespace gc
