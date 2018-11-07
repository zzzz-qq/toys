#include <array>
#include <list>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <exception>
#include <random>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <SDL2/SDL.h>

using namespace std;

#define BACKGROUND_COLOR 0x21, 0x21, 0x21, 0xFF
constexpr int FPS = 60;
const int MILLISECONDS_PER_FRAME = round(1000. / FPS);
constexpr int CELL_LEN = 40;
constexpr int CELL_COLUMNS = 10;
constexpr int CELL_ROWS = 22;
constexpr int VISABLE_ROWS = 20;
constexpr int HIDDEN_ROWS = CELL_ROWS - VISABLE_ROWS;
constexpr int CELL_MARGIN = 6;
constexpr int CELL_DRAWN_LEN = CELL_LEN - CELL_MARGIN * 2;
constexpr int NEXT_PIECES_COUNT = 3;
constexpr SDL_Rect HOLD_BOARD { 0, 0, 6*CELL_LEN, 4*CELL_LEN };
constexpr SDL_Rect PLAYFIELD { HOLD_BOARD.x + HOLD_BOARD.w + CELL_LEN, 0, CELL_COLUMNS * CELL_LEN, VISABLE_ROWS * CELL_LEN };
constexpr SDL_Rect NEXT_BOARD { PLAYFIELD.x + PLAYFIELD.w + CELL_LEN, 0, 6*CELL_LEN, (3*NEXT_PIECES_COUNT + 1) * CELL_LEN, };
constexpr int SCREEN_WIDTH = HOLD_BOARD.w + CELL_LEN + PLAYFIELD.w + CELL_LEN + NEXT_BOARD.w;
constexpr int SCREEN_HEIGHT = VISABLE_ROWS * CELL_LEN;

struct SDLError : public exception
{
    explicit SDLError(string message) : message(move(message)) { }
    const char* what() const noexcept override { return message.c_str(); }
    string message;
};

#define REQUIRES_ZERO(value) \
do { \
    if ((value) != 0) \
    { \
        ostringstream ss; \
        ss << __FILE__ << ":" << __LINE__ << ": " << SDL_GetError(); \
        throw SDLError(ss.str()); \
    } \
} while (0)

#define REQUIRES_NOT_NULL(value) \
do { \
    if (!(value)) \
    { \
        ostringstream ss; \
        ss << __FILE__ << ":" << __LINE__ << ": " << SDL_GetError(); \
        throw SDLError(ss.str()); \
    } \
} while (0)

#define DEFINE_SINGLETON(type) \
static type& instance() \
{ \
    static type type_Singleton; \
    return type_Singleton; \
} \

using RendererPtr = unique_ptr<SDL_Renderer, void(*)(SDL_Renderer*)>;
using WindowPtr = unique_ptr<SDL_Window, void(*)(SDL_Window*)>;
using SurfacePtr = unique_ptr<SDL_Surface, void(*)(SDL_Surface*)>;
using TexturePtr = unique_ptr<SDL_Texture, void(*)(SDL_Texture*)>;

struct Object { virtual ~Object() { } };
struct Cell { int column; int row; };
using Cells = array<Cell, 4>;

class ITetromino : public Object
{
public:
    static constexpr Uint32 LOCK_DELAY_MILLISECONDS = 500;
    static constexpr int STATES_COUNT = 4;

    enum State { Up, Right, Down, Left };
    using Shape = Uint16;
    using Shapes = array<Shape, STATES_COUNT>;
    struct HardDropResult { int cleard; int dropped; };

    virtual SDL_Color color() const = 0;
    virtual int heightOf(State) const = 0;
    virtual int widthOf(State) const = 0;
    virtual Shape shapeOf(State) const = 0;
    virtual void tryRotate() = 0;

    void init();
    void spawn();
    void moveLeft();
    void moveRight();
    int softDrop(int rows = 1);
    HardDropResult hardDrop();

    void draw(int x, int y, State state) const;
    void draw() const { draw(PLAYFIELD.x + mLeft * CELL_LEN, PLAYFIELD.y + mBottom * CELL_LEN, mState); }

    Cells split(int left, int bottom, State state) const;
    Cells split(int left, int bottom) const { return split(left, bottom, mState); }
    Cells split() const { return split(mLeft, mBottom, mState); }

    int width() const { return widthOf(mState); }
    int height() const { return heightOf(mState); }
    bool visiable() const { return mBottom > HIDDEN_ROWS; }
    bool locking() const { return mLocking; }
    Uint32 lockTicks() const { return mLockTicks; }

protected:
    void tryRotate(const vector<Cell>& offsets, const vector<vector<Cell>>& attempts);
    void lock(Uint32 ticksNow);
    void unlock(Uint32 ticksNow);

private:
    int mLeft = 0;
    int mBottom = 0;
    State mState = State::Up;
    Uint32 mLockTicks = 0;
    bool mLocking = false;
};

struct ITetromino3x3 : public ITetromino
{
    int heightOf(State state) const override { return "\2\3\2\3"[state]; }
    int widthOf(State state) const override { return "\3\2\3\2"[state]; }
    void tryRotate() override;
};

struct I_Tetromino final : public ITetromino
{
    SDL_Color color() const override { return { 0x00, 0xE6, 0xE6, 0xAA }; }
    int heightOf(State state) const override { return "\1\4\1\4"[state]; }
    int widthOf(State state) const override { return "\4\1\4\1"[state]; }
    Shape shapeOf(State state) const override { return Shapes{ 0x000F, 0x8888, 0x000F, 0x8888 }[state]; }
    void tryRotate() override;
};

struct O_Tetromino final : public ITetromino
{
    SDL_Color color() const override { return { 0xE6, 0xE6, 0x00, 0xAA }; }
    int widthOf(State) const override { return 2; }
    int heightOf(State) const override { return 2; }
    Shape shapeOf(State) const override { return 0x00CC; }
    void tryRotate() override { }
};

struct T_Tetromino final : public ITetromino3x3
{
    SDL_Color color() const override { return { 0xE6, 0x00, 0xE6, 0xAA }; }
    Shape shapeOf(State state) const override { return Shapes{ 0x004E, 0x08C8, 0x00E4, 0x04C4 }[state]; }
};

struct J_Tetromino final : public ITetromino3x3
{
    SDL_Color color() const override { return { 0x00, 0x72, 0xFB, 0xAA }; }
    Shape shapeOf(State state) const override { return Shapes{ 0x008E, 0x0C88, 0x00E2, 0x044C }[state]; }
};

struct L_Tetromino final : public ITetromino3x3
{
    SDL_Color color() const override { return { 0xE6, 0x95, 0x00, 0xAA }; }
    Shape shapeOf(State state) const override { return Shapes{ 0x002E, 0x088C, 0x00E8, 0x0C44 }[state]; }
};

struct S_Tetromino final : public ITetromino3x3
{
    SDL_Color color() const override { return { 0x00, 0xE6, 0x00, 0xAA }; }
    Shape shapeOf(State state) const override { return Shapes{ 0x006C, 0x08C4, 0x006C, 0x08C4 }[state]; }
};

struct Z_Tetromino final : public ITetromino3x3
{
    SDL_Color color() const override { return { 0xE6, 0x00, 0x00, 0xAA }; }
    Shape shapeOf(State state) const override { return Shapes{ 0x00C6, 0x04C8, 0x00C6, 0x04C8 }[state]; }
};

class TetrominoController
{
public:
    DEFINE_SINGLETON(TetrominoController)

    void reset();
    void onKeyDown(const SDL_Event&);
    void update();

    void draw() const;

private:
    TetrominoController();

    shared_ptr<ITetromino> make();
    shared_ptr<ITetromino> next();
    void hold();
    void land();

    shared_ptr<ITetromino> mActive;
    shared_ptr<ITetromino> mHeld;
    list<shared_ptr<ITetromino>> mNextPieces;
    array<function<shared_ptr<ITetromino> ()>, 7> mTetrominoCreators;
    size_t mIndex = 7;
    Uint32 mUpdateTicks = 0;
    bool mHasHeld = false;
};

class Playfield final
{
public:
    struct Block { SDL_Color color; bool filled; };
    using Row = array<Block, CELL_COLUMNS>;

    DEFINE_SINGLETON(Playfield)

    void reset();
    int onLanding(const Cells&, SDL_Color);
    void draw() const;
    Cells getLandingSpot(const Cells&) const;
    bool isFilled(const Cells&) const;

private:
    Playfield() { reset(); }
    bool isFilled(int column, int row) const;

    vector<Row> mPlayfield;
};

class ScoreBoard final
{
public:
    DEFINE_SINGLETON(ScoreBoard)

    void reset();
    void onClear(int rows);
    void onSoftDrop(int rows);
    void onHardDrop(int rows);
    void updateTitle();

    string title() const;
    int speed() const { return mTicksPerRow; }

private:
    ScoreBoard() { reset(); }
    void tryLevelUp();

    int mTicksPerRow;
    int mCurrLevel;
    int mCurrCleardRows;
    int mTotalCleardRows;
    int mScores;
};

class Timer final
{
public:
    DEFINE_SINGLETON(Timer)

    void tick(Uint32 cappingTicks);
    void pause();
    void resume();

    Uint32 frameTicks() const { return mFrameTicks; }
    Uint32 getTicks() const { return (mHasPaused ?  mPauseStart : SDL_GetTicks()) - mPausedTicks; }

private:
    Timer() { mMark = mLastTicks = SDL_GetTicks(); }

    Uint32 mMark;
    Uint32 mLastTicks;
    Uint32 mFrameTicks = 0;
    Uint32 mPauseStart = 0;
    Uint32 mPausedTicks = 0;
    bool mHasPaused = false;
};

struct GameState : public Object
{
    enum class ID { None, Playing, Paused, GameOver, BeforeExit, };

    virtual ID id() const = 0;
    virtual void handleEvent(const SDL_Event&) = 0;
    virtual void update() { }
    virtual void draw() { }
    virtual void onEnter() { };
    virtual void onExit(ID) { };
};

struct PlayState final : public GameState
{
    ID id() const override { return ID::Playing; }
    void handleEvent(const SDL_Event&) override;
    void update() override;
    void draw() override;
    void onEnter() override;
};

struct PauseState final : public GameState
{
    ID id() const override { return ID::Paused; }
    void handleEvent(const SDL_Event&) override;
    void onEnter() override;
    void onExit(ID nextStateID) override;
};

struct GameOver final : public GameState
{
    ID id() const override { return ID::GameOver; }

    void handleEvent(const SDL_Event&) override;
    void draw() override;
    void onEnter() override;
    void onExit(ID nextStateID) override;
};

struct BeforeExit final : public GameState
{
    ID id() const override { return ID::BeforeExit; }

    void handleEvent(const SDL_Event&) override;
    void onEnter() override;
};

class GameStateManager final
{
public:
    DEFINE_SINGLETON(GameStateManager)

    void changeState(const shared_ptr<GameState>&);
    void goBack();
    void handleEvents();
    void update() { mCurrState->update(); }
    void draw() { mCurrState->draw(); }

    GameState::ID lastStateID() const { return mLastState ? mLastState->id() : GameState::ID::None; }

private:
    GameStateManager() = default;

    shared_ptr<GameState> mLastState;
    shared_ptr<GameState> mCurrState;
};

class Game final
{
public:
    DEFINE_SINGLETON(Game)

    void draw();
    void reset();
    SDL_Renderer* renderer() { return mRenderer.get(); }
    SDL_Window* window() { return mWindow.get(); }

private:
    Game();

    WindowPtr mWindow { nullptr, SDL_DestroyWindow };
    RendererPtr mRenderer { nullptr, SDL_DestroyRenderer };
    TexturePtr mBackground { nullptr, SDL_DestroyTexture };
};

inline void fillCell(int x, int y, SDL_Color color)
{
    REQUIRES_ZERO(SDL_SetRenderDrawColor(
        Game::instance().renderer(), color.r, color.g, color.b, color.a));

    SDL_Rect rect {
        x + CELL_MARGIN,
        y + CELL_MARGIN - HIDDEN_ROWS * CELL_LEN,
        CELL_DRAWN_LEN, CELL_DRAWN_LEN
    };

    REQUIRES_ZERO(SDL_RenderFillRect(Game::instance().renderer(), &rect));
}

inline void drawCell(int x, int y, SDL_Color color)
{
    REQUIRES_ZERO(SDL_SetRenderDrawColor(
        Game::instance().renderer(), color.r, color.g, color.b, color.a));

    SDL_Rect rect {
        x + CELL_MARGIN,
        y + CELL_MARGIN - HIDDEN_ROWS * CELL_LEN,
        CELL_DRAWN_LEN, CELL_DRAWN_LEN };

    REQUIRES_ZERO(SDL_RenderDrawRect(Game::instance().renderer(), &rect));
}

void ITetromino::init()
{
    mState = State::Up;
    mLocking = false;
    mLockTicks = 0;
    mLeft = (CELL_COLUMNS - width()) / 2;
    mBottom = height();
}

void ITetromino::spawn()
{
    init();

    for (int bottom = HIDDEN_ROWS + mBottom; bottom >= mBottom; --bottom)
    {
        if (!Playfield::instance().isFilled(split(mLeft, bottom)))
        {
            mBottom = bottom;
            break;
        }
    }

    if (Playfield::instance().isFilled(split(mLeft, mBottom + 1)))
    {
        lock(Timer::instance().getTicks());
    }
}

void ITetromino::moveLeft()
{
    if (!Playfield::instance().isFilled(split(mLeft - 1, mBottom)))
    {
        --mLeft;
        unlock(Timer::instance().getTicks());
    }
}

void ITetromino::moveRight()
{
    if (!Playfield::instance().isFilled(split(mLeft + 1, mBottom)))
    {
        ++mLeft;
        unlock(Timer::instance().getTicks());
    }
}

int ITetromino::softDrop(int rows)
{
    if (locking())
        return 0;

    int height = [this] {
        auto cells = split();
        auto landingSpot = Playfield::instance().getLandingSpot(cells);
        return landingSpot.at(0).row - cells.at(0).row;
    }();

    if (height <= rows)
    {
        mBottom += height;
        lock(Timer::instance().getTicks());
        return height;
    }

    mBottom += rows;
    return rows;
}

ITetromino::HardDropResult ITetromino::hardDrop()
{
    HardDropResult r;
    r.dropped = softDrop(CELL_ROWS);
    r.cleard = Playfield::instance().onLanding(split(), color());
    return r;
}

Cells ITetromino::split(int left, int bottom, State state) const
{
    Cells cells;
    auto shape = shapeOf(state);
    for (int i = 0, j = 0; i != 16; ++i)
    {
        if (shape & (0x8000 >> i))
        {
            cells[j].column = left + i % STATES_COUNT;
            cells[j++].row = bottom - STATES_COUNT + i / STATES_COUNT;
        }
    }
    return cells;
}

void ITetromino::draw(int x, int y, State state) const
{
    auto color_ = locking() ? SDL_Color{ 0x55, 0x55, 0x55, 0xFF } : color();
    for (const auto cell : split(0, 0, state))
    {
        fillCell(x + cell.column * CELL_LEN, y + cell.row * CELL_LEN, color_);
    }
}

void ITetromino::tryRotate(const vector<Cell>& offsets, const vector<vector<Cell>>& attempts)
{
    auto nextState = static_cast<State>((mState + 1) % STATES_COUNT);
    auto leftBase = mLeft + offsets.at(nextState).column;
    auto bottomBase = mBottom + offsets.at(nextState).row;

    for (const auto attempt : attempts.at(nextState))
    {
        auto left = leftBase + attempt.column;
        auto bottom = bottomBase + attempt.row;
        if (!Playfield::instance().isFilled(split(left, bottom, nextState)))
        {
            mLeft = left;
            mBottom = bottom;
            mState = nextState;
            unlock(Timer::instance().getTicks());
            return;
        }
    }
}

void ITetromino::lock(Uint32 ticksNow)
{
    mLocking = true;
    mLockTicks = ticksNow;
}

void ITetromino::unlock(Uint32 ticksNow)
{
    if (!Playfield::instance().isFilled(split(mLeft, mBottom + 1)))
    {
        mLocking = false;
    }
    mLockTicks = ticksNow;
}

void ITetromino3x3::tryRotate()
{
    static const vector<Cell> offsets { {0, -1}, {1, 1}, {-1, 0}, {0, 0} };
    static const vector<vector<Cell>> attempts {
        { {0, 0}, {-1, 0}, {-1,-1}, {0, 2}, {-1, 2}, },
        { {0, 0}, {-1, 0}, {-1, 1}, {0,-2}, {-1,-2}, },
        { {0, 0}, {1, 0}, {1,-1}, {0, 2}, {1, 2}, },
        { {0, 0}, {1, 0}, {1, 1}, {0,-2}, {1,-2}, },
    };
    ITetromino::tryRotate(offsets, attempts);
}

void I_Tetromino::tryRotate()
{
    static const vector<Cell> offsets { {-1, -2}, {2, 2}, {-2, -1}, {1, 1}, };
    static const vector<vector<Cell>> attempts {
        { {0, 0}, {1, 0}, {-2, 0}, {1,-2}, {-2, 1}, },
        { {0, 0}, {-2, 0}, {1, 0}, {-2,-1}, {1, 2}, },
        { {0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2,-1}, },
        { {0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1,-2}, },
    };
    ITetromino::tryRotate(offsets, attempts);
}

TetrominoController::TetrominoController()
{
    srand(time(nullptr));

    mTetrominoCreators = {
        [] { return make_shared<I_Tetromino>(); }, [] { return make_shared<O_Tetromino>(); },
        [] { return make_shared<T_Tetromino>(); }, [] { return make_shared<J_Tetromino>(); },
        [] { return make_shared<L_Tetromino>(); }, [] { return make_shared<S_Tetromino>(); },
        [] { return make_shared<Z_Tetromino>(); },
    };

    reset();
}

void TetrominoController::reset()
{
    mIndex = mTetrominoCreators.size();

    mActive = make();
    mActive->spawn();
    mHeld.reset();

    mNextPieces.clear();
    for (int i = 0; i != NEXT_PIECES_COUNT; ++i)
        mNextPieces.emplace_back(make());

    mUpdateTicks = 0;
    mHasHeld = false;
}

void TetrominoController::onKeyDown(const SDL_Event& e)
{
    switch (e.key.keysym.sym)
    {
    case SDLK_UP: if (!e.key.repeat) mActive->tryRotate(); break;
    case SDLK_c: if (!e.key.repeat) hold(); break;
    case SDLK_DOWN: ScoreBoard::instance().onSoftDrop(mActive->softDrop()); break;
    case SDLK_LEFT: mActive->moveLeft(); break;
    case SDLK_RIGHT: mActive->moveRight(); break;
    case SDLK_SPACE: land(); break;
    default: break;
    }
}

void TetrominoController::update()
{
    if (mActive->locking())
    {
        auto lockingTicks = Timer::instance().getTicks() - mActive->lockTicks();
        if (lockingTicks >= ITetromino::LOCK_DELAY_MILLISECONDS)
        {
            land();
            mUpdateTicks = 0;
            return;
        }
    }

    mUpdateTicks += Timer::instance().frameTicks();
    auto rows = mUpdateTicks / ScoreBoard::instance().speed();
    if (rows > 0)
    {
        mUpdateTicks %= ScoreBoard::instance().speed();
        mActive->softDrop(rows);
    }
}

void TetrominoController::draw() const
{
    if (mActive->visiable())
    {
        for (const auto cell : Playfield::instance().getLandingSpot(mActive->split()))
            drawCell(
                PLAYFIELD.x + cell.column*CELL_LEN,
                PLAYFIELD.y + cell.row*CELL_LEN,
                mActive->color());
    }
    mActive->draw();

    int i = 0;
    for (const auto& n: mNextPieces)
    {
        n->draw(
            NEXT_BOARD.x + (NEXT_BOARD.w - n->widthOf(ITetromino::State::Up) * CELL_LEN) / 2,
            NEXT_BOARD.y + 3*CELL_LEN * (i+1) + HIDDEN_ROWS * CELL_LEN,
            ITetromino::State::Up);
        ++i;
    }

    if (mHeld)
    {
        mHeld->draw(
            HOLD_BOARD.x + (HOLD_BOARD.w - mHeld->widthOf(ITetromino::State::Up) * CELL_LEN) / 2,
            HOLD_BOARD.y + 3*CELL_LEN + HIDDEN_ROWS * CELL_LEN,
            ITetromino::State::Up);
    }
}

shared_ptr<ITetromino> TetrominoController::make()
{
    if (mIndex >= mTetrominoCreators.size())
    {
        random_shuffle(mTetrominoCreators.begin(), mTetrominoCreators.end());
        mIndex = 0;
    }
    return mTetrominoCreators[mIndex++]();
}

shared_ptr<ITetromino> TetrominoController::next()
{
    auto next = mNextPieces.front();
    next->spawn();
    mNextPieces.pop_front();
    mNextPieces.push_back(make());
    mHasHeld = false;
    return next;
}

void TetrominoController::hold()
{
    if (!mHasHeld)
    {
        mActive.swap(mHeld);
        if (!mActive)
            mActive = next();
        mActive->spawn();
        mHeld->init();
        mHasHeld = true;
    }
}

void TetrominoController::land()
{
    auto r = mActive->hardDrop();
    if (!mActive->visiable())
    {
        GameStateManager::instance().changeState(make_shared<GameOver>());
        return;
    }

    ScoreBoard::instance().onClear(r.cleard);
    ScoreBoard::instance().onHardDrop(r.dropped);

    mActive = next();
    if (Playfield::instance().isFilled(mActive->split()))
    {
        GameStateManager::instance().changeState(make_shared<GameOver>());
    }
}

void Playfield::reset()
{
    mPlayfield.resize(CELL_ROWS);

    for (auto& row : mPlayfield)
    {
        for (auto& block : row)
        {
            block.filled = false;
        }
    }
}

int Playfield::onLanding(const Cells& cells, SDL_Color color)
{
    for (const auto cell: cells)
    {
        mPlayfield.at(cell.row).at(cell.column).filled = true;
        mPlayfield[cell.row][cell.column].color = color;
    }

    int cleared = 0;
    for (auto row = mPlayfield.begin(); row != mPlayfield.end();)
    {
        if (all_of(row->cbegin(), row->cend(), [] (const auto& block) { return block.filled; }))
        {
            ++cleared;
            row = mPlayfield.erase(row);
            continue;
        }
        ++row;
    }

    for (int i = 0; i != cleared; ++i)
        mPlayfield.emplace(mPlayfield.begin(), Row());

    return cleared;
}

Cells Playfield::getLandingSpot(const Cells& cells) const
{
    for (auto landingSpot = cells;;)
    {
        for (const auto cell : landingSpot)
        {
            if (isFilled(cell.column, cell.row + 1))
                return landingSpot;
        }

        for (auto& cell : landingSpot)
            ++cell.row;
    }
}

bool Playfield::isFilled(const Cells& cells) const
{
    return any_of(
        cells.cbegin(), cells.cend(),
        [this] (Cell cell) { return isFilled(cell.column, cell.row); });
}

void Playfield::draw() const
{
    for (size_t r = 0; r != mPlayfield.size(); ++r)
    {
        for (size_t c = 0; c != mPlayfield[r].size(); ++c)
        {
            if (mPlayfield[r][c].filled)
                fillCell(
                    PLAYFIELD.x + c*CELL_LEN, PLAYFIELD.y + r*CELL_LEN,
                    mPlayfield[r][c].color);
        }
    }
}

bool Playfield::isFilled(int column, int row) const
{
    return row < 0 || row >= CELL_ROWS
        || column < 0 || column >= CELL_COLUMNS
        || mPlayfield[row][column].filled;
}


void ScoreBoard::reset()
{
    mTicksPerRow = 1000;
    mCurrLevel = 1;
    mCurrCleardRows = 0;
    mTotalCleardRows = 0;
    mScores = 0;
}

void ScoreBoard::onClear(int rows)
{
    if (rows > 0)
    {
        mScores += array<int, 4>{ 100, 300, 500, 800 }.at(rows - 1) * mCurrLevel;
        mTotalCleardRows += rows;
        mCurrCleardRows += rows;
        tryLevelUp();
        updateTitle();
    }
}

void ScoreBoard::onSoftDrop(int rows)
{
    if (rows > 0)
    {
        mScores += min(rows, 20);
        updateTitle();
    }
}

void ScoreBoard::onHardDrop(int rows)
{
    if (rows > 0)
    {
        mScores += min(rows * 2, 40);
        updateTitle();
    }
}

string ScoreBoard::title() const
{
    ostringstream ss;
    ss << "Level: " << mCurrLevel << " "
       << "Lines: " << mTotalCleardRows << " "
       << "Scores: " << mScores;
    return ss.str();
}

void ScoreBoard::updateTitle()
{
    SDL_SetWindowTitle(Game::instance().window(), title().c_str());
}

void ScoreBoard::tryLevelUp()
{
    static array<Uint32, 15> speeds {
        1000, 793, 618, 473, 355,
        262, 190, 135, 94, 64,
        43, 28, 18, 11, 7
    };

    if (mCurrLevel >= 15)
        return;

    int requiredRows = mCurrLevel * 5;
    if (mCurrCleardRows < requiredRows)
        return;

    mTicksPerRow = speeds.at(mCurrLevel);
    mCurrCleardRows -= requiredRows;
    ++mCurrLevel;
}

void Timer::tick(Uint32 cappingTicks)
{
    Uint32 interval = SDL_GetTicks() - mMark;
    if (interval < cappingTicks)
        SDL_Delay(cappingTicks - interval);

    Uint32 currTicks = getTicks();
    mFrameTicks = currTicks - mLastTicks;
    mLastTicks = currTicks;
    mMark = SDL_GetTicks();
}

void Timer::pause()
{
    if (!mHasPaused)
    {
        mHasPaused = true;
        mPauseStart = SDL_GetTicks();
    }
}

void Timer::resume()
{
    if (mHasPaused)
    {
        mHasPaused = false;
        mPausedTicks += SDL_GetTicks() - mPauseStart;
    }
}

void PlayState::handleEvent(const SDL_Event& e)
{
    if (e.type != SDL_KEYDOWN)
        return;

    if (e.key.keysym.sym == SDLK_ESCAPE)
    {
        GameStateManager::instance().changeState(make_shared<PauseState>());
        return;
    }

    TetrominoController::instance().onKeyDown(e);
}

void PlayState::update()
{
    TetrominoController::instance().update();
}

void PlayState::draw()
{
    Playfield::instance().draw();
    TetrominoController::instance().draw();
}

void PlayState::onEnter()
{
    ScoreBoard::instance().updateTitle();
}

void PauseState::handleEvent(const SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)
    {
        GameStateManager::instance().changeState(make_unique<PlayState>());
    }
}

void PauseState::onEnter()
{
    const char* title =
        GameStateManager::instance().lastStateID() == GameState::ID::Playing
        ? "Paused... press <Enter> to resume!"
        : "Tetris - press <Enter> to start!";

    SDL_SetWindowTitle(Game::instance().window(), title);
    Timer::instance().pause();
}

void PauseState::onExit(ID nextStateID)
{
    if (nextStateID == ID::Playing)
        Timer::instance().resume();
}

void GameOver::handleEvent(const SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)
    {
        GameStateManager::instance().changeState(make_shared<PlayState>());
    }
}

void GameOver::draw()
{
    Playfield::instance().draw();
    TetrominoController::instance().draw();
}

void GameOver::onEnter()
{
    ostringstream ss;
    ss << "Game Over! "
       << ScoreBoard::instance().title()
       << " - press <Enter> to restart";
    SDL_SetWindowTitle(Game::instance().window(), ss.str().c_str());
}

void GameOver::onExit(ID nextStateID)
{
    if (nextStateID == ID::Playing)
        Game::instance().reset();
}

void BeforeExit::handleEvent(const SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN)
    {
        if (e.key.keysym.sym == SDLK_ESCAPE)
        {
            SDL_Quit();
            exit(EXIT_SUCCESS);
        }

        if (e.key.keysym.sym == SDLK_RETURN)
            GameStateManager::instance().goBack();
    }
}

void BeforeExit::onEnter()
{
    SDL_SetWindowTitle(
        Game::instance().window(),
        "Press <Esc> to exit or <Enter> to cancel!");
}

void GameStateManager::handleEvents()
{
    for (SDL_Event e; SDL_PollEvent(&e);)
    {
        if (e.type == SDL_QUIT && mCurrState->id() != GameState::ID::BeforeExit)
        {
            changeState(make_shared<BeforeExit>());
            continue;
        }
        mCurrState->handleEvent(e);
    }
}

void GameStateManager::changeState(const shared_ptr<GameState>& state)
{
    if (mCurrState)
        mCurrState->onExit(state->id());
    mLastState = mCurrState;
    mCurrState = state;
    mCurrState->onEnter();
}

void GameStateManager::goBack()
{
    if (!mCurrState || !mLastState)
        return;

    mCurrState->onExit(mLastState->id());
    mLastState->onEnter();
    mCurrState.swap(mLastState);
}

Game::Game()
{
    REQUIRES_ZERO(SDL_Init(SDL_INIT_EVERYTHING));

    mWindow.reset(SDL_CreateWindow(
        "Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN));
    REQUIRES_NOT_NULL(mWindow);

    mRenderer.reset(SDL_CreateRenderer(window(), -1, SDL_RENDERER_ACCELERATED));
    REQUIRES_NOT_NULL(mRenderer);
    REQUIRES_ZERO(SDL_SetRenderDrawBlendMode(renderer(), SDL_BLENDMODE_BLEND));

    mBackground = [this] {
        int w, h;
        SDL_GetWindowSize(window(), &w, &h);

        TexturePtr background(
            SDL_CreateTexture(
                renderer(), SDL_GetWindowPixelFormat(window()),
                SDL_TEXTUREACCESS_TARGET, w, h),
            SDL_DestroyTexture);
        REQUIRES_NOT_NULL(background);

        REQUIRES_ZERO(SDL_SetRenderTarget(renderer(), background.get()));
        REQUIRES_ZERO(SDL_SetRenderDrawColor(renderer(), BACKGROUND_COLOR));
        REQUIRES_ZERO(SDL_RenderClear(renderer()));

        REQUIRES_ZERO(SDL_SetRenderDrawColor(renderer(), 0x37, 0x37, 0x37, 0xFF));
        for (int x = 0; x < SCREEN_WIDTH; x += CELL_LEN)
            REQUIRES_ZERO(SDL_RenderDrawLine(renderer(), x, 0, x, SCREEN_HEIGHT));
        for (int y = CELL_LEN; y < SCREEN_HEIGHT; y += CELL_LEN)
            REQUIRES_ZERO(SDL_RenderDrawLine(renderer(), 0, y, SCREEN_WIDTH, y));

        REQUIRES_ZERO(SDL_SetRenderDrawColor(renderer(), 0xFF, 0xFF, 0xFF, 0xFF));
        REQUIRES_ZERO(SDL_RenderDrawRect(renderer(), &HOLD_BOARD));
        REQUIRES_ZERO(SDL_RenderDrawRect(renderer(), &PLAYFIELD));
        REQUIRES_ZERO(SDL_RenderDrawRect(renderer(), &NEXT_BOARD));

        REQUIRES_ZERO(SDL_SetRenderTarget(renderer(), nullptr));
        return background;
    }();
}

void Game::reset()
{
    Playfield::instance().reset();
    ScoreBoard::instance().reset();
    TetrominoController::instance().reset();
}

void Game::draw()
{
    REQUIRES_ZERO(SDL_SetRenderDrawColor(renderer(), BACKGROUND_COLOR));
    REQUIRES_ZERO(SDL_RenderClear(renderer()));
    REQUIRES_ZERO(SDL_RenderCopy(renderer(), mBackground.get(), nullptr, nullptr));

    GameStateManager::instance().draw();

    SDL_RenderPresent(Game::instance().renderer());
}

int main()
{
    GameStateManager::instance().changeState(make_shared<PauseState>());
    while (true)
    {
        GameStateManager::instance().handleEvents();
        GameStateManager::instance().update();
        Game::instance().draw();
        Timer::instance().tick(MILLISECONDS_PER_FRAME);
    }
}
