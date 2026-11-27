#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace pacman {

struct Position {
    int row = 0;
    int col = 0;

    bool operator==(const Position&) const = default;
};

enum class Direction { None, Up, Right, Down, Left };
enum class Tile { Wall, Empty, Pellet, PowerPellet };
enum class Status { Title, Playing, Paused, Dying, LevelComplete, GameOver };
enum class GhostMode { Chase, Frightened, Eaten };

struct GhostState {
    Position position;
    Position home;
    Direction direction = Direction::None;
    GhostMode mode = GhostMode::Chase;
    int releaseTick = 0;
};

struct Level {
    std::vector<std::string> rows;
    Position pacmanSpawn;
    std::vector<Position> ghostSpawns;
};

class Game {
public:
    explicit Game(Level level, std::uint32_t seed = 1);

    static Game classic(std::uint32_t seed = 1);

    void start();
    void restart();
    void togglePause();
    void queueDirection(Direction direction);
    void tick();

    int width() const;
    int height() const;
    int score() const { return score_; }
    int highScore() const { return highScore_; }
    int lives() const { return lives_; }
    int levelNumber() const { return levelNumber_; }
    int remainingPellets() const { return remainingPellets_; }
    int frightenedTicks() const { return frightenedTicks_; }
    int levelTicks() const { return levelTicks_; }
    Status status() const { return status_; }
    Position pacmanPosition() const { return pacman_; }
    Direction pacmanDirection() const { return pacmanDirection_; }
    const std::vector<GhostState>& ghosts() const { return ghosts_; }
    Tile tileAt(Position position) const;

    void setHighScore(int value);

private:
    Level initialLevel_;
    std::vector<std::vector<Tile>> board_;
    std::vector<GhostState> ghosts_;
    Position pacman_;
    Direction pacmanDirection_ = Direction::None;
    Direction queuedDirection_ = Direction::None;
    Status status_ = Status::Title;
    std::mt19937 random_;

    int score_ = 0;
    int highScore_ = 0;
    int lives_ = 3;
    int levelNumber_ = 1;
    int remainingPellets_ = 0;
    int frightenedTicks_ = 0;
    int frightenedCombo_ = 0;
    int levelTicks_ = 0;
    int transitionTicks_ = 0;
    int nextExtraLife_ = 10000;

    void resetRun();
    void resetBoard();
    void resetActors();
    void movePacman();
    void moveGhost(GhostState& ghost, std::size_t index);
    void consumeTile();
    void addScore(int points);
    void checkPacmanCollision(const Position& previousPacman,
                              const Position& previousGhost,
                              GhostState& ghost);
    void loseLife();
    void finishPowerMode();
    Position targetForGhost(std::size_t index) const;
    Position wrapped(Position position) const;
    Position nextPosition(Position position, Direction direction) const;
    bool canMove(Position position, Direction direction) const;
};

Position operator+(Position lhs, Position rhs);
Position operator-(Position lhs, Position rhs);
Position directionDelta(Direction direction);
Direction opposite(Direction direction);
int distanceSquared(Position lhs, Position rhs);

} // namespace pacman
