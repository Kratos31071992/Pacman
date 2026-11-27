#include "Terminal.h"

#include <array>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <cerrno>
#include <termios.h>
#include <unistd.h>
#endif

namespace pacman {

struct TerminalSession::Impl {
#ifdef _WIN32
    bool active = false;

    Impl() {
        active = _isatty(_fileno(stdin)) != 0;
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode)) {
            SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#else
    termios original{};
    bool active = false;

    Impl() {
        if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &original) != 0) {
            return;
        }
        termios raw = original;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        active = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0;
    }

    ~Impl() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        }
    }

    int readByte() {
        unsigned char value = 0;
        const ssize_t count = read(STDIN_FILENO, &value, 1);
        if (count == 1) {
            return value;
        }
        if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            active = false;
        }
        return -1;
    }
#endif
};

TerminalSession::TerminalSession() : impl_(std::make_unique<Impl>()) {}
TerminalSession::~TerminalSession() = default;

bool TerminalSession::interactive() const {
    return impl_->active;
}

Key TerminalSession::readKey() {
#ifdef _WIN32
    if (!_kbhit()) {
        return Key::None;
    }
    int value = _getch();
    if (value == 0 || value == 224) {
        switch (_getch()) {
            case 72: return Key::Up;
            case 77: return Key::Right;
            case 80: return Key::Down;
            case 75: return Key::Left;
            default: return Key::None;
        }
    }
#else
    int value = impl_->readByte();
    if (value == 27) {
        if (impl_->readByte() != '[') {
            return Key::None;
        }
        switch (impl_->readByte()) {
            case 'A': return Key::Up;
            case 'C': return Key::Right;
            case 'B': return Key::Down;
            case 'D': return Key::Left;
            default: return Key::None;
        }
    }
#endif

    switch (value) {
        case 'w': case 'W': return Key::Up;
        case 'd': case 'D': return Key::Right;
        case 's': case 'S': return Key::Down;
        case 'a': case 'A': return Key::Left;
        case 'p': case 'P': return Key::Pause;
        case 'q': case 'Q': return Key::Quit;
        case '\r': case '\n': case ' ': return Key::Start;
        default: return Key::None;
    }
}

namespace {

const char* color(bool enabled, const char* code) {
    return enabled ? code : "";
}

} // namespace

void render(const Game& game, bool useColor) {
    std::ostringstream frame;
    frame << "\x1b[H";
    frame << color(useColor, "\x1b[1;33m") << "  PACMAN :: TERMINAL EDITION"
          << color(useColor, "\x1b[0m") << "\n";
    frame << "  SCORE " << game.score()
          << "   HIGH " << game.highScore()
          << "   LIVES " << game.lives()
          << "   LEVEL " << game.levelNumber() << "\n\n";

    for (int row = 0; row < game.height(); ++row) {
        frame << "  ";
        for (int col = 0; col < game.width(); ++col) {
            const Position position{row, col};
            bool drawn = false;

            for (std::size_t index = 0; index < game.ghosts().size(); ++index) {
                const auto& ghost = game.ghosts()[index];
                if (ghost.position != position) {
                    continue;
                }
                if (ghost.mode == GhostMode::Frightened) {
                    frame << color(useColor, "\x1b[1;36m") << 'g';
                } else if (ghost.mode == GhostMode::Eaten) {
                    frame << color(useColor, "\x1b[37m") << 'e';
                } else {
                    constexpr std::array<const char*, 4> ghostColors{
                        "\x1b[1;31m", "\x1b[1;35m", "\x1b[1;34m", "\x1b[1;33m"
                    };
                    constexpr std::array<char, 4> ghostLetters{'B', 'P', 'I', 'C'};
                    frame << color(useColor, ghostColors[std::min(index, ghostColors.size() - 1)])
                          << ghostLetters[std::min(index, ghostLetters.size() - 1)];
                }
                frame << color(useColor, "\x1b[0m");
                drawn = true;
                break;
            }
            if (drawn) {
                continue;
            }

            if (game.pacmanPosition() == position) {
                frame << color(useColor, "\x1b[1;33m")
                      << (game.status() == Status::Dying ? 'x' : 'C')
                      << color(useColor, "\x1b[0m");
                continue;
            }

            switch (game.tileAt(position)) {
                case Tile::Wall:
                    frame << color(useColor, "\x1b[34m") << '#'
                          << color(useColor, "\x1b[0m");
                    break;
                case Tile::Pellet:
                    frame << color(useColor, "\x1b[37m") << '.'
                          << color(useColor, "\x1b[0m");
                    break;
                case Tile::PowerPellet:
                    frame << color(useColor, "\x1b[1;35m") << 'o'
                          << color(useColor, "\x1b[0m");
                    break;
                case Tile::Empty:
                    frame << ' ';
                    break;
            }
        }
        frame << '\n';
    }

    frame << "\n  WASD / стрелки — движение   P — пауза   Q — выход\n";
    switch (game.status()) {
        case Status::Title:
            frame << color(useColor, "\x1b[1;33m")
                  << "  ENTER или ПРОБЕЛ — начать игру\n";
            break;
        case Status::Paused:
            frame << color(useColor, "\x1b[1;36m")
                  << "  ПАУЗА. Нажми P для продолжения.\n";
            break;
        case Status::Dying:
            frame << color(useColor, "\x1b[1;31m")
                  << "  Призрак поймал Pacman! Осталось жизней: " << game.lives() << "\n";
            break;
        case Status::LevelComplete:
            frame << color(useColor, "\x1b[1;32m")
                  << "  Уровень пройден! Следующий лабиринт загружается...\n";
            break;
        case Status::GameOver:
            frame << color(useColor, "\x1b[1;31m")
                  << "  ИГРА ОКОНЧЕНА. ENTER — заново, Q — выйти.\n";
            break;
        case Status::Playing:
            if (game.frightenedTicks() > 0) {
                frame << color(useColor, "\x1b[1;36m")
                      << "  Энерджайзер: " << game.frightenedTicks() / 10 + 1
                      << " сек. Лови призраков!\n";
            } else {
                frame << "  Собери все точки и избегай призраков.\n";
            }
            break;
    }
    frame << color(useColor, "\x1b[0m") << std::flush;
    std::cout << frame.str() << std::flush;
}

} // namespace pacman
