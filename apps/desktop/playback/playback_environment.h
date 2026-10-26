#pragma once
#include <clocale>
#include <stdexcept>

// Call once on the main thread, after Qt initializes the process locale and
// before creating playback clients or workers. libmpv requires C numeric syntax.
inline void initializePlaybackEnvironment() {
    if (!std::setlocale(LC_NUMERIC, "C"))
        throw std::runtime_error("Cannot initialize the playback numeric locale.");
}
