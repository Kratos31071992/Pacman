#include "Game.h"

#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << __func__ << ':' << __LINE__ \
                      << ": проверка не пройдена: " #condition "\n"; \
            ++failures; \
        } \
    } while (false)

using pacman::Direction;
using pacman::Game;
using pacman::GhostMode;
using pacman::Level;
using pacman::Position;
using pacman::Status;
using pacman::Tile;

Game movementGame() {
    return Game(Level{
        {"#######", "#.....#", "#.###.#", "#.....#", "#######"},
        {1, 1},
        {}
    });
}

void startAndTitle() {
    Game game = movementGame();
    CHECK(game.status() == Status::Title);
    CHECK(game.score() == 0);
    CHECK(game.lives() == 3);
    const auto before = game.pacmanPosition();
    game.tick();
    CHECK(game.pacmanPosition() == before);
    game.start();
    CHECK(game.status() == Status::Playing);
}

void movementAndPellets() {
    Game game = movementGame();
    game.start();
    game.queueDirection(Direction::Right);
    game.tick();
    CHECK(game.pacmanPosition() == Position(1, 2));
    CHECK(game.score() == 10);

    game.queueDirection(Direction::Up);
    const int score = game.score();
    // Сверху стена, но прежнее направление вправо остаётся доступным.
    game.tick();
    CHECK(game.pacmanPosition() == Position(1, 3));
    CHECK(game.score() > score);

    Game blocked = movementGame();
    blocked.start();
    blocked.queueDirection(Direction::Up);
    blocked.tick();
    CHECK(blocked.pacmanPosition() == Position(1, 1));
    CHECK(blocked.score() == 0);
}

void pauseFreezesWorld() {
    Game game = movementGame();
    game.start();
    game.queueDirection(Direction::Right);
    game.togglePause();
    const auto before = game.pacmanPosition();
    game.tick();
    CHECK(game.status() == Status::Paused);
    CHECK(game.pacmanPosition() == before);
    CHECK(game.score() == 0);
    game.togglePause();
    game.tick();
    CHECK(game.pacmanPosition() != before);
}

void powerPelletLetsPacmanEatGhost() {
    Game game(Level{
        {"#######", "#.o...#", "#.....#", "#######"},
        {1, 1},
        {{1, 3}}
    });
    game.start();
    game.queueDirection(Direction::Right);
    game.tick();
    CHECK(game.frightenedTicks() > 0);
    CHECK(game.ghosts().at(0).mode == GhostMode::Frightened);
    game.tick();
    CHECK(game.ghosts().at(0).mode == GhostMode::Eaten);
    CHECK(game.score() == 250); // 50 за энерджайзер и 200 за первого призрака.
    CHECK(game.lives() == 3);
}

void collisionConsumesLivesAndRestarts() {
    const Level level{{"#####", "#...#", "#####"}, {1, 1}, {{1, 1}}};
    Game game(level);
    game.start();
    game.tick();
    CHECK(game.status() == Status::Dying);
    CHECK(game.lives() == 2);
    for (int i = 0; i < 12; ++i) {
        game.tick();
    }
    CHECK(game.status() == Status::Playing);

    game.tick();
    CHECK(game.lives() == 1);
    for (int i = 0; i < 12; ++i) {
        game.tick();
    }
    game.tick();
    CHECK(game.status() == Status::GameOver);
    CHECK(game.lives() == 0);

    game.start();
    CHECK(game.status() == Status::Playing);
    CHECK(game.lives() == 3);
    CHECK(game.score() == 0);
}

void completingLevelAdvancesGame() {
    Game game(Level{{"#####", "#. .#", "#####"}, {1, 1}, {}});
    CHECK(game.remainingPellets() == 1);
    game.start();
    game.queueDirection(Direction::Right);
    game.tick();
    CHECK(game.status() == Status::Playing);
    game.tick();
    CHECK(game.remainingPellets() == 0);
    CHECK(game.status() == Status::LevelComplete);
    for (int i = 0; i < 20; ++i) {
        game.tick();
    }
    CHECK(game.status() == Status::Playing);
    CHECK(game.levelNumber() == 2);
    CHECK(game.remainingPellets() == 1);
    CHECK(game.pacmanPosition() == Position(1, 1));
}

void highScoreSurvivesRestart() {
    Game game = movementGame();
    game.setHighScore(5000);
    game.start();
    game.queueDirection(Direction::Right);
    game.tick();
    CHECK(game.highScore() == 5000);
    game.restart();
    CHECK(game.highScore() == 5000);
    CHECK(game.score() == 0);
}

void classicMapIsConnected() {
    const Game game = Game::classic();
    std::set<std::pair<int, int>> visited;
    std::queue<Position> queue;
    queue.push(game.pacmanPosition());
    visited.insert({game.pacmanPosition().row, game.pacmanPosition().col});
    constexpr Direction directions[]{
        Direction::Up, Direction::Right, Direction::Down, Direction::Left
    };

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();
        for (const auto direction : directions) {
            const auto delta = pacman::directionDelta(direction);
            const Position next{current.row + delta.row, current.col + delta.col};
            if (next.row < 0 || next.row >= game.height() ||
                next.col < 0 || next.col >= game.width() ||
                game.tileAt(next) == Tile::Wall) {
                continue;
            }
            if (visited.insert({next.row, next.col}).second) {
                queue.push(next);
            }
        }
    }

    int unreachablePellets = 0;
    for (int row = 0; row < game.height(); ++row) {
        for (int col = 0; col < game.width(); ++col) {
            const Position position{row, col};
            const auto tile = game.tileAt(position);
            if ((tile == Tile::Pellet || tile == Tile::PowerPellet) &&
                !visited.contains({row, col})) {
                ++unreachablePellets;
            }
        }
    }
    CHECK(unreachablePellets == 0);
    CHECK(game.remainingPellets() > 100);
    CHECK(game.ghosts().size() == 4);
    for (const auto& ghost : game.ghosts()) {
        CHECK(visited.contains({ghost.position.row, ghost.position.col}));
    }
}

void invalidMapIsRejected() {
    bool thrown = false;
    try {
        Game invalid(Level{{"###", "##"}, {0, 0}, {}});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    CHECK(thrown);

    thrown = false;
    try {
        Game wallSpawn(Level{{"###", "#.#", "###"}, {0, 0}, {}});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    CHECK(thrown);
}

} // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"startAndTitle", startAndTitle},
        {"movementAndPellets", movementAndPellets},
        {"pauseFreezesWorld", pauseFreezesWorld},
        {"powerPelletLetsPacmanEatGhost", powerPelletLetsPacmanEatGhost},
        {"collisionConsumesLivesAndRestarts", collisionConsumesLivesAndRestarts},
        {"completingLevelAdvancesGame", completingLevelAdvancesGame},
        {"highScoreSurvivesRestart", highScoreSurvivesRestart},
        {"classicMapIsConnected", classicMapIsConnected},
        {"invalidMapIsRejected", invalidMapIsRejected},
    };

    for (const auto& [name, test] : tests) {
        const int before = failures;
        test();
        std::cout << (failures == before ? "[OK] " : "[FAIL] ") << name << '\n';
    }
    if (failures != 0) {
        std::cerr << "Ошибок: " << failures << '\n';
        return 1;
    }
    std::cout << "Все тесты пройдены: " << tests.size() << '\n';
    return 0;
}
