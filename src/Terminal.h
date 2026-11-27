#pragma once

#include "Game.h"

#include <memory>

namespace pacman {

enum class Key { None, Up, Right, Down, Left, Start, Pause, Quit };

class TerminalSession {
public:
    TerminalSession();
    ~TerminalSession();

    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

    Key readKey();
    bool interactive() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

void render(const Game& game, bool useColor);

} // namespace pacman
