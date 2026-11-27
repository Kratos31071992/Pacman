#include "Game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pacman {
namespace {

constexpr std::array<Direction, 4> kDirections{
    Direction::Up, Direction::Left, Direction::Down, Direction::Right
};
constexpr std::array<int, 4> kReleaseTicks{0, 20, 45, 70};

Level classicLevel() {
    return Level{
        {
            "############################",
            "#............##............#",
            "#.####.#####.##.#####.####.#",
            "#o####.#####.##.#####.####o#",
            "#.####.#####.##.#####.####.#",
            "#..........................#",
            "#.####.##.########.##.####.#",
            "#.####.##.########.##.####.#",
            "#......##....##....##......#",
            "######.##### ## #####.######",
            "######.##### ## #####.######",
            "######.##          ##.######",
            "######.## ###  ### ##.######",
            "######.## #      # ##.######",
            "######.## #      # ##.######",
            "######.## #      # ##.######",
            "######.## ######## ##.######",
            "######.##          ##.######",
            "######.## ######## ##.######",
            "######.## ######## ##.######",
            "#............##............#",
            "#.####.#####.##.#####.####.#",
            "#.####.#####.##.#####.####.#",
            "#o..##.......  .......##..o#",
            "###.##.##.########.##.##.###",
            "###.##.##.########.##.##.###",
            "#......##....##....##......#",
            "#.##########.##.##########.#",
            "#.##########.##.##########.#",
            "#..........................#",
            "############################",
        },
        {23, 13},
        {{14, 12}, {14, 13}, {14, 14}, {14, 15}}
    };
}

} // namespace

Position operator+(Position lhs, Position rhs) {
    return {lhs.row + rhs.row, lhs.col + rhs.col};
}

Position operator-(Position lhs, Position rhs) {
    return {lhs.row - rhs.row, lhs.col - rhs.col};
}

Position directionDelta(Direction direction) {
    switch (direction) {
        case Direction::Up: return {-1, 0};
        case Direction::Right: return {0, 1};
        case Direction::Down: return {1, 0};
        case Direction::Left: return {0, -1};
        case Direction::None: return {0, 0};
    }
    return {0, 0};
}

Direction opposite(Direction direction) {
    switch (direction) {
        case Direction::Up: return Direction::Down;
        case Direction::Right: return Direction::Left;
        case Direction::Down: return Direction::Up;
        case Direction::Left: return Direction::Right;
        case Direction::None: return Direction::None;
    }
    return Direction::None;
}

int distanceSquared(Position lhs, Position rhs) {
    const int rowDiff = lhs.row - rhs.row;
    const int colDiff = lhs.col - rhs.col;
    return rowDiff * rowDiff + colDiff * colDiff;
}

Game::Game(Level level, std::uint32_t seed)
    : initialLevel_(std::move(level)), random_(seed) {
    if (initialLevel_.rows.empty() || initialLevel_.rows.front().empty()) {
        throw std::invalid_argument("Лабиринт не должен быть пустым");
    }
    const auto width = initialLevel_.rows.front().size();
    for (const auto& row : initialLevel_.rows) {
        if (row.size() != width) {
            throw std::invalid_argument("Все строки лабиринта должны иметь одинаковую длину");
        }
        const auto invalid = std::find_if_not(row.begin(), row.end(), [](char cell) {
            return cell == '#' || cell == '.' || cell == 'o' || cell == ' ';
        });
        if (invalid != row.end()) {
            throw std::invalid_argument("Лабиринт содержит неизвестный символ");
        }
    }
    const auto validSpawn = [this, width](Position spawn) {
        return spawn.row >= 0 && spawn.row < height() &&
               spawn.col >= 0 && spawn.col < static_cast<int>(width) &&
               initialLevel_.rows[static_cast<std::size_t>(spawn.row)]
                                 [static_cast<std::size_t>(spawn.col)] != '#';
    };
    if (!validSpawn(initialLevel_.pacmanSpawn)) {
        throw std::invalid_argument("Pacman должен появляться на доступной клетке");
    }
    for (const auto& spawn : initialLevel_.ghostSpawns) {
        if (!validSpawn(spawn)) {
            throw std::invalid_argument("Призрак должен появляться на доступной клетке");
        }
    }
    resetRun();
}

Game Game::classic(std::uint32_t seed) {
    return Game(classicLevel(), seed);
}

int Game::width() const {
    return static_cast<int>(initialLevel_.rows.front().size());
}

int Game::height() const {
    return static_cast<int>(initialLevel_.rows.size());
}

Tile Game::tileAt(Position position) const {
    position = wrapped(position);
    if (position.row < 0 || position.row >= height()) {
        return Tile::Wall;
    }
    return board_[static_cast<std::size_t>(position.row)]
                 [static_cast<std::size_t>(position.col)];
}

void Game::setHighScore(int value) {
    highScore_ = std::max({0, value, score_});
}

void Game::start() {
    if (status_ == Status::GameOver) {
        restart();
    } else if (status_ == Status::Title) {
        status_ = Status::Playing;
    }
}

void Game::restart() {
    resetRun();
    status_ = Status::Playing;
}

void Game::togglePause() {
    if (status_ == Status::Playing) {
        status_ = Status::Paused;
    } else if (status_ == Status::Paused) {
        status_ = Status::Playing;
    }
}

void Game::queueDirection(Direction direction) {
    if (direction != Direction::None) {
        queuedDirection_ = direction;
    }
}

void Game::tick() {
    if (status_ == Status::Dying) {
        if (--transitionTicks_ <= 0) {
            resetActors();
            status_ = Status::Playing;
        }
        return;
    }
    if (status_ == Status::LevelComplete) {
        if (--transitionTicks_ <= 0) {
            ++levelNumber_;
            resetBoard();
            resetActors();
            status_ = Status::Playing;
        }
        return;
    }
    if (status_ != Status::Playing) {
        return;
    }

    ++levelTicks_;
    if (frightenedTicks_ > 0 && --frightenedTicks_ == 0) {
        finishPowerMode();
    }

    const Position previousPacman = pacman_;
    movePacman();
    if (status_ != Status::Playing) {
        return;
    }

    // Сначала проверяем столкновение после движения игрока.
    for (auto& ghost : ghosts_) {
        checkPacmanCollision(previousPacman, ghost.position, ghost);
        if (status_ != Status::Playing) {
            return;
        }
    }

    // Призраки двигаются реже Pacman. В испуганном режиме они ещё медленнее.
    for (std::size_t index = 0; index < ghosts_.size(); ++index) {
        auto& ghost = ghosts_[index];
        if (levelTicks_ < ghost.releaseTick) {
            continue;
        }
        const int cadence = ghost.mode == GhostMode::Frightened ? 3 : 2;
        if (levelTicks_ % cadence != 0) {
            continue;
        }
        const Position previousGhost = ghost.position;
        moveGhost(ghost, index);
        if (ghost.mode == GhostMode::Eaten && ghost.position == ghost.home) {
            ghost.mode = GhostMode::Chase;
            ghost.direction = Direction::Up;
        }
        checkPacmanCollision(previousPacman, previousGhost, ghost);
        if (status_ != Status::Playing) {
            return;
        }
    }
}

void Game::resetRun() {
    score_ = 0;
    lives_ = 3;
    levelNumber_ = 1;
    nextExtraLife_ = 10000;
    resetBoard();
    resetActors();
    status_ = Status::Title;
}

void Game::resetBoard() {
    board_.clear();
    remainingPellets_ = 0;
    for (const auto& row : initialLevel_.rows) {
        std::vector<Tile> tiles;
        tiles.reserve(row.size());
        for (char cell : row) {
            switch (cell) {
                case '#': tiles.push_back(Tile::Wall); break;
                case '.':
                    tiles.push_back(Tile::Pellet);
                    ++remainingPellets_;
                    break;
                case 'o':
                    tiles.push_back(Tile::PowerPellet);
                    ++remainingPellets_;
                    break;
                case ' ': tiles.push_back(Tile::Empty); break;
                default: throw std::invalid_argument("Неизвестная клетка лабиринта");
            }
        }
        board_.push_back(std::move(tiles));
    }

    // Стартовые клетки всегда пустые, даже если автор уровня поставил там точку.
    const auto clearSpawn = [this](Position spawn) {
        auto& tile = board_[static_cast<std::size_t>(spawn.row)]
                           [static_cast<std::size_t>(spawn.col)];
        if (tile == Tile::Pellet || tile == Tile::PowerPellet) {
            tile = Tile::Empty;
            --remainingPellets_;
        }
    };
    clearSpawn(initialLevel_.pacmanSpawn);
    for (const auto& spawn : initialLevel_.ghostSpawns) {
        clearSpawn(spawn);
    }
}

void Game::resetActors() {
    pacman_ = initialLevel_.pacmanSpawn;
    pacmanDirection_ = Direction::None;
    queuedDirection_ = Direction::None;
    frightenedTicks_ = 0;
    frightenedCombo_ = 0;
    levelTicks_ = 0;
    transitionTicks_ = 0;
    ghosts_.clear();
    for (std::size_t index = 0; index < initialLevel_.ghostSpawns.size(); ++index) {
        const auto spawn = initialLevel_.ghostSpawns[index];
        const int baseRelease = kReleaseTicks[std::min(index, kReleaseTicks.size() - 1)];
        ghosts_.push_back(GhostState{
            spawn,
            spawn,
            Direction::None,
            GhostMode::Chase,
            std::max(0, baseRelease - (levelNumber_ - 1) * 3)
        });
    }
}

void Game::movePacman() {
    if (canMove(pacman_, queuedDirection_)) {
        pacmanDirection_ = queuedDirection_;
    }
    if (!canMove(pacman_, pacmanDirection_)) {
        return;
    }
    pacman_ = nextPosition(pacman_, pacmanDirection_);
    consumeTile();
}

void Game::consumeTile() {
    auto& tile = board_[static_cast<std::size_t>(pacman_.row)]
                       [static_cast<std::size_t>(pacman_.col)];
    if (tile == Tile::Pellet) {
        tile = Tile::Empty;
        --remainingPellets_;
        addScore(10);
    } else if (tile == Tile::PowerPellet) {
        tile = Tile::Empty;
        --remainingPellets_;
        addScore(50);
        frightenedTicks_ = std::max(20, 55 - (levelNumber_ - 1) * 5);
        frightenedCombo_ = 0;
        for (auto& ghost : ghosts_) {
            if (ghost.mode != GhostMode::Eaten) {
                ghost.mode = GhostMode::Frightened;
                ghost.direction = opposite(ghost.direction);
            }
        }
    } else {
        return;
    }

    if (remainingPellets_ == 0) {
        status_ = Status::LevelComplete;
        transitionTicks_ = 20;
    }
}

void Game::addScore(int points) {
    score_ += points;
    highScore_ = std::max(highScore_, score_);
    while (score_ >= nextExtraLife_) {
        lives_ = std::min(5, lives_ + 1);
        nextExtraLife_ += 10000;
    }
}

void Game::moveGhost(GhostState& ghost, std::size_t index) {
    std::vector<Direction> available;
    for (const auto direction : kDirections) {
        if (canMove(ghost.position, direction)) {
            available.push_back(direction);
        }
    }
    if (available.empty()) {
        ghost.direction = Direction::None;
        return;
    }

    if (available.size() > 1 && ghost.direction != Direction::None) {
        const auto reverse = opposite(ghost.direction);
        const auto withoutReverse = std::remove(available.begin(), available.end(), reverse);
        if (withoutReverse != available.begin()) {
            available.erase(withoutReverse, available.end());
        }
    }

    Direction chosen = available.front();
    if (ghost.mode == GhostMode::Frightened) {
        std::shuffle(available.begin(), available.end(), random_);
        chosen = available.front();
    } else {
        const Position target = targetForGhost(index);
        int bestDistance = std::numeric_limits<int>::max();
        for (const auto direction : available) {
            const Position candidate = nextPosition(ghost.position, direction);
            const int distance = distanceSquared(candidate, target);
            if (distance < bestDistance) {
                bestDistance = distance;
                chosen = direction;
            }
        }
    }

    ghost.direction = chosen;
    ghost.position = nextPosition(ghost.position, chosen);
}

Position Game::targetForGhost(std::size_t index) const {
    const auto& ghost = ghosts_[index];
    if (ghost.mode == GhostMode::Eaten) {
        return ghost.home;
    }

    // Первые семь секунд каждой волны призраки расходятся по углам.
    const bool scatter = (levelTicks_ / 70) % 2 == 0;
    if (scatter) {
        const std::array<Position, 4> corners{
            Position{0, width() - 1}, Position{0, 0},
            Position{height() - 1, width() - 1}, Position{height() - 1, 0}
        };
        return corners[std::min(index, corners.size() - 1)];
    }

    switch (index) {
        case 0: // Blinky преследует текущую клетку игрока.
            return pacman_;
        case 1: { // Pinky целится на четыре клетки впереди Pacman.
            const auto delta = directionDelta(pacmanDirection_);
            return pacman_ + Position{delta.row * 4, delta.col * 4};
        }
        case 2: { // Inky использует вектор от Blinky к Pacman.
            const auto vector = pacman_ - ghosts_[0].position;
            return pacman_ + vector;
        }
        case 3: // Clyde отступает в угол, если игрок близко.
            return distanceSquared(ghost.position, pacman_) > 64
                ? pacman_
                : Position{height() - 1, 0};
        default:
            return pacman_;
    }
}

void Game::checkPacmanCollision(const Position& previousPacman,
                                const Position& previousGhost,
                                GhostState& ghost) {
    if (levelTicks_ < ghost.releaseTick || ghost.mode == GhostMode::Eaten) {
        return;
    }
    const bool sameCell = ghost.position == pacman_;
    const bool crossed = ghost.position == previousPacman && previousGhost == pacman_;
    if (!sameCell && !crossed) {
        return;
    }

    if (ghost.mode == GhostMode::Frightened) {
        const int points = 200 * (1 << std::min(frightenedCombo_, 3));
        addScore(points);
        ++frightenedCombo_;
        ghost.mode = GhostMode::Eaten;
        ghost.direction = opposite(ghost.direction);
    } else {
        loseLife();
    }
}

void Game::loseLife() {
    --lives_;
    if (lives_ <= 0) {
        lives_ = 0;
        status_ = Status::GameOver;
    } else {
        status_ = Status::Dying;
        transitionTicks_ = 12;
    }
}

void Game::finishPowerMode() {
    frightenedCombo_ = 0;
    for (auto& ghost : ghosts_) {
        if (ghost.mode == GhostMode::Frightened) {
            ghost.mode = GhostMode::Chase;
        }
    }
}

Position Game::wrapped(Position position) const {
    if (position.col < 0) {
        position.col = width() - 1;
    } else if (position.col >= width()) {
        position.col = 0;
    }
    return position;
}

Position Game::nextPosition(Position position, Direction direction) const {
    return wrapped(position + directionDelta(direction));
}

bool Game::canMove(Position position, Direction direction) const {
    if (direction == Direction::None) {
        return false;
    }
    const Position candidate = nextPosition(position, direction);
    if (candidate.row < 0 || candidate.row >= height()) {
        return false;
    }
    return tileAt(candidate) != Tile::Wall;
}

} // namespace pacman
