#include <cstring>
#include <string>

#include "ui/App.h"

namespace {

void printUsage() {
    std::printf(
        "Galactic Conquest - a Star Wars: Empire at War inspired RTS\n"
        "\n"
        "  --autostart              skip the menu and start the first campaign\n"
        "  --campaign N             campaign index (0..2)\n"
        "  --faction N              0 Republic, 1 CIS, 2 Hutts\n"
        "  --difficulty N           0 Padawan .. 3 Grand Master\n"
        "  --demo-battle            start straight in a tactical space battle\n"
        "  --demo-summary           fight a demo battle to the end, show the report\n"
        "  --demo-hud               grant your heroes and pause (HUD screenshots)\n"
        "  --screenshot FILE.bmp    render a few frames, save the image and exit\n"
        "  --frames N               which frame to capture (default 40)\n"
        "  --help                   this text\n");
}

}  // namespace

int main(int argc, char** argv) {
    gc::ui::AppOptions options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](int fallback) {
            return (i + 1 < argc) ? std::atoi(argv[++i]) : fallback;
        };
        if (arg == "--autostart") {
            options.autostart = true;
        } else if (arg == "--demo-battle") {
            options.demoBattle = true;
        } else if (arg == "--demo-summary") {
            options.demoSummary = true;
        } else if (arg == "--demo-hud") {
            options.demoHud = true;
        } else if (arg == "--campaign") {
            options.campaign = next(0);
        } else if (arg == "--faction") {
            options.faction = next(0);
        } else if (arg == "--difficulty") {
            options.difficulty = next(1);
        } else if (arg == "--frames") {
            options.screenshotFrame = next(40);
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::printf("Unknown option: %s\n\n", arg.c_str());
            printUsage();
            return 1;
        }
    }

    gc::ui::App app;
    return app.run(options);
}
