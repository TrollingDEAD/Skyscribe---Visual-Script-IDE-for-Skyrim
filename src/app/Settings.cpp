#include "app/Settings.h"

namespace app {

Settings& Settings::Get() {
    static Settings instance;
    return instance;
}

} // namespace app
