#include "Game.h"
#include "Terminal.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>

namespace {

constexpr auto kHighScoreFile = ".pacman_highscore";

int loadHighScore() {
    std::ifstream input(kHighScoreFile);
    int value = 0;
    if (input >> value && value >= 0) {
        return value;
    }
    return 0;
}

bool saveHighScore(int value) {
    const std::string temporary = std::string(kHighScoreFile) + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!(output << value << '\n')) {
            return false;
        }
    }
    return std::rename(temporary.c_str(), kHighScoreFile) == 0;
}

pacman::Direction directionForKey(pacman::Key key) {
    using pacman::Direction;
    using pacman::Key;
    switch (key) {
        case Key::Up: return Direction::Up;
        case Key::Right: return Direction::Right;
        case Key::Down: return Direction::Down;
        case Key::Left: return Direction::Left;
        default: return Direction::None;
    }
}

} // namespace

int main() {
    using namespace pacman;

    TerminalSession terminal;
    if (!terminal.interactive()) {
        std::cerr << "Pacman нужно запустить в интерактивном терминале.\n";
        return 1;
    }

    const int savedHighScore = loadHighScore();
    Game game = Game::classic(std::random_device{}());
    game.setHighScore(savedHighScore);
    bool running = true;
    Status previousStatus = game.status();

    std::cout << "\x1b[2J\x1b[?25l" << std::flush;
    while (running) {
        Key key = terminal.readKey();
        const Direction direction = directionForKey(key);
        if (direction != Direction::None) {
            game.queueDirection(direction);
        }

        switch (key) {
            case Key::Start:
                if (game.status() == Status::Title || game.status() == Status::GameOver) {
                    game.start();
                }
                break;
            case Key::Pause:
                game.togglePause();
                break;
            case Key::Quit:
                running = false;
                break;
            default:
                break;
        }

        game.tick();
        render(game, true);

        if (game.status() == Status::GameOver && previousStatus != Status::GameOver) {
            saveHighScore(game.highScore());
        }
        previousStatus = game.status();
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
    }

    std::cout << "\x1b[0m\x1b[?25h\x1b[2J\x1b[H" << std::flush;
    if (!saveHighScore(game.highScore())) {
        std::cerr << "Не удалось сохранить рекорд в " << kHighScoreFile << '\n';
        return 1;
    }
    std::cout << "Игра завершена. Рекорд: " << game.highScore() << '\n';
    return 0;
}
