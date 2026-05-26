// app/main.cpp
// Entry point. All startup logic lives in Engine::start().
#include "engine/Engine.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        ndde::Engine engine;
        engine.start(argc > 0 ? argv[0] : "");   // loads engine_config.json (or uses defaults)
        engine.run();     // blocks until window close
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
