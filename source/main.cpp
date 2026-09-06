#include <3ds.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <sys/stat.h>

enum class Mark : int8_t { Empty = 0, X = 1, O = 2 };
enum class Difficulty : int8_t { TwoPlayer = 0, Easy = 1, Medium = 2, Hard = 3, Impossible = 4 };
enum class AppScreen : int8_t { Splash, ModeSelect, SideSelect, Playing, GameOver, Stats, Help };

static Mark opponentOf(Mark m) { return m == Mark::X ? Mark::O : Mark::X; }
static char markGlyph(Mark m) { return m == Mark::X ? 'X' : (m == Mark::O ? 'O' : ' '); }

struct Rect {
    int x0, y0, x1, y1;
    bool contains(int px, int py) const { return px >= x0 && px <= x1 && py >= y0 && py <= y1; }
};

class Board {
public:
    std::array<Mark, 9> cells;

    Board() { reset(); }

    void reset() { cells.fill(Mark::Empty); }

    bool place(int idx, Mark m) {
        if (idx < 0 || idx > 8 || cells[idx] != Mark::Empty) return false;
        cells[idx] = m;
        return true;
    }

    static const int (&lines())[8][3] {
        static const int L[8][3] = {
            {0,1,2},{3,4,5},{6,7,8},
            {0,3,6},{1,4,7},{2,5,8},
            {0,4,8},{2,4,6}
        };
        return L;
    }

    Mark winner() const {
        for (auto &l : lines()) {
            if (cells[l[0]] != Mark::Empty && cells[l[0]] == cells[l[1]] && cells[l[1]] == cells[l[2]])
                return cells[l[0]];
        }
        return Mark::Empty;
    }

    const int* winningLine() const {
        for (auto &l : lines()) {
            if (cells[l[0]] != Mark::Empty && cells[l[0]] == cells[l[1]] && cells[l[1]] == cells[l[2]])
                return l;
        }
        return nullptr;
    }

    bool isFull() const {
        for (auto c : cells) if (c == Mark::Empty) return false;
        return true;
    }

    bool isTerminal() const { return winner() != Mark::Empty || isFull(); }

    std::vector<int> available() const {
        std::vector<int> r;
        r.reserve(9);
        for (int i = 0; i < 9; i++) if (cells[i] == Mark::Empty) r.push_back(i);
        return r;
    }
};

namespace AIEngine {

    inline int terminalScore(const Board& b, Mark ai, Mark human, int depth) {
        Mark w = b.winner();
        if (w == ai) return 10 - depth;
        if (w == human) return depth - 10;
        return 0;
    }

    int minimax(Board& b, int depth, bool maximizing, Mark ai, Mark human, int alpha, int beta) {
        if (b.winner() != Mark::Empty || b.isFull())
            return terminalScore(b, ai, human, depth);

        if (maximizing) {
            int best = -1000;
            for (int i : b.available()) {
                b.cells[i] = ai;
                best = std::max(best, minimax(b, depth + 1, false, ai, human, alpha, beta));
                b.cells[i] = Mark::Empty;
                alpha = std::max(alpha, best);
                if (beta <= alpha) break;
            }
            return best;
        } else {
            int best = 1000;
            for (int i : b.available()) {
                b.cells[i] = human;
                best = std::min(best, minimax(b, depth + 1, true, ai, human, alpha, beta));
                b.cells[i] = Mark::Empty;
                beta = std::min(beta, best);
                if (beta <= alpha) break;
            }
            return best;
        }
    }

    int bestMove(Board board, Mark ai, Mark human) {
        int bestScore = -1000, move = -1;
        for (int i : board.available()) {
            board.cells[i] = ai;
            int s = minimax(board, 1, false, ai, human, -1000, 1000);
            board.cells[i] = Mark::Empty;
            if (s > bestScore) { bestScore = s; move = i; }
        }
        return move;
    }

    int pickMove(const Board& board, Mark ai, Mark human, Difficulty diff) {
        std::vector<int> avail = board.available();
        if (avail.empty()) return -1;

        int roll = std::rand() % 100;
        bool useBest = false;

        switch (diff) {
            case Difficulty::Impossible: useBest = true; break;
            case Difficulty::Hard: useBest = roll < 80; break;
            case Difficulty::Medium: useBest = roll < 40; break;
            default: useBest = false; break;
        }

        if (useBest) return bestMove(board, ai, human);
        return avail[std::rand() % avail.size()];
    }
}

struct DifficultyRecord {
    int wins = 0;
    int losses = 0;
    int draws = 0;
};

struct Stats {
    DifficultyRecord records[5];
    int twoPlayerXWins = 0;
    int twoPlayerOWins = 0;
    int twoPlayerDraws = 0;

    void reset() {
        for (auto &r : records) r = DifficultyRecord();
        twoPlayerXWins = twoPlayerOWins = twoPlayerDraws = 0;
    }

    bool load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        Stats tmp;
        size_t n = fread(&tmp, sizeof(Stats), 1, f);
        fclose(f);
        if (n == 1) { *this = tmp; return true; }
        return false;
    }

    bool save(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) return false;
        fwrite(this, sizeof(Stats), 1, f);
        fclose(f);
        return true;
    }
};

struct ThemeColors {
    std::string name;
    std::string board;
    std::string accent;
    std::string highlightX;
    std::string highlightO;
};

static std::vector<ThemeColors> buildThemes() {
    return {
        { "Dunkel", "\x1b[37;40m", "\x1b[30;47m", "\x1b[36;40m", "\x1b[33;40m" },
        { "Hell",   "\x1b[30;47m", "\x1b[37;40m", "\x1b[34;47m", "\x1b[31;47m" },
        { "Ozean",  "\x1b[36;44m", "\x1b[34;46m", "\x1b[37;44m", "\x1b[33;44m" },
        { "Retro",  "\x1b[33;40m", "\x1b[30;43m", "\x1b[32;40m", "\x1b[31;40m" }
    };
}

class ThemeManager {
public:
    ThemeManager() : themes(buildThemes()), index(0) {}
    const ThemeColors& current() const { return themes[index]; }
    void next() { index = (index + 1) % (int)themes.size(); }
    int count() const { return (int)themes.size(); }
    int currentIndex() const { return index; }
    void setIndex(int i) { if (i >= 0 && i < (int)themes.size()) index = i; }
private:
    std::vector<ThemeColors> themes;
    int index;
};

struct Particle {
    float x, y;
    float vx, vy;
    char glyph;
    int life;
};

class ParticleSystem {
public:
    void spawnBurst(float originX, float originY) {
        static const char glyphs[] = { '*', '.', '+', 'o', 'x' };
        for (int i = 0; i < 14; i++) {
            Particle p;
            p.x = originX;
            p.y = originY;
            float angle = (float)(std::rand() % 360) * 0.0174533f;
            float speed = 0.15f + (std::rand() % 100) / 400.0f;
            p.vx = std::cos(angle) * speed * 2.0f;
            p.vy = std::sin(angle) * speed;
            p.glyph = glyphs[std::rand() % 5];
            p.life = 10 + std::rand() % 8;
            particles.push_back(p);
        }
    }

    void update() {
        for (auto &p : particles) {
            p.x += p.vx;
            p.y += p.vy;
            p.life--;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.life <= 0; }), particles.end());
    }

    void render(int maxCol, int maxRow) const {
        for (auto &p : particles) {
            int col = (int)p.x;
            int row = (int)p.y;
            if (col < 1 || col > maxCol || row < 1 || row > maxRow) continue;
            printf("\x1b[%d;%dH%c", row, col, p.glyph);
        }
    }

    bool active() const { return !particles.empty(); }
    void clear() { particles.clear(); }

private:
    std::vector<Particle> particles;
};

class TouchZones {
public:
    static int tabIndex(int px, int py) {
        if (py > 29) return -1;
        int idx = px / 64;
        return (idx >= 0 && idx < 5) ? idx : -1;
    }

    static Rect sideSelectX() { return { 0, 80, 159, 159 }; }
    static Rect sideSelectO() { return { 160, 80, 319, 159 }; }
    static Rect boardArea() { return { 72, 40, 256, 176 }; }
    static Rect rematchButton() { return { 0, 180, 159, 199 }; }
    static Rect mainMenuButton() { return { 160, 180, 319, 199 }; }
    static Rect statsButton() { return { 0, 200, 159, 215 }; }
    static Rect helpButton() { return { 160, 200, 319, 215 }; }
    static Rect themeToggle() { return { 0, 216, 319, 239 }; }
    static Rect backButton() { return { 0, 216, 319, 239 }; }

    static int boardCellFromTouch(int px, int py) {
        Rect area = boardArea();
        if (!area.contains(px, py)) return -1;
        int col = (px - area.x0) / 61;
        int row = (py - area.y0) / 45;
        if (col > 2) col = 2;
        if (row > 2) row = 2;
        return row * 3 + col;
    }
};

class GameApp {
public:
    GameApp() : difficulty(Difficulty::TwoPlayer), currentPlayer(Mark::X),
                humanMark(Mark::X), screen(AppScreen::Splash), previousScreen(AppScreen::Splash),
                menuFocus(0), boardCursor(4), aiThinkFrames(0), waitingForAI(false), frameCounter(0),
                needsRedraw(true) {
        revealCounts.fill(7);
    }

    void init() {
        gfxInitDefault();
        consoleInit(GFX_TOP, &topScreen);
        consoleInit(GFX_BOTTOM, &bottomScreen);
        std::srand((unsigned)time(nullptr));
        mkdir("sdmc:/3ds", 0777);
        mkdir("sdmc:/3ds/tictactoe", 0777);
        stats.load(statsPath());
    }

    void run() {
        while (aptMainLoop()) {
            hidScanInput();
            u32 down = hidKeysDown();
            if (down & KEY_START) break;

            handleButtons(down);

            if (down & KEY_TOUCH) {
                touchPosition touch;
                hidTouchRead(&touch);
                handleTouch(touch.px, touch.py);
            }

            updateAnimations();

            if (waitingForAI) {
                aiThinkFrames--;
                if (aiThinkFrames <= 0) {
                    performAIMove();
                    waitingForAI = false;
                }
            }

            if (needsRedraw || particles.active()) {
                render();
                needsRedraw = false;
            }

            frameCounter++;
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        }
        gfxExit();
    }

private:
    PrintConsole topScreen, bottomScreen;
    Board board;
    Difficulty difficulty;
    Mark currentPlayer;
    Mark humanMark;
    AppScreen screen;
    AppScreen previousScreen;
    ThemeManager theme;
    ParticleSystem particles;
    Stats stats;
    std::array<int, 9> revealCounts;
    int menuFocus;
    int boardCursor;
    int aiThinkFrames;
    bool waitingForAI;
    uint32_t frameCounter;
    bool needsRedraw;

    static const char* statsPath() { return "sdmc:/3ds/tictactoe/stats.bin"; }

    void goTo(AppScreen s) {
        previousScreen = screen;
        screen = s;
        needsRedraw = true;
    }

    void startNewGame(Difficulty d, Mark human) {
        difficulty = d;
        humanMark = human;
        board.reset();
        revealCounts.fill(7);
        currentPlayer = Mark::X;
        boardCursor = 4;
        particles.clear();
        waitingForAI = false;
        goTo(AppScreen::Playing);
        maybeTriggerAI();
    }

    void selectMode(int idx) {
        Difficulty d = (Difficulty)idx;
        if (d == Difficulty::TwoPlayer) {
            difficulty = d;
            goTo(AppScreen::SideSelect);
        } else {
            startNewGame(d, Mark::X);
        }
    }

    void maybeTriggerAI() {
        if (difficulty == Difficulty::TwoPlayer) return;
        Mark aiMark = opponentOf(humanMark);
        if (currentPlayer == aiMark && !board.isTerminal()) {
            waitingForAI = true;
            aiThinkFrames = 18 + std::rand() % 12;
        }
    }

    void performAIMove() {
        Mark aiMark = opponentOf(humanMark);
        int move = AIEngine::pickMove(board, aiMark, humanMark, difficulty);
        if (move < 0) return;
        placeMark(move, aiMark);
    }

    void placeMark(int idx, Mark mark) {
        if (!board.place(idx, mark)) return;
        revealCounts[idx] = 0;
        needsRedraw = true;

        int row = idx / 3, col = idx % 3;
        float ox = 15.0f + col * 10.0f;
        float oy = 4.0f + row * 10.0f;
        particles.spawnBurst(ox, oy);

        if (board.winner() != Mark::Empty || board.isFull()) {
            recordResult();
            goTo(AppScreen::GameOver);
            return;
        }

        currentPlayer = opponentOf(currentPlayer);
        if (difficulty != Difficulty::TwoPlayer) maybeTriggerAI();
    }

    void recordResult() {
        Mark w = board.winner();
        if (difficulty == Difficulty::TwoPlayer) {
            if (w == Mark::X) stats.twoPlayerXWins++;
            else if (w == Mark::O) stats.twoPlayerOWins++;
            else stats.twoPlayerDraws++;
        } else {
            DifficultyRecord& rec = stats.records[(int)difficulty];
            Mark aiMark = opponentOf(humanMark);
            if (w == humanMark) rec.wins++;
            else if (w == aiMark) rec.losses++;
            else rec.draws++;
        }
        stats.save(statsPath());
    }

    bool humanMayPlay() const {
        if (screen != AppScreen::Playing) return false;
        if (difficulty == Difficulty::TwoPlayer) return true;
        return currentPlayer == humanMark && !waitingForAI;
    }

    void updateAnimations() {
        bool animating = false;
        for (int i = 0; i < 9; i++) {
            if (board.cells[i] != Mark::Empty && revealCounts[i] < 7) {
                revealCounts[i]++;
                animating = true;
                needsRedraw = true;
            }
        }
        if (particles.active()) {
            particles.update();
            needsRedraw = true;
        }
        (void)animating;
    }

    void handleButtons(u32 down) {
        if (down & KEY_L) { theme.next(); needsRedraw = true; }
        if (down & KEY_R) { theme.next(); needsRedraw = true; }

        if (down & KEY_X) {
            if (screen == AppScreen::Help) goTo(previousScreen);
            else goTo(AppScreen::Help);
            return;
        }
        if (down & KEY_SELECT) {
            if (screen == AppScreen::Stats) goTo(previousScreen);
            else goTo(AppScreen::Stats);
            return;
        }
        if (down & KEY_B) {
            if (screen == AppScreen::Help || screen == AppScreen::Stats) goTo(previousScreen);
        }

        switch (screen) {
            case AppScreen::Splash:
                if (down & KEY_A) goTo(AppScreen::ModeSelect);
                break;
            case AppScreen::ModeSelect:
                if (down & KEY_DLEFT) { menuFocus = (menuFocus + 4) % 5; needsRedraw = true; }
                if (down & KEY_DRIGHT) { menuFocus = (menuFocus + 1) % 5; needsRedraw = true; }
                if (down & KEY_A) selectMode(menuFocus);
                break;
            case AppScreen::SideSelect:
                if (down & (KEY_DLEFT | KEY_DRIGHT)) { menuFocus = 1 - menuFocus; needsRedraw = true; }
                if (down & KEY_A) startNewGame(Difficulty::TwoPlayer, menuFocus == 0 ? Mark::X : Mark::O);
                break;
            case AppScreen::Playing:
                if (humanMayPlay()) {
                    int r = boardCursor / 3, c = boardCursor % 3;
                    if (down & KEY_DUP) r = (r + 2) % 3;
                    if (down & KEY_DDOWN) r = (r + 1) % 3;
                    if (down & KEY_DLEFT) c = (c + 2) % 3;
                    if (down & KEY_DRIGHT) c = (c + 1) % 3;
                    boardCursor = r * 3 + c;
                    if (down & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) needsRedraw = true;
                    if (down & KEY_A) placeMark(boardCursor, currentPlayer);
                }
                break;
            case AppScreen::GameOver:
                if (down & (KEY_DLEFT | KEY_DRIGHT)) { menuFocus = 1 - menuFocus; needsRedraw = true; }
                if (down & KEY_A) {
                    if (menuFocus == 0) startNewGame(difficulty, humanMark);
                    else goTo(AppScreen::ModeSelect);
                }
                break;
            default:
                break;
        }
    }

    void handleTouch(int px, int py) {
        int tab = TouchZones::tabIndex(px, py);
        if (tab >= 0) {
            menuFocus = tab;
            selectMode(tab);
            return;
        }

        if (screen == AppScreen::Help || screen == AppScreen::Stats) {
            if (TouchZones::backButton().contains(px, py)) goTo(previousScreen);
            return;
        }

        if (TouchZones::statsButton().contains(px, py)) { goTo(AppScreen::Stats); return; }
        if (TouchZones::helpButton().contains(px, py)) { goTo(AppScreen::Help); return; }
        if (TouchZones::themeToggle().contains(px, py) && screen != AppScreen::GameOver) {
            theme.next();
            needsRedraw = true;
            return;
        }

        switch (screen) {
            case AppScreen::Splash:
                goTo(AppScreen::ModeSelect);
                break;
            case AppScreen::SideSelect:
                if (TouchZones::sideSelectX().contains(px, py)) startNewGame(Difficulty::TwoPlayer, Mark::X);
                else if (TouchZones::sideSelectO().contains(px, py)) startNewGame(Difficulty::TwoPlayer, Mark::O);
                break;
            case AppScreen::Playing:
                if (humanMayPlay()) {
                    int idx = TouchZones::boardCellFromTouch(px, py);
                    if (idx >= 0) {
                        boardCursor = idx;
                        placeMark(idx, currentPlayer);
                    }
                }
                break;
            case AppScreen::GameOver:
                if (TouchZones::rematchButton().contains(px, py)) startNewGame(difficulty, humanMark);
                else if (TouchZones::mainMenuButton().contains(px, py)) goTo(AppScreen::ModeSelect);
                else if (TouchZones::themeToggle().contains(px, py)) { theme.next(); needsRedraw = true; }
                break;
            default:
                break;
        }
    }

    void putAt(int row, int col, const std::string& s) {
        printf("\x1b[%d;%dH%s", row, col, s.c_str());
    }

    void clearScreen(const std::string& colorCode) {
        printf("%s\x1b[2J", colorCode.c_str());
    }

    void drawCentered(int row, int width, int leftCol, const std::string& text) {
        int pad = (width - (int)text.size()) / 2;
        if (pad < 0) pad = 0;
        putAt(row, leftCol + pad, text);
    }

    void render() {
        renderTop();
        renderBottom();
    }

    void renderTop() {
        const ThemeColors& t = theme.current();
        consoleSelect(&topScreen);
        clearScreen(t.board);

        if (screen == AppScreen::Splash) {
            drawCentered(10, 50, 0, "T I C   T A C   T O E");
            drawCentered(12, 50, 0, "3DS Edition");
            drawCentered(16, 50, 0, "Tippe den Bildschirm an");
            drawCentered(17, 50, 0, "oder druecke A");
            return;
        }

        if (screen == AppScreen::Help || screen == AppScreen::Stats) {
            drawCentered(2, 50, 0, screen == AppScreen::Help ? "HILFE" : "STATISTIK");
            return;
        }

        for (int r = 1; r <= 29; r++) {
            for (int c = 11; c <= 39; c++) {
                if (r == 10 || r == 20) {
                    putAt(r, c, (c == 20 || c == 30) ? "\xC5" : "\xC4");
                } else if (c == 20 || c == 30) {
                    putAt(r, c, "\xB3");
                }
            }
        }

        const int* winLine = board.winningLine();

        for (int i = 0; i < 9; i++) {
            if (board.cells[i] == Mark::Empty) continue;
            int row = 1 + (i / 3) * 10;
            int col = 11 + (i % 3) * 10;
            int reveal = revealCounts[i];
            bool isWinning = false;
            if (winLine) for (int k = 0; k < 3; k++) if (winLine[k] == i) isWinning = true;

            std::string color = board.cells[i] == Mark::X ? t.highlightX : t.highlightO;
            printf("%s", isWinning ? t.accent.c_str() : color.c_str());

            static const char* xrows[7] = {
                "XX     XX", " XX   XX ", "  XX XX  ", "   XXX   ",
                "  XX XX  ", " XX   XX ", "XX     XX"
            };
            static const char* orows[7] = {
                "  OOOOO  ", " OO   OO ", "OO     OO", "OO     OO",
                "OO     OO", " OO   OO ", "  OOOOO  "
            };
            const char** glyph = (board.cells[i] == Mark::X) ? xrows : orows;

            for (int gr = 0; gr < reveal && gr < 7; gr++)
                putAt(row + 1 + gr, col, glyph[gr]);

            printf("%s", t.board.c_str());
        }

        if (screen == AppScreen::Playing && humanMayPlay()) {
            int row = 1 + (boardCursor / 3) * 10;
            int col = 11 + (boardCursor % 3) * 10;
            printf("%s", t.accent.c_str());
            putAt(row, col, "+---------+");
            putAt(row + 8, col, "+---------+");
            printf("%s", t.board.c_str());
        }

        particles.render(49, 29);
    }

    void renderBottom() {
        const ThemeColors& t = theme.current();
        consoleSelect(&bottomScreen);
        clearScreen(t.board);

        static const char* modeNames[5] = { "   2P   ", "  Easy  ", " Medium ", "  Hard  ", " Imposs " };
        for (int i = 0; i < 5; i++) {
            bool selected = ((int)difficulty == i) && screen != AppScreen::ModeSelect;
            bool focused = (screen == AppScreen::ModeSelect && menuFocus == i);
            bool active = selected || focused;
            printf("\x1b[1;%dH%s%s%s", i * 8 + 1, active ? t.accent.c_str() : t.board.c_str(),
                   modeNames[i], t.board.c_str());
            printf("\x1b[2;%dH%s        %s", i * 8 + 1, active ? t.accent.c_str() : t.board.c_str(),
                   t.board.c_str());
        }

        if (screen == AppScreen::Splash) {
            drawCentered(15, 40, 0, "Tippe unten oder");
            drawCentered(16, 40, 0, "druecke A zum Start");
            return;
        }

        if (screen == AppScreen::Help) {
            renderHelp();
            drawBackButton();
            return;
        }

        if (screen == AppScreen::Stats) {
            renderStats();
            drawBackButton();
            return;
        }

        if (screen == AppScreen::SideSelect) {
            bool focusX = (menuFocus == 0);
            printf("%s", focusX ? t.accent.c_str() : t.board.c_str());
            putAt(12, 8, "+---------+");
            putAt(13, 8, "| S1: X   |");
            putAt(14, 8, "+---------+");
            printf("%s", (!focusX) ? t.accent.c_str() : t.board.c_str());
            putAt(12, 23, "+---------+");
            putAt(13, 23, "| S1: O   |");
            putAt(14, 23, "+---------+");
            printf("%s", t.board.c_str());
            drawCentered(17, 40, 0, "(Bitte oben waehlen)");
            drawFooterBar();
            return;
        }

        for (int r = 5; r <= 21; r++) {
            for (int c = 9; c <= 31; c++) {
                if (r == 10 || r == 16) {
                    putAt(r, c, (c == 16 || c == 24) ? "\xC5" : "\xC4");
                } else if (c == 16 || c == 24) {
                    putAt(r, c, "\xB3");
                }
            }
        }
        for (int i = 0; i < 9; i++) {
            if (board.cells[i] == Mark::Empty) continue;
            int row = 5 + (i / 3) * 6;
            int col = 9 + (i % 3) * 8;
            putAt(row + 2, col + 3, std::string(1, markGlyph(board.cells[i])));
        }
        if (screen == AppScreen::Playing && humanMayPlay()) {
            int row = 5 + (boardCursor / 3) * 6;
            int col = 9 + (boardCursor % 3) * 8;
            printf("%s", t.accent.c_str());
            putAt(row + 1, col + 1, "*");
            printf("%s", t.board.c_str());
        }

        if (screen == AppScreen::GameOver) {
            Mark w = board.winner();
            if (w == Mark::Empty) drawCentered(23, 40, 0, "UNENTSCHIEDEN!");
            else {
                std::string txt = "GEWINNER: ";
                txt += markGlyph(w);
                drawCentered(23, 40, 0, txt);
            }
            printf("%s", (menuFocus == 0) ? t.accent.c_str() : t.board.c_str());
            putAt(25, 4, "[   NOCHMAL   ]");
            printf("%s", (menuFocus == 1) ? t.accent.c_str() : t.board.c_str());
            putAt(25, 22, "[  HAUPTMENUE ]");
            printf("%s", t.board.c_str());
        } else if (screen == AppScreen::Playing) {
            if (waitingForAI) drawCentered(23, 40, 0, "GEGNER DENKT NACH...");
            else {
                std::string txt = "AM ZUG: ";
                txt += markGlyph(currentPlayer);
                drawCentered(23, 40, 0, txt);
            }
        }

        drawFooterBar();
    }

    void drawFooterBar() {
        const ThemeColors& t = theme.current();
        putAt(27, 2, "[ STATISTIK ]");
        putAt(27, 22, "[ HILFE ]");
        printf("%s", t.accent.c_str());
        putAt(29, 3, "[  HIER TIPPEN / L / R : THEME  ]");
        printf("%s", t.board.c_str());
    }

    void drawBackButton() {
        const ThemeColors& t = theme.current();
        printf("%s", t.accent.c_str());
        putAt(29, 12, "[  ZURUECK (B)  ]");
        printf("%s", t.board.c_str());
    }

    void renderHelp() {
        drawCentered(3, 40, 0, "SPIELREGELN");
        putAt(6, 3, "Berühre ein Feld oder nutze");
        putAt(7, 3, "das Steuerkreuz + A zum Setzen.");
        putAt(9, 3, "Oben: Modus waehlen");
        putAt(10, 3, "(2P, Easy, Medium, Hard, Imposs).");
        putAt(12, 3, "L / R / T: Farbthema wechseln");
        putAt(13, 3, "SELECT: Statistik oeffnen");
        putAt(14, 3, "X: Diese Hilfe oeffnen/schliessen");
        putAt(16, 3, "START: Spiel beenden");
    }

    void renderStats() {
        drawCentered(3, 40, 0, "STATISTIK");
        putAt(6, 3, "2 Spieler:");
        char line[48];
        std::snprintf(line, sizeof(line), "X: %d  O: %d  Unentsch.: %d",
                      stats.twoPlayerXWins, stats.twoPlayerOWins, stats.twoPlayerDraws);
        putAt(7, 5, line);

        static const char* names[5] = { "2P", "Easy", "Medium", "Hard", "Imposs" };
        int row = 10;
        for (int i = 1; i <= 4; i++) {
            const DifficultyRecord& r = stats.records[i];
            std::snprintf(line, sizeof(line), "%-7s S:%d N:%d U:%d", names[i], r.wins, r.losses, r.draws);
            putAt(row, 3, line);
            row += 2;
        }
    }
};

int main(int argc, char** argv) {
    GameApp app;
    app.init();
    app.run();
    return 0;
}
