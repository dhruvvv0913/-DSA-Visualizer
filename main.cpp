/*
 * ===========================================================================
 *  DSA VISUALIZER  --  Sorting & Searching Algorithm Visualiser
 * ---------------------------------------------------------------------------
 *  Minor Project  :  C, C++ with DSA Program (InternsElite)
 *  Author         :  Dhruv
 *  Language       :  Standard C++ (C++11 / C++14), STL, OOP
 *  Visualisation  :  Console based, animated colour bar graph
 *
 *  WHAT THIS PROGRAM DOES
 *  ----------------------
 *  It animates, step by step, how the classic sorting and searching
 *  algorithms actually move data around.  Every comparison, every swap and
 *  every pass is drawn on screen as a colour coded bar chart, while a live
 *  statistics panel keeps a running count of the work being done.
 *
 *  ALGORITHMS IMPLEMENTED
 *  ----------------------
 *      Sorting  :  Bubble, Selection, Insertion, Merge (recursive),
 *                  Quick (recursive, Lomuto partition)
 *      Searching:  Linear, Binary (recursive halving, with a guard that
 *                  refuses to run on an unsorted array)
 *
 *  OOP CONCEPTS USED
 *  -----------------
 *      Classes & Objects .... Theme, Statistics, Visualizer, Algorithm, ...
 *      Encapsulation ........ every class keeps its state private and
 *                             exposes it only through member functions
 *      Inheritance .......... Algorithm -> SortingAlgorithm -> BubbleSort
 *      Polymorphism ......... the menu stores base class pointers and calls
 *                             virtual run() / name() / complexity() on them
 *      Abstraction .......... Algorithm is a pure virtual interface
 *      Composition .......... Visualizer owns a Theme, Algorithm owns Stats
 *      RAII ................. unique_ptr owns the algorithm objects, the
 *                             console is restored on every exit path
 *
 *  BUILD
 *  -----
 *      g++ -std=c++14 -O2 main.cpp -o dsa_visualizer
 *      ./dsa_visualizer
 *
 *  See README.md for full documentation and screenshots.
 * ===========================================================================
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <functional>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
#endif

/* ===========================================================================
 * SECTION 0 : GLOBAL LIMITS AND SMALL UTILITIES
 * ===========================================================================
 * Every user supplied number in this program is validated against the limits
 * declared here, so the rules live in exactly one place.
 */
namespace limits {
    const int MIN_ELEMENTS   = 2;     // an array of 1 element is not worth animating
    const int MAX_ELEMENTS   = 24;    // keeps the bar chart inside a normal console
    const int MIN_VALUE      = 1;     // bar heights are drawn from these bounds
    const int MAX_VALUE      = 99;
    const int MIN_BENCH_SIZE = 10;    // benchmark runs without animation, so it
    const int MAX_BENCH_SIZE = 2000;  // can afford much larger inputs
}

namespace util {

/* Repeat a (possibly multi byte UTF-8) string n times. */
std::string repeat(const std::string& unit, int times) {
    std::string out;
    if (times <= 0) return out;
    out.reserve(unit.size() * static_cast<size_t>(times));
    for (int i = 0; i < times; ++i) out += unit;
    return out;
}

/*
 * How many terminal columns a string occupies.
 *
 * std::string::size() counts bytes, and the box drawing characters used here
 * are multi byte UTF-8 - so padding by size() would shift a panel's right hand
 * border left by one column for every non ASCII character in the line.  Every
 * glyph this program prints is single width, so counting UTF-8 lead bytes
 * (everything except 10xxxxxx continuation bytes) gives the column count.
 */
int displayWidth(const std::string& text) {
    int width = 0;
    for (size_t i = 0; i < text.size(); ++i)
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) ++width;
    return width;
}

/* Pad text on the right so the whole field is `width` columns. */
std::string padRight(const std::string& text, int width) {
    int length = displayWidth(text);
    if (length >= width) return text;
    return text + std::string(static_cast<size_t>(width - length), ' ');
}

/* Pad text on the left (used for right aligned numbers). */
std::string padLeft(const std::string& text, int width) {
    int length = displayWidth(text);
    if (length >= width) return text;
    return std::string(static_cast<size_t>(width - length), ' ') + text;
}

/* Centre text inside a field of `width` columns. */
std::string padCentre(const std::string& text, int width) {
    int length = displayWidth(text);
    if (length >= width) return text;
    int left  = (width - length) / 2;
    int right = width - length - left;
    return std::string(static_cast<size_t>(left), ' ') + text +
           std::string(static_cast<size_t>(right), ' ');
}

/* Trim leading and trailing whitespace. */
std::string trim(const std::string& text) {
    size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string toUpper(const std::string& text) {
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));
    return out;
}

/* Render a vector as "[ 12, 45, 7 ]" for the summary panels. */
std::string arrayToString(const std::vector<int>& data) {
    std::ostringstream out;
    out << "[ ";
    for (size_t i = 0; i < data.size(); ++i) {
        out << data[i];
        if (i + 1 < data.size()) out << ", ";
    }
    out << " ]";
    return out.str();
}

/* Format a microsecond duration into the most readable unit. */
std::string formatDuration(double microseconds) {
    std::ostringstream out;
    out << std::fixed;
    if (microseconds < 1.0)        out << std::setprecision(3) << microseconds       << " us";
    else if (microseconds < 1000.0)out << std::setprecision(2) << microseconds       << " us";
    else if (microseconds < 1e6)   out << std::setprecision(3) << microseconds/1e3   << " ms";
    else                           out << std::setprecision(3) << microseconds/1e6   << " s";
    return out.str();
}

/* Thousands separators, so 1234567 reads as 1,234,567. */
std::string withCommas(long long value) {
    std::string digits = std::to_string(value < 0 ? -value : value);
    std::string out;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        out += digits[static_cast<size_t>(i)];
        if (++count % 3 == 0 && i > 0) out += ',';
    }
    if (value < 0) out += '-';
    std::reverse(out.begin(), out.end());
    return out;
}

} // namespace util

/* ===========================================================================
 * SECTION 1 : PLATFORM LAYER
 * ===========================================================================
 * Everything operating system specific is isolated behind this one class, so
 * the rest of the program is plain portable C++.  On Windows it switches the
 * console into UTF-8 + virtual terminal mode (which is what makes the colours
 * and the flicker free redraw possible); elsewhere it falls back to POSIX.
 */
class Console {
public:
    /* Prepare the terminal. Safe to call once at start up. */
    static void initialise() {
#ifdef _WIN32
        out_ = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleTitleA("DSA Visualizer  |  Sorting & Searching");

        DWORD mode = 0;
        if (GetConsoleMode(out_, &mode)) {
            isTerminal_    = true;
            originalMode_  = mode;
            richOutput_    = SetConsoleMode(out_, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
            enlargeWindow(110, 42);
        } else {
            /* Output is redirected to a file or a pipe: no cursor tricks. */
            isTerminal_ = false;
            richOutput_ = false;
        }
#else
        isTerminal_ = isatty(STDOUT_FILENO) != 0;
        richOutput_ = isTerminal_;
#endif
        if (richOutput_) hideCursor();
    }

    /* Put the terminal back the way we found it. */
    static void restore() {
        if (richOutput_) {
            showCursor();
            std::cout << "\x1b[0m" << std::flush;
        }
#ifdef _WIN32
        if (isTerminal_) SetConsoleMode(out_, originalMode_);
#else
        disableRawMode();
#endif
    }

    /* True when we may emit ANSI escape sequences and animate in place. */
    static bool richOutput()  { return richOutput_; }
    static bool isTerminal()  { return isTerminal_; }

    static int width() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (isTerminal_ && GetConsoleScreenBufferInfo(out_, &info))
            return info.srWindow.Right - info.srWindow.Left + 1;
#endif
        return 100;
    }

    static int height() {
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (isTerminal_ && GetConsoleScreenBufferInfo(out_, &info))
            return info.srWindow.Bottom - info.srWindow.Top + 1;
#endif
        return 42;
    }

    static void home()            { if (richOutput_) std::cout << "\x1b[H"; }
    static void clearBelow()      { if (richOutput_) std::cout << "\x1b[0J"; }
    static void hideCursor()      { if (richOutput_) std::cout << "\x1b[?25l" << std::flush; }
    static void showCursor()      { if (richOutput_) std::cout << "\x1b[?25h" << std::flush; }
    static void moveTo(int row)   { if (richOutput_) std::cout << "\x1b[" << row << ";1H"; }

    /* Full clear. Uses the current background colour, which is how the
     * light / dark theme paints the whole screen. */
    static void clear() {
        if (richOutput_) std::cout << "\x1b[H\x1b[2J\x1b[3J" << std::flush;
        else             std::cout << "\n\n";
    }

    static void sleepMs(int milliseconds) {
        if (milliseconds <= 0) return;
#ifdef _WIN32
        Sleep(static_cast<DWORD>(milliseconds));
#else
        usleep(static_cast<useconds_t>(milliseconds) * 1000);
#endif
    }

    /* Non blocking check: has the user pressed a key? */
    static bool keyAvailable() {
        if (!isTerminal_) return false;
#ifdef _WIN32
        return _kbhit() != 0;
#else
        enableRawMode();
        timeval timeout; timeout.tv_sec = 0; timeout.tv_usec = 0;
        fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        return select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout) > 0;
#endif
    }

    /* Blocking single key read, returned as a lower case character. */
    static int readKey() {
        if (!isTerminal_) return '\n';
#ifdef _WIN32
        int key = _getch();
        if (key == 0 || key == 224) key = _getch();   // discard arrow key prefix
#else
        enableRawMode();
        int key = getchar();
#endif
        return std::tolower(key);
    }

    /* High resolution wall clock, in microseconds. */
    static double nowMicros() {
#ifdef _WIN32
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        return (static_cast<double>(counter.QuadPart) * 1e6) /
                static_cast<double>(frequency.QuadPart);
#else
        return std::chrono::duration<double, std::micro>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
    }

private:
#ifdef _WIN32
    /*
     * Try to grow the console so the whole bar chart is visible at once.
     *
     * This only ever grows, never shrinks.  The usual trick is to collapse the
     * window to 1x1 first so the buffer can be resized freely - but if the
     * regrow then fails (Windows Terminal does not always honour these calls)
     * the user is left staring at a one character window.  Growing the buffer
     * first and putting it back on failure cannot leave things worse than they
     * started.  Any failure is fine: the visualiser adapts to whatever size it
     * actually gets.
     */
    static void enlargeWindow(int columns, int rows) {
        CONSOLE_SCREEN_BUFFER_INFO before;
        if (!GetConsoleScreenBufferInfo(out_, &before)) return;

        int haveColumns = before.srWindow.Right  - before.srWindow.Left + 1;
        int haveRows    = before.srWindow.Bottom - before.srWindow.Top  + 1;
        if (haveColumns >= columns && haveRows >= rows) return;

        int wantColumns = haveColumns > columns ? haveColumns : columns;
        int wantRows    = haveRows    > rows    ? haveRows    : rows;

        /* The buffer must be at least as large as the window, so widen it
         * first.  Keep the existing scrollback if it is already generous. */
        COORD buffer;
        buffer.X = static_cast<SHORT>(wantColumns);
        buffer.Y = before.dwSize.Y > 400 ? before.dwSize.Y : 400;
        if (!SetConsoleScreenBufferSize(out_, buffer)) return;

        SMALL_RECT window = {0, 0, static_cast<SHORT>(wantColumns - 1),
                                   static_cast<SHORT>(wantRows - 1)};
        if (!SetConsoleWindowInfo(out_, TRUE, &window))
            SetConsoleScreenBufferSize(out_, before.dwSize);   // undo
    }
    static HANDLE out_;
    static DWORD  originalMode_;
#else
    static void enableRawMode() {
        if (rawMode_) return;
        termios settings;
        tcgetattr(STDIN_FILENO, &original_);
        settings = original_;
        settings.c_lflag &= ~(ICANON | ECHO);
        settings.c_cc[VMIN]  = 1;
        settings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &settings);
        rawMode_ = true;
    }
    static void disableRawMode() {
        if (!rawMode_) return;
        tcsetattr(STDIN_FILENO, TCSANOW, &original_);
        rawMode_ = false;
    }
    static termios original_;
    static bool    rawMode_;
#endif
    static bool richOutput_;
    static bool isTerminal_;
};

#ifdef _WIN32
HANDLE  Console::out_          = NULL;
DWORD   Console::originalMode_ = 0;
#else
termios Console::original_ = termios();
bool    Console::rawMode_  = false;
#endif
bool Console::richOutput_ = false;
bool Console::isTerminal_ = false;

/* Restore the console no matter how the program leaves main(). */
class ConsoleGuard {
public:
    ConsoleGuard()  { Console::initialise(); }
    ~ConsoleGuard() { Console::restore(); }
};

/* ===========================================================================
 * SECTION 2 : THEME  (dark / light mode + glyph set)
 * ===========================================================================
 */

/* The role a bar is playing in the current animation frame.  The Visualizer
 * turns these into colours, so an algorithm never mentions a colour itself. */
enum BarRole {
    ROLE_NORMAL,     // untouched element
    ROLE_COMPARE,    // being compared right now
    ROLE_SWAP,       // being swapped / overwritten right now
    ROLE_PIVOT,      // pivot (quick sort) or midpoint (binary search)
    ROLE_ACTIVE,     // cursor / insertion point / boundary
    ROLE_RANGE,      // inside the sub array currently being worked on
    ROLE_SORTED,     // final position reached
    ROLE_DISCARDED,  // ruled out (binary search) or already visited
    ROLE_FOUND       // the search target
};

/* Box drawing and block characters, with a pure ASCII fallback for terminals
 * or fonts that cannot render Unicode. */
struct Glyphs {
    std::string block, axis, vertical, horizontal;
    std::string topLeft, topRight, bottomLeft, bottomRight;
    std::string bannerTL, bannerTR, bannerBL, bannerBR, bannerH, bannerV;
    std::string bullet;
};

static const Glyphs UNICODE_GLYPHS = {
    "█", "─", "│", "─",
    "┌", "┐", "└", "┘",
    "╔", "╗", "╚", "╝", "═", "║",
    "·"
};

static const Glyphs ASCII_GLYPHS = {
    "#", "-", "|", "-",
    "+", "+", "+", "+",
    "+", "+", "+", "+", "=", "|",
    "*"
};

enum ThemeMode { THEME_DARK, THEME_LIGHT };

/*
 * Theme owns the colour palette.  Colours are emitted as 24 bit ANSI escape
 * sequences; when the terminal cannot handle them every accessor returns an
 * empty string, so the same drawing code produces clean plain text.
 */
class Theme {
public:
    Theme() : mode_(THEME_DARK), asciiMode_(false) {}

    void toggleMode()          { mode_ = (mode_ == THEME_DARK) ? THEME_LIGHT : THEME_DARK; }
    ThemeMode mode() const     { return mode_; }
    std::string modeName() const { return mode_ == THEME_DARK ? "Dark" : "Light"; }

    void setAsciiMode(bool on)  { asciiMode_ = on; }
    bool asciiMode() const      { return asciiMode_; }
    const Glyphs& glyphs() const { return asciiMode_ ? ASCII_GLYPHS : UNICODE_GLYPHS; }

    /* Base = normal foreground on the theme background.  Every drawn line
     * starts and ends with this so the background fills the whole screen. */
    std::string base() const {
        if (!Console::richOutput()) return "";
        return foreground(textR(), textG(), textB()) + background();
    }

    std::string background() const {
        if (!Console::richOutput()) return "";
        return mode_ == THEME_DARK ? bg(18, 20, 28) : bg(246, 247, 250);
    }

    std::string reset() const { return Console::richOutput() ? "\x1b[0m" : ""; }

    std::string title()  const { return colour(mode_ == THEME_DARK ? rgb(255, 214, 120) : rgb(150,  92,   0)); }
    std::string accent() const { return colour(mode_ == THEME_DARK ? rgb( 88, 166, 255) : rgb( 20,  86, 200)); }
    std::string dim()    const { return colour(mode_ == THEME_DARK ? rgb(120, 130, 155) : rgb(122, 130, 148)); }
    std::string ok()     const { return colour(mode_ == THEME_DARK ? rgb( 86, 211, 127) : rgb( 20, 132,  68)); }
    std::string warn()   const { return colour(mode_ == THEME_DARK ? rgb(240, 185,  80) : rgb(170, 108,   0)); }
    std::string error()  const { return colour(mode_ == THEME_DARK ? rgb(248, 105, 105) : rgb(198,  40,  40)); }
    std::string text()   const { return colour(rgb(textR(), textG(), textB())); }

    /* Colour for a bar, chosen from the role the algorithm assigned to it. */
    std::string barColour(BarRole role) const {
        if (!Console::richOutput()) return "";
        const bool dark = (mode_ == THEME_DARK);
        switch (role) {
            case ROLE_COMPARE:   return colour(dark ? rgb(255, 214, 102) : rgb(198, 138,   0));
            case ROLE_SWAP:      return colour(dark ? rgb(248, 105, 105) : rgb(203,  48,  48));
            case ROLE_PIVOT:     return colour(dark ? rgb(199, 125, 255) : rgb(138,  58, 198));
            case ROLE_ACTIVE:    return colour(dark ? rgb( 88, 166, 255) : rgb( 22,  88, 200));
            case ROLE_RANGE:     return colour(dark ? rgb(140, 156, 190) : rgb(146, 158, 186));
            case ROLE_SORTED:    return colour(dark ? rgb( 86, 211, 127) : rgb( 24, 138,  72));
            case ROLE_DISCARDED: return colour(dark ? rgb( 58,  64,  82) : rgb(208, 212, 222));
            case ROLE_FOUND:     return colour(dark ? rgb(  0, 226, 132) : rgb(  0, 150,  80));
            case ROLE_NORMAL:
            default:             return colour(dark ? rgb(100, 116, 160) : rgb(118, 134, 172));
        }
    }

    static std::string roleName(BarRole role) {
        switch (role) {
            case ROLE_COMPARE:   return "comparing";
            case ROLE_SWAP:      return "swapping";
            case ROLE_PIVOT:     return "pivot";
            case ROLE_ACTIVE:    return "cursor";
            case ROLE_RANGE:     return "search range";
            case ROLE_SORTED:    return "sorted";
            case ROLE_DISCARDED: return "discarded";
            case ROLE_FOUND:     return "found";
            case ROLE_NORMAL:
            default:             return "unsorted";
        }
    }

private:
    struct Rgb { int r, g, b; };
    static Rgb rgb(int r, int g, int b) { Rgb c = {r, g, b}; return c; }

    static std::string foreground(int r, int g, int b) {
        std::ostringstream out;
        out << "\x1b[38;2;" << r << ";" << g << ";" << b << "m";
        return out.str();
    }
    static std::string bg(int r, int g, int b) {
        std::ostringstream out;
        out << "\x1b[48;2;" << r << ";" << g << ";" << b << "m";
        return out.str();
    }
    std::string colour(Rgb c) const {
        if (!Console::richOutput()) return "";
        return foreground(c.r, c.g, c.b);
    }
    int textR() const { return mode_ == THEME_DARK ? 222 : 26; }
    int textG() const { return mode_ == THEME_DARK ? 226 : 30; }
    int textB() const { return mode_ == THEME_DARK ? 234 : 40; }

    ThemeMode mode_;
    bool      asciiMode_;
};

/* ===========================================================================
 * SECTION 3 : INPUT VALIDATION
 * ===========================================================================
 * Nothing in this program trusts the keyboard.  Every read goes through these
 * helpers, which re-prompt until the input is genuinely usable.
 */

/* Thrown when the input stream closes (Ctrl+Z / Ctrl+D or a piped script). */
struct ExitApplication {};

class InputReader {
public:
    explicit InputReader(const Theme& theme) : theme_(theme) {}

    /* Read a whole line. Throws ExitApplication if the stream is exhausted. */
    std::string readLine(const std::string& prompt) const {
        Console::showCursor();
        std::cout << theme_.base() << theme_.accent() << prompt << theme_.text() << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) throw ExitApplication();
        Console::hideCursor();
        return util::trim(line);
    }

    /* Read an integer inside [minimum, maximum], re-prompting on bad input. */
    int readInt(const std::string& prompt, int minimum, int maximum) const {
        while (true) {
            std::string line = readLine(prompt);
            if (line.empty()) { complain("Input cannot be empty."); continue; }

            int value = 0;
            if (!parseInt(line, value)) {
                complain("\"" + line + "\" is not a whole number. Digits only, please.");
                continue;
            }
            if (value < minimum || value > maximum) {
                std::ostringstream message;
                message << "Value must be between " << minimum << " and " << maximum
                        << ". You entered " << value << ".";
                complain(message.str());
                continue;
            }
            return value;
        }
    }

    /* Menu choice: same as readInt but phrased for menus. */
    int readChoice(int minimum, int maximum) const {
        while (true) {
            std::string line = readLine("  Your choice > ");
            if (line.empty()) { complain("Please type the number of a menu option."); continue; }
            int value = 0;
            if (!parseInt(line, value)) {
                complain("\"" + line + "\" is not a valid option. Type one of the numbers shown.");
                continue;
            }
            if (value < minimum || value > maximum) {
                std::ostringstream message;
                message << "Option " << value << " does not exist. Choose between "
                        << minimum << " and " << maximum << ".";
                complain(message.str());
                continue;
            }
            return value;
        }
    }

    bool readYesNo(const std::string& prompt, bool defaultAnswer) const {
        while (true) {
            std::string line = util::toUpper(readLine(prompt));
            if (line.empty())                 return defaultAnswer;
            if (line == "Y" || line == "YES") return true;
            if (line == "N" || line == "NO")  return false;
            complain("Please answer with 'y' or 'n'.");
        }
    }

    /*
     * Read a whole array in one go: "12 45 7" or "12,45,7".
     * Validates the element count and every individual value, and reports the
     * exact token that failed so the user knows what to fix.
     */
    std::vector<int> readArray(int minCount, int maxCount,
                               int minValue, int maxValue) const {
        while (true) {
            std::ostringstream prompt;
            prompt << "  Enter " << minCount << " to " << maxCount
                   << " values (" << minValue << "-" << maxValue << "), separated by spaces > ";
            std::string line = readLine(prompt.str());
            if (line.empty()) { complain("No values entered."); continue; }

            /* Commas are accepted as separators too. */
            for (size_t i = 0; i < line.size(); ++i)
                if (line[i] == ',' || line[i] == ';') line[i] = ' ';

            std::istringstream stream(line);
            std::vector<int> values;
            std::string token;
            bool valid = true;

            while (stream >> token) {
                int value = 0;
                if (!parseInt(token, value)) {
                    complain("\"" + token + "\" is not a whole number.");
                    valid = false;
                    break;
                }
                if (value < minValue || value > maxValue) {
                    std::ostringstream message;
                    message << "Value " << value << " is out of range ("
                            << minValue << "-" << maxValue << ").";
                    complain(message.str());
                    valid = false;
                    break;
                }
                if (static_cast<int>(values.size()) >= maxCount) {
                    std::ostringstream message;
                    message << "Too many values - the maximum is " << maxCount << ".";
                    complain(message.str());
                    valid = false;
                    break;
                }
                values.push_back(value);
            }
            if (!valid) continue;

            if (static_cast<int>(values.size()) < minCount) {
                std::ostringstream message;
                message << "Only " << values.size() << " value(s) entered - at least "
                        << minCount << " are needed.";
                complain(message.str());
                continue;
            }
            return values;
        }
    }

    void pause(const std::string& message = "  Press Enter to continue . . . ") const {
        std::cout << theme_.base() << theme_.dim() << message << theme_.text() << std::flush;
        std::string discard;
        if (!std::getline(std::cin, discard)) throw ExitApplication();
    }

private:
    /*
     * Strict integer parsing.  std::stoi is deliberately not used because it
     * happily accepts "12abc"; here the entire token must be consumed.
     */
    static bool parseInt(const std::string& text, int& result) {
        if (text.empty()) return false;
        size_t index = 0;
        bool negative = false;
        if (text[0] == '+' || text[0] == '-') {
            negative = (text[0] == '-');
            index = 1;
            if (text.size() == 1) return false;
        }
        long long value = 0;
        for (; index < text.size(); ++index) {
            if (!std::isdigit(static_cast<unsigned char>(text[index]))) return false;
            value = value * 10 + (text[index] - '0');
            if (value > 2147483647LL) return false;          // overflow guard
        }
        result = static_cast<int>(negative ? -value : value);
        return true;
    }

    void complain(const std::string& message) const {
        std::cout << theme_.base() << theme_.error() << "  [!] " << message
                  << theme_.text() << "\n";
    }

    const Theme& theme_;
};

/* ===========================================================================
 * SECTION 4 : STATISTICS
 * ===========================================================================
 * A small, fully encapsulated counter object.  Each algorithm owns one and
 * updates it as it works; the Visualizer only ever reads from it.
 */
class Statistics {
public:
    Statistics() { reset(); }

    void reset() {
        comparisons_ = 0;
        swaps_       = 0;
        writes_      = 0;
        pass_        = 0;
        totalPasses_ = 0;
        depth_       = 0;
        maxDepth_    = 0;
        microseconds_= 0.0;
    }

    void countComparison()      { ++comparisons_; }
    void countSwap()            { ++swaps_; writes_ += 2; }   // a swap writes 2 slots
    void countShift()           { ++swaps_; }                 // a shift displaces 1 element
    void countWrite()           { ++writes_; }
    void setPass(int current, int total) { pass_ = current; totalPasses_ = total; }
    void setMicroseconds(double value)   { microseconds_ = value; }

    void enterRecursion() {
        ++depth_;
        if (depth_ > maxDepth_) maxDepth_ = depth_;
    }
    void exitRecursion() { --depth_; }

    long long comparisons() const { return comparisons_; }
    long long swaps()       const { return swaps_; }
    long long writes()      const { return writes_; }
    int  pass()             const { return pass_; }
    int  totalPasses()      const { return totalPasses_; }
    int  depth()            const { return depth_; }
    int  maxDepth()         const { return maxDepth_; }
    double microseconds()   const { return microseconds_; }

private:
    long long comparisons_;
    long long swaps_;
    long long writes_;
    int       pass_;
    int       totalPasses_;
    int       depth_;
    int       maxDepth_;
    double    microseconds_;
};

/* ===========================================================================
 * SECTION 5 : VISUALIZER
 * ===========================================================================
 * The drawing engine.  It knows how to paint one animation frame - the bar
 * chart, the status line, the legend and the live statistics panel - and how
 * to pace the animation (speed control, pause, single step).
 *
 * Algorithms talk to it through exactly one call: frame(...).
 */

/* Thrown when the user presses Q to abandon a running animation. */
struct AbortVisualization {};

/* One entry of the per algorithm colour legend. */
struct LegendEntry {
    BarRole     role;
    std::string label;
    LegendEntry(BarRole r, const std::string& l) : role(r), label(l) {}
};

/* Roles for the current frame, built with a small fluent helper so the
 * algorithms stay readable. */
class RoleMap {
public:
    explicit RoleMap(size_t size, BarRole fill = ROLE_NORMAL)
        : roles_(size, fill) {}

    RoleMap& set(int index, BarRole role) {
        if (index >= 0 && index < static_cast<int>(roles_.size()))
            roles_[static_cast<size_t>(index)] = role;
        return *this;
    }
    RoleMap& setRange(int from, int to, BarRole role) {
        for (int i = from; i <= to; ++i) set(i, role);
        return *this;
    }
    /* Mark every index outside [from, to] with a role (used by binary search). */
    RoleMap& setOutside(int from, int to, BarRole role) {
        for (int i = 0; i < static_cast<int>(roles_.size()); ++i)
            if (i < from || i > to) roles_[static_cast<size_t>(i)] = role;
        return *this;
    }
    const std::vector<BarRole>& get() const { return roles_; }

private:
    std::vector<BarRole> roles_;
};

class Visualizer {
public:
    explicit Visualizer(Theme& theme)
        : theme_(theme), silent_(false), stepMode_(false),
          speedIndex_(kDefaultSpeed), controlsRow_(0), aborted_(false) {}

    /* ---- configuration ------------------------------------------------- */
    void setSilent(bool silent)   { silent_ = silent; }
    bool silent() const           { return silent_; }

    /* True only when frames will actually reach the screen.  Algorithms test
     * this before composing any status text, because formatting a message is
     * far more expensive than the sorting step it describes - and the timing
     * runs must measure the algorithm, not the narration. */
    bool animating() const        { return !silent_ && Console::richOutput(); }

    void setStepMode(bool on)     { stepMode_ = on; }
    bool stepMode() const         { return stepMode_; }

    int  speedIndex() const       { return speedIndex_; }
    void setSpeedIndex(int index) {
        if (index < 0) index = 0;
        if (index >= kSpeedCount) index = kSpeedCount - 1;
        speedIndex_ = index;
    }
    static int speedCount()                 { return kSpeedCount; }
    static std::string speedName(int index) { return kSpeeds[index].name; }
    static int speedDelay(int index)        { return kSpeeds[index].delayMs; }
    std::string currentSpeedName() const    { return kSpeeds[speedIndex_].name; }
    int currentDelay() const                { return kSpeeds[speedIndex_].delayMs; }

    /* ---- lifecycle of one animated run --------------------------------- */
    void beginRun(const std::string& algorithmName,
                  const std::string& subtitle,
                  const std::string& complexity,
                  const std::vector<LegendEntry>& legend) {
        algorithmName_ = algorithmName;
        subtitle_      = subtitle;
        complexity_    = complexity;
        legend_        = legend;
        aborted_       = false;
        if (!silent_ && Console::richOutput()) {
            std::cout << theme_.base();
            Console::clear();
        }
    }

    bool aborted() const { return aborted_; }

    /*
     * Draw one frame and then wait according to the current speed setting.
     * This is the single entry point used by every algorithm.
     */
    void frame(const std::vector<int>& data,
               const std::vector<BarRole>& roles,
               const std::string& status,
               const Statistics& stats) {
        if (silent_ || !Console::richOutput()) return;
        render(data, roles, status, stats);
        waitForNextFrame();
    }

    /* Draw a frame and hold it a little longer - used for key moments. */
    void emphasisFrame(const std::vector<int>& data,
                       const std::vector<BarRole>& roles,
                       const std::string& status,
                       const Statistics& stats,
                       int extraMilliseconds) {
        if (silent_ || !Console::richOutput()) return;
        render(data, roles, status, stats);
        waitForNextFrame();
        if (!stepMode_) interruptibleSleep(extraMilliseconds);
    }

private:
    /* ---- layout -------------------------------------------------------- */
    struct Layout {
        int barWidth;    // columns of block characters per bar
        int gap;         // blank columns between bars
        int cell;        // barWidth + gap
        int height;      // number of rows the chart is tall
        int chartWidth;  // total columns the chart occupies
        int panelWidth;  // width of the header / statistics boxes
    };

    Layout computeLayout(size_t count) const {
        Layout layout;
        int available = Console::width() - 4;
        if (available < 40) available = 40;
        if (available > 118) available = 118;

        int n = static_cast<int>(count);
        if      (n * 4 <= available) { layout.barWidth = 3; layout.gap = 1; }
        else if (n * 3 <= available) { layout.barWidth = 2; layout.gap = 1; }
        else                         { layout.barWidth = 1; layout.gap = 1; }

        layout.cell       = layout.barWidth + layout.gap;
        layout.chartWidth = n * layout.cell;

        /*
         * A frame is the chart plus kFixedRows of furniture (banner, status,
         * axis, labels, legend, statistics panel, controls).  Deriving the
         * chart height from the console height - rather than from a table of
         * guessed thresholds - guarantees the frame always fits, so it never
         * scrolls and the animation never jitters.
         */
        layout.height = Console::height() - kFixedRows;
        if (layout.height < 4)  layout.height = 4;
        if (layout.height > 14) layout.height = 14;

        /* The panel sits inside a 2 column margin and adds its own two corner
         * characters, so it must stay 4 columns clear of the usable width. */
        layout.panelWidth = layout.chartWidth + 2;
        if (layout.panelWidth < 62)            layout.panelWidth = 62;
        if (layout.panelWidth > available - 4) layout.panelWidth = available - 4;
        return layout;
    }

    /* ---- frame construction -------------------------------------------- */
    void render(const std::vector<int>& data,
                const std::vector<BarRole>& roles,
                const std::string& status,
                const Statistics& stats) {
        const Glyphs& g = theme_.glyphs();
        Layout layout   = computeLayout(data.size());

        /* Scale every value to a bar height in rows. */
        int maxValue = 1;
        for (size_t i = 0; i < data.size(); ++i)
            if (data[i] > maxValue) maxValue = data[i];

        std::vector<int> heights(data.size(), 0);
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] <= 0) { heights[i] = 0; continue; }
            double scaled = (static_cast<double>(data[i]) * layout.height) / maxValue;
            int rows = static_cast<int>(scaled + 0.5);
            heights[i] = rows < 1 ? 1 : rows;
        }

        std::ostringstream out;
        int row = 0;
        const std::string margin = "  ";

        /* --- banner --- */
        emit(out, row, margin + theme_.accent() + g.bannerTL +
                       util::repeat(g.bannerH, layout.panelWidth) + g.bannerTR);
        {
            std::string heading = " DSA VISUALIZER " + g.bullet + " " + algorithmName_;
            if (!subtitle_.empty()) heading += "  (" + subtitle_ + ")";
            emit(out, row, margin + theme_.accent() + g.bannerV + theme_.title() +
                           util::padRight(heading, layout.panelWidth) +
                           theme_.accent() + g.bannerV);
        }
        emit(out, row, margin + theme_.accent() + g.bannerBL +
                       util::repeat(g.bannerH, layout.panelWidth) + g.bannerBR);
        emit(out, row, "");

        /* --- status line: what the algorithm is doing this instant --- */
        emit(out, row, margin + theme_.text() + status);
        emit(out, row, "");

        /* --- the bar chart, drawn top row first --- */
        for (int line = layout.height; line >= 1; --line) {
            std::string rendered = margin;
            for (size_t i = 0; i < data.size(); ++i) {
                if (heights[i] >= line)
                    rendered += theme_.barColour(roles[i]) +
                                util::repeat(g.block, layout.barWidth);
                else
                    rendered += std::string(static_cast<size_t>(layout.barWidth), ' ');
                rendered += std::string(static_cast<size_t>(layout.gap), ' ');
            }
            emit(out, row, rendered);
        }

        /* --- axis, then the value of each bar, then its index --- */
        emit(out, row, margin + theme_.dim() + util::repeat(g.axis, layout.chartWidth));

        std::string valueRow = margin;
        for (size_t i = 0; i < data.size(); ++i) {
            valueRow += theme_.barColour(roles[i]) +
                        util::padCentre(std::to_string(data[i]), layout.barWidth) +
                        std::string(static_cast<size_t>(layout.gap), ' ');
        }
        emit(out, row, valueRow);

        std::string indexRow = margin + theme_.dim();
        for (size_t i = 0; i < data.size(); ++i) {
            indexRow += util::padCentre(std::to_string(i), layout.barWidth) +
                        std::string(static_cast<size_t>(layout.gap), ' ');
        }
        emit(out, row, indexRow);
        emit(out, row, "");

        /*
         * --- legend ---
         * Wrapped across a fixed two rows.  A legend that ran past the console
         * width would soft wrap and push everything below it down by a line,
         * which would make the whole animation jitter; emitting a constant
         * number of rows keeps every frame exactly the same height.
         */
        {
            std::vector<std::string> legendRows;
            std::string current      = margin;
            int         currentWidth = 0;

            for (size_t i = 0; i < legend_.size(); ++i) {
                int entryWidth = 2 + 1 + util::displayWidth(legend_[i].label) + 3;
                if (currentWidth > 0 && currentWidth + entryWidth > layout.panelWidth) {
                    legendRows.push_back(current);
                    current      = margin;
                    currentWidth = 0;
                }
                current += theme_.barColour(legend_[i].role) + util::repeat(g.block, 2) +
                           theme_.dim() + " " + legend_[i].label + "   ";
                currentWidth += entryWidth;
            }
            legendRows.push_back(current);

            for (size_t i = 0; i < kLegendRows; ++i)
                emit(out, row, i < legendRows.size() ? legendRows[i] : "");
        }
        emit(out, row, "");

        /* --- live statistics panel --- */
        emit(out, row, margin + theme_.dim() + g.topLeft +
                       util::repeat(g.horizontal, layout.panelWidth) + g.topRight);
        {
            std::ostringstream left;
            left << " Comparisons : " << util::padRight(util::withCommas(stats.comparisons()), 10)
                 << "  Swaps : "      << util::padRight(util::withCommas(stats.swaps()), 10)
                 << "  Writes : "     << util::withCommas(stats.writes());
            emit(out, row, margin + theme_.dim() + g.vertical + theme_.text() +
                           util::padRight(left.str(), layout.panelWidth) +
                           theme_.dim() + g.vertical);

            std::ostringstream right;
            right << " Time  : " << util::padRight(complexity_, 22);
            if (stats.totalPasses() > 0)
                right << " Pass : " << stats.pass() << " / " << stats.totalPasses();
            else if (stats.maxDepth() > 0)
                right << " Recursion depth : " << stats.depth()
                      << " (max " << stats.maxDepth() << ")";
            emit(out, row, margin + theme_.dim() + g.vertical + theme_.text() +
                           util::padRight(right.str(), layout.panelWidth) +
                           theme_.dim() + g.vertical);
        }
        emit(out, row, margin + theme_.dim() + g.bottomLeft +
                       util::repeat(g.horizontal, layout.panelWidth) + g.bottomRight);
        emit(out, row, "");

        /* --- control hints (this row is rewritten in place when paused) --- */
        controlsRow_ = row + 1;
        emit(out, row, controlsLine());

        Console::home();
        std::cout << out.str();
        Console::clearBelow();
        std::cout << std::flush;
    }

    /* Write one screen line: theme background, content, clear to end of line. */
    void emit(std::ostringstream& out, int& row, const std::string& content) const {
        out << theme_.base() << content << theme_.base();
        if (Console::richOutput()) out << "\x1b[K";
        out << "\n";
        ++row;
    }

    std::string controlsLine() const {
        std::ostringstream out;
        out << "  " << theme_.dim() << "[Space]" << theme_.text() << " pause   "
            << theme_.dim() << "[+/-]"   << theme_.text() << " speed   "
            << theme_.dim() << "[S]"     << theme_.text() << " step mode   "
            << theme_.dim() << "[Q]"     << theme_.text() << " abort"
            << theme_.dim() << "        Speed: " << theme_.accent() << currentSpeedName()
            << theme_.dim() << " (" << currentDelay() << " ms)"
            << (stepMode_ ? std::string("  ") + theme_.warn() + "[STEP MODE]" : std::string());
        return out.str();
    }

    /* ---- pacing and interactive controls -------------------------------- */
    void waitForNextFrame() {
        drainKeys();
        if (stepMode_) {
            showBanner(theme_.warn() + "  STEP MODE " + theme_.dim() +
                       " - press any key for the next step, [S] to resume, [Q] to abort");
            handleKey(Console::readKey());
            return;
        }
        interruptibleSleep(currentDelay());
    }

    /* Sleep in short slices so key presses feel instant. */
    void interruptibleSleep(int milliseconds) {
        int remaining = milliseconds;
        while (remaining > 0) {
            drainKeys();
            if (stepMode_) return;              // user switched to stepping
            int slice = remaining < 12 ? remaining : 12;
            Console::sleepMs(slice);
            remaining -= slice;
        }
        drainKeys();
    }

    void drainKeys() {
        while (Console::keyAvailable()) handleKey(Console::readKey());
    }

    void handleKey(int key) {
        switch (key) {
            case 'q':
                aborted_ = true;
                throw AbortVisualization();
            case ' ':
                pauseUntilResumed();
                break;
            case '+': case '=':
                setSpeedIndex(speedIndex_ + 1);
                showBanner("");
                break;
            case '-': case '_':
                setSpeedIndex(speedIndex_ - 1);
                showBanner("");
                break;
            case 's':
                stepMode_ = !stepMode_;
                showBanner("");
                break;
            default:
                break;
        }
    }

    void pauseUntilResumed() {
        showBanner(theme_.warn() + "  PAUSED " + theme_.dim() +
                   " - [Space] resume   [S] step mode   [Q] abort");
        while (true) {
            int key = Console::readKey();
            if (key == ' ') { showBanner(""); return; }
            if (key == 'q') { aborted_ = true; throw AbortVisualization(); }
            if (key == 's') { stepMode_ = true; showBanner(""); return; }
            if (key == '+' || key == '=') { setSpeedIndex(speedIndex_ + 1);
                showBanner(theme_.warn() + "  PAUSED " + theme_.dim() + " - speed now " + currentSpeedName()); }
            if (key == '-' || key == '_') { setSpeedIndex(speedIndex_ - 1);
                showBanner(theme_.warn() + "  PAUSED " + theme_.dim() + " - speed now " + currentSpeedName()); }
        }
    }

    /* Rewrite just the controls row without redrawing the whole frame. */
    void showBanner(const std::string& override) {
        if (!Console::richOutput() || controlsRow_ <= 0) return;
        Console::moveTo(controlsRow_);
        std::cout << theme_.base()
                  << (override.empty() ? controlsLine() : override)
                  << theme_.base() << "\x1b[K" << std::flush;
    }

    /* ---- constants ------------------------------------------------------ */
    static const size_t kLegendRows = 2;   // fixed, so the frame never shifts

    /* Rows a frame uses for everything except the bars themselves:
     * 3 banner + 1 blank + 1 status + 1 blank + 1 axis + 1 values + 1 indices
     * + 1 blank + 2 legend + 1 blank + 4 statistics box + 1 blank + 1 controls
     * = 20, plus one spare line so the cursor never forces a scroll. */
    static const int kFixedRows = 21;

    /* ---- speed table ---------------------------------------------------- */
    struct SpeedSetting { const char* name; int delayMs; };
    static const int kSpeedCount   = 6;
    static const int kDefaultSpeed = 2;
    static const SpeedSetting kSpeeds[kSpeedCount];

    Theme&                   theme_;
    bool                     silent_;
    bool                     stepMode_;
    int                      speedIndex_;
    int                      controlsRow_;
    bool                     aborted_;
    std::string              algorithmName_;
    std::string              subtitle_;
    std::string              complexity_;
    std::vector<LegendEntry> legend_;
};

const int    Visualizer::kSpeedCount;
const int    Visualizer::kDefaultSpeed;
const int    Visualizer::kFixedRows;
const size_t Visualizer::kLegendRows;
const Visualizer::SpeedSetting Visualizer::kSpeeds[Visualizer::kSpeedCount] = {
    { "Very Slow", 700 },
    { "Slow",      330 },
    { "Normal",    150 },
    { "Fast",       65 },
    { "Very Fast",  22 },
    { "Instant",     0 }
};

/* ===========================================================================
 * SECTION 6 : ALGORITHM HIERARCHY  (inheritance + polymorphism)
 * ===========================================================================
 *
 *                       Algorithm  (abstract)
 *                      /                    \
 *          SortingAlgorithm            SearchingAlgorithm
 *          /   /    |     \   \             /        \
 *    Bubble Selection Insertion Merge Quick   Linear  Binary
 */

/* Complexity facts about one algorithm - shown live and in the chart. */
struct Complexity {
    std::string best, average, worst, space, stability, note;
    Complexity() {}
    Complexity(const std::string& b, const std::string& a, const std::string& w,
               const std::string& s, const std::string& st, const std::string& n)
        : best(b), average(a), worst(w), space(s), stability(st), note(n) {}
};

class Algorithm {
public:
    explicit Algorithm(Visualizer& visualizer) : viz_(visualizer) {}
    virtual ~Algorithm() {}

    /* Pure virtual interface - this is what makes the menu polymorphic. */
    virtual std::string name()        const = 0;
    virtual std::string category()    const = 0;
    virtual Complexity  complexity()  const = 0;
    virtual std::string description() const = 0;

    const Statistics& stats() const { return stats_; }

protected:
    /* Is the animation actually on screen right now? */
    bool animating() const { return viz_.animating(); }

    /*
     * Run `build` only when the animation is visible.  Every frame in this
     * program is wrapped in this, so a silent run does no drawing work at
     * all - which is what makes the measured execution times meaningful.
     */
    template <typename BuildFrame>
    void animate(const BuildFrame& build) {
        if (viz_.animating()) build();
    }

    /* Measure one silent run of `work`, averaged over enough repetitions to
     * beat the clock resolution.  The cost of resetting the input between
     * repetitions is measured separately and subtracted, so the number really
     * is the algorithm's own running time. */
    static double measureMicros(const std::function<void()>& setup,
                                const std::function<void()>& work) {
        int repetitions = 1;
        double elapsed  = 0.0;
        while (repetitions < (1 << 20)) {
            double start = Console::nowMicros();
            for (int i = 0; i < repetitions; ++i) { setup(); work(); }
            elapsed = Console::nowMicros() - start;
            if (elapsed >= 20000.0) break;          // 20 ms of samples is plenty
            repetitions *= 2;
        }
        double startBaseline = Console::nowMicros();
        for (int i = 0; i < repetitions; ++i) setup();
        double baseline = Console::nowMicros() - startBaseline;

        double perRun = (elapsed - baseline) / repetitions;
        return perRun > 0.0 ? perRun : 0.0;
    }

    Visualizer& viz_;
    Statistics  stats_;
};

/* --------------------------------------------------------------------------
 * SortingAlgorithm - shared behaviour for all five sorts.
 *
 * run() is a template method: it fixes the sequence of events (animate,
 * verify, re-run silently for accurate stats, time it) while each concrete
 * sort supplies only its own sort() body.
 * -------------------------------------------------------------------------- */
class SortingAlgorithm : public Algorithm {
public:
    explicit SortingAlgorithm(Visualizer& visualizer) : Algorithm(visualizer) {}

    std::string category() const { return "Sorting"; }

    /* Legend shown beneath the chart. Sorts may override to add a pivot key. */
    virtual std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_NORMAL,  "unsorted"));
        entries.push_back(LegendEntry(ROLE_COMPARE, "comparing"));
        entries.push_back(LegendEntry(ROLE_SWAP,    "swapping"));
        entries.push_back(LegendEntry(ROLE_SORTED,  "final place"));
        return entries;
    }

    /* Sort `data` in place, animating the process. Returns false if the user
     * aborted the animation (the array is still correctly sorted). */
    bool run(std::vector<int>& data) {
        std::vector<int> original = data;
        bool completed = true;

        /* 1. The animated run the user actually watches. */
        stats_.reset();
        viz_.setSilent(false);
        viz_.beginRun(name(), category(), complexity().average, legend());
        try {
            sort(data);
            finalSweep(data);
        } catch (const AbortVisualization&) {
            completed = false;
        }

        /* 2. A silent re-run gives the definitive operation counts (and
         *    repairs the array if the animation was aborted half way). */
        data = original;
        stats_.reset();
        viz_.setSilent(true);
        sort(data);

        /* 3. Time it properly, then restore the visualiser for next time. */
        Statistics snapshot = stats_;
        std::vector<int> scratch;
        double micros = measureMicros(
            [&]() { scratch = original; },
            [&]() { sort(scratch); });
        viz_.setSilent(false);

        stats_ = snapshot;
        stats_.setMicroseconds(micros);
        return completed;
    }

    /* Sort without any animation - used by the benchmark screen. */
    void runSilently(std::vector<int>& data) {
        stats_.reset();
        bool wasSilent = viz_.silent();
        viz_.setSilent(true);
        sort(data);
        viz_.setSilent(wasSilent);
    }

protected:
    virtual void sort(std::vector<int>& data) = 0;

    /* Counting wrappers - every comparison and swap in this program goes
     * through one of these, which is why the statistics are exact. */
    bool greater(const std::vector<int>& data, int left, int right) {
        stats_.countComparison();
        return data[static_cast<size_t>(left)] > data[static_cast<size_t>(right)];
    }
    bool greaterThanValue(const std::vector<int>& data, int index, int value) {
        stats_.countComparison();
        return data[static_cast<size_t>(index)] > value;
    }
    bool lessOrEqualValue(const std::vector<int>& data, int index, int value) {
        stats_.countComparison();
        return data[static_cast<size_t>(index)] <= value;
    }
    void swapAt(std::vector<int>& data, int left, int right) {
        std::swap(data[static_cast<size_t>(left)], data[static_cast<size_t>(right)]);
        stats_.countSwap();
    }
    void writeAt(std::vector<int>& data, int index, int value) {
        data[static_cast<size_t>(index)] = value;
        stats_.countWrite();
    }

    /* Closing animation: sweep left to right turning every bar green. */
    void finalSweep(const std::vector<int>& data) {
        if (!animating()) return;
        int n = static_cast<int>(data.size());
        for (int i = 0; i <= n; ++i) {
            RoleMap roles(data.size(), ROLE_NORMAL);
            roles.setRange(0, i - 1, ROLE_SORTED);
            std::ostringstream status;
            status << "Verifying result . . .  " << i << " / " << n << " elements confirmed in place";
            viz_.frame(data, roles.get(), status.str(), stats_);
        }
    }
};

/* --------------------------------------------------------------------------
 * SearchingAlgorithm - shared behaviour for the two searches.
 * -------------------------------------------------------------------------- */
class SearchingAlgorithm : public Algorithm {
public:
    explicit SearchingAlgorithm(Visualizer& visualizer) : Algorithm(visualizer) {}

    std::string category() const { return "Searching"; }

    /* Binary search overrides this to demand a sorted array. */
    virtual bool requiresSortedInput() const { return false; }

    virtual std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_NORMAL,    "not checked"));
        entries.push_back(LegendEntry(ROLE_COMPARE,   "checking"));
        entries.push_back(LegendEntry(ROLE_DISCARDED, "ruled out"));
        entries.push_back(LegendEntry(ROLE_FOUND,     "found"));
        return entries;
    }

    /* Returns the index of `target`, or -1 if it is not present. */
    int run(const std::vector<int>& data, int target) {
        int result = -1;
        stats_.reset();
        viz_.setSilent(false);
        viz_.beginRun(name(), category(), complexity().average, legend());
        try {
            result = search(data, target);
            showOutcome(data, target, result);
        } catch (const AbortVisualization&) {
            /* Recompute quietly so the reported answer is still correct. */
        }

        stats_.reset();
        viz_.setSilent(true);
        result = search(data, target);

        Statistics snapshot = stats_;
        double micros = measureMicros(
            [&]() {},
            [&]() { search(data, target); });
        viz_.setSilent(false);

        stats_ = snapshot;
        stats_.setMicroseconds(micros);
        return result;
    }

protected:
    virtual int search(const std::vector<int>& data, int target) = 0;

    bool equals(const std::vector<int>& data, int index, int target) {
        stats_.countComparison();
        return data[static_cast<size_t>(index)] == target;
    }
    bool lessThanTarget(const std::vector<int>& data, int index, int target) {
        stats_.countComparison();
        return data[static_cast<size_t>(index)] < target;
    }

    /* Hold the final frame so the user can see where the target was. */
    void showOutcome(const std::vector<int>& data, int target, int index) {
        if (!animating()) return;
        RoleMap roles(data.size(), ROLE_DISCARDED);
        std::ostringstream status;
        if (index >= 0) {
            roles.set(index, ROLE_FOUND);
            status << "FOUND: target " << target << " sits at index " << index
                   << " after " << stats_.comparisons() << " comparison(s).";
        } else {
            status << "NOT FOUND: " << target << " does not occur in the array ("
                   << stats_.comparisons() << " comparison(s) made).";
        }
        viz_.emphasisFrame(data, roles.get(), status.str(), stats_, 900);
    }
};

/* ===========================================================================
 * SECTION 7 : THE FIVE SORTING ALGORITHMS
 * ===========================================================================
 */

/* ---------------------------------------------------------------- BUBBLE -- */
class BubbleSort : public SortingAlgorithm {
public:
    explicit BubbleSort(Visualizer& v) : SortingAlgorithm(v) {}

    std::string name() const { return "Bubble Sort"; }
    Complexity complexity() const {
        return Complexity("O(n)", "O(n^2)", "O(n^2)", "O(1)", "Stable",
                          "Adjacent swaps; ends early if a pass makes no swap.");
    }
    std::string description() const {
        return "Repeatedly walks the array swapping neighbours that are out of "
               "order, so the largest value 'bubbles' to the end on every pass.";
    }

protected:
    void sort(std::vector<int>& data) {
        int n = static_cast<int>(data.size());
        for (int pass = 0; pass < n - 1; ++pass) {
            stats_.setPass(pass + 1, n - 1);
            bool swappedThisPass = false;

            for (int j = 0; j < n - pass - 1; ++j) {
                animate([&]() {
                    RoleMap roles(data.size());
                    roles.setRange(n - pass, n - 1, ROLE_SORTED)
                         .set(j, ROLE_COMPARE).set(j + 1, ROLE_COMPARE);
                    std::ostringstream status;
                    status << "Pass " << (pass + 1) << " of " << (n - 1)
                           << ":  comparing A[" << j << "]=" << data[j]
                           << " with A[" << (j + 1) << "]=" << data[j + 1];
                    viz_.frame(data, roles.get(), status.str(), stats_);
                });

                if (greater(data, j, j + 1)) {
                    /* The same frame is shown before and after the swap, so
                     * the two bars visibly exchange places. */
                    RoleMap     swapping(animating() ? data.size() : 0);
                    std::string message;
                    animate([&]() {
                        swapping.setRange(n - pass, n - 1, ROLE_SORTED)
                                .set(j, ROLE_SWAP).set(j + 1, ROLE_SWAP);
                        std::ostringstream status;
                        status << "Pass " << (pass + 1) << " of " << (n - 1)
                               << ":  " << data[j] << " > " << data[j + 1]
                               << "  ->  swapping A[" << j << "] and A[" << (j + 1) << "]";
                        message = status.str();
                        viz_.frame(data, swapping.get(), message, stats_);
                    });

                    swapAt(data, j, j + 1);
                    swappedThisPass = true;

                    animate([&]() { viz_.frame(data, swapping.get(), message, stats_); });
                }
            }

            /* Optimisation: a pass with no swaps means the array is sorted. */
            if (!swappedThisPass) {
                animate([&]() {
                    RoleMap roles(data.size(), ROLE_SORTED);
                    std::ostringstream status;
                    status << "Pass " << (pass + 1) << " completed with zero swaps"
                           << "  ->  the array is already ordered, stopping early.";
                    viz_.emphasisFrame(data, roles.get(), status.str(), stats_, 700);
                });
                return;
            }
        }
    }
};

/* ------------------------------------------------------------- SELECTION -- */
class SelectionSort : public SortingAlgorithm {
public:
    explicit SelectionSort(Visualizer& v) : SortingAlgorithm(v) {}

    std::string name() const { return "Selection Sort"; }
    Complexity complexity() const {
        return Complexity("O(n^2)", "O(n^2)", "O(n^2)", "O(1)", "Not stable",
                          "Always n(n-1)/2 comparisons, but at most n-1 swaps.");
    }
    std::string description() const {
        return "Scans the unsorted part for the smallest remaining value and "
               "swaps it into the boundary position - one exact swap per pass.";
    }

    std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_NORMAL,  "unsorted"));
        entries.push_back(LegendEntry(ROLE_COMPARE, "scanning"));
        entries.push_back(LegendEntry(ROLE_PIVOT,   "min so far"));
        entries.push_back(LegendEntry(ROLE_ACTIVE,  "boundary"));
        entries.push_back(LegendEntry(ROLE_SORTED,  "final place"));
        return entries;
    }

protected:
    void sort(std::vector<int>& data) {
        int n = static_cast<int>(data.size());
        for (int i = 0; i < n - 1; ++i) {
            stats_.setPass(i + 1, n - 1);
            int minimumIndex = i;

            for (int j = i + 1; j < n; ++j) {
                animate([&]() {
                    RoleMap roles(data.size());
                    roles.setRange(0, i - 1, ROLE_SORTED)
                         .set(i, ROLE_ACTIVE)
                         .set(minimumIndex, ROLE_PIVOT)
                         .set(j, ROLE_COMPARE);
                    std::ostringstream status;
                    status << "Pass " << (i + 1) << " of " << (n - 1)
                           << ":  is A[" << j << "]=" << data[j]
                           << " smaller than the current minimum A[" << minimumIndex
                           << "]=" << data[minimumIndex] << " ?";
                    viz_.frame(data, roles.get(), status.str(), stats_);
                });

                if (greater(data, minimumIndex, j)) {
                    minimumIndex = j;
                    animate([&]() {
                        RoleMap update(data.size());
                        update.setRange(0, i - 1, ROLE_SORTED)
                              .set(i, ROLE_ACTIVE)
                              .set(minimumIndex, ROLE_PIVOT);
                        std::ostringstream found;
                        found << "Pass " << (i + 1) << " of " << (n - 1)
                              << ":  new minimum found - A[" << minimumIndex << "]="
                              << data[minimumIndex];
                        viz_.frame(data, update.get(), found.str(), stats_);
                    });
                }
            }

            if (minimumIndex != i) {
                RoleMap     swapping(animating() ? data.size() : 0);
                std::string message;
                animate([&]() {
                    swapping.setRange(0, i - 1, ROLE_SORTED)
                            .set(i, ROLE_SWAP).set(minimumIndex, ROLE_SWAP);
                    std::ostringstream status;
                    status << "Pass " << (i + 1) << " of " << (n - 1)
                           << ":  swapping the minimum " << data[minimumIndex]
                           << " into position " << i;
                    message = status.str();
                    viz_.frame(data, swapping.get(), message, stats_);
                });
                swapAt(data, i, minimumIndex);
                animate([&]() { viz_.frame(data, swapping.get(), message, stats_); });
            } else {
                animate([&]() {
                    RoleMap roles(data.size());
                    roles.setRange(0, i, ROLE_SORTED);
                    std::ostringstream status;
                    status << "Pass " << (i + 1) << " of " << (n - 1)
                           << ":  A[" << i << "]=" << data[i]
                           << " is already the smallest - no swap needed.";
                    viz_.frame(data, roles.get(), status.str(), stats_);
                });
            }
        }
    }
};

/* ------------------------------------------------------------- INSERTION -- */
class InsertionSort : public SortingAlgorithm {
public:
    explicit InsertionSort(Visualizer& v) : SortingAlgorithm(v) {}

    std::string name() const { return "Insertion Sort"; }
    Complexity complexity() const {
        return Complexity("O(n)", "O(n^2)", "O(n^2)", "O(1)", "Stable",
                          "Excellent on nearly sorted data - the inner loop rarely runs.");
    }
    std::string description() const {
        return "Grows a sorted prefix one element at a time, sliding each new "
               "value left until it lands in the right place - like sorting cards.";
    }

    std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_SORTED,  "sorted prefix"));
        entries.push_back(LegendEntry(ROLE_ACTIVE,  "key"));
        entries.push_back(LegendEntry(ROLE_COMPARE, "comparing"));
        entries.push_back(LegendEntry(ROLE_SWAP,    "shifting"));
        entries.push_back(LegendEntry(ROLE_NORMAL,  "not reached"));
        return entries;
    }

protected:
    void sort(std::vector<int>& data) {
        int n = static_cast<int>(data.size());
        for (int i = 1; i < n; ++i) {
            stats_.setPass(i, n - 1);
            int key = data[static_cast<size_t>(i)];
            int j   = i - 1;

            animate([&]() {
                RoleMap roles(data.size());
                roles.setRange(0, i - 1, ROLE_SORTED).set(i, ROLE_ACTIVE);
                std::ostringstream status;
                status << "Pass " << i << " of " << (n - 1)
                       << ":  lifting key = " << key << " from index " << i
                       << " to insert it into the sorted prefix A[0.." << (i - 1) << "]";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });

            /* Slide every larger element one place to the right. */
            while (j >= 0 && greaterThanValue(data, j, key)) {
                RoleMap     roles(animating() ? data.size() : 0);
                std::string message;
                animate([&]() {
                    roles.setRange(0, i - 1, ROLE_SORTED)
                         .set(j, ROLE_COMPARE).set(j + 1, ROLE_SWAP);
                    std::ostringstream status;
                    status << "Pass " << i << " of " << (n - 1)
                           << ":  A[" << j << "]=" << data[j] << " > key " << key
                           << "  ->  shifting it right into index " << (j + 1);
                    message = status.str();
                    viz_.frame(data, roles.get(), message, stats_);
                });

                writeAt(data, j + 1, data[static_cast<size_t>(j)]);
                stats_.countShift();         // one shift = one displacement

                animate([&]() { viz_.frame(data, roles.get(), message, stats_); });
                --j;
            }

            writeAt(data, j + 1, key);
            animate([&]() {
                RoleMap roles(data.size());
                roles.setRange(0, i, ROLE_SORTED).set(j + 1, ROLE_ACTIVE);
                std::ostringstream status;
                status << "Pass " << i << " of " << (n - 1)
                       << ":  key " << key << " dropped into index " << (j + 1)
                       << " - prefix A[0.." << i << "] is now sorted.";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });
        }
    }
};

/* ----------------------------------------------------------------- MERGE -- */
class MergeSort : public SortingAlgorithm {
public:
    explicit MergeSort(Visualizer& v) : SortingAlgorithm(v) {}

    std::string name() const { return "Merge Sort"; }
    Complexity complexity() const {
        return Complexity("O(n log n)", "O(n log n)", "O(n log n)", "O(n)", "Stable",
                          "Divide and conquer; the only sort here that guarantees n log n.");
    }
    std::string description() const {
        return "Recursively halves the array until single elements remain, then "
               "merges the sorted halves back together in linear time.";
    }

    std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_NORMAL,  "outside range"));
        entries.push_back(LegendEntry(ROLE_RANGE,   "sub array"));
        entries.push_back(LegendEntry(ROLE_COMPARE, "half fronts"));
        entries.push_back(LegendEntry(ROLE_ACTIVE,  "write cursor"));
        entries.push_back(LegendEntry(ROLE_SWAP,    "just written"));
        entries.push_back(LegendEntry(ROLE_SORTED,  "merged"));
        return entries;
    }

protected:
    void sort(std::vector<int>& data) {
        mergeSort(data, 0, static_cast<int>(data.size()) - 1);
    }

private:
    /* Classic recursive divide and conquer. */
    void mergeSort(std::vector<int>& data, int low, int high) {
        if (low >= high) return;
        stats_.enterRecursion();

        int middle = low + (high - low) / 2;

        animate([&]() {
            RoleMap roles(data.size());
            roles.setRange(low, high, ROLE_RANGE);
            std::ostringstream status;
            status << "Divide (depth " << stats_.depth() << "):  splitting A["
                   << low << ".." << high << "] into A[" << low << ".." << middle
                   << "] and A[" << (middle + 1) << ".." << high << "]";
            viz_.frame(data, roles.get(), status.str(), stats_);
        });

        mergeSort(data, low, middle);
        mergeSort(data, middle + 1, high);
        merge(data, low, middle, high);

        stats_.exitRecursion();
    }

    /* Merge the two sorted halves A[low..middle] and A[middle+1..high]. */
    void merge(std::vector<int>& data, int low, int middle, int high) {
        std::vector<int> left (data.begin() + low,        data.begin() + middle + 1);
        std::vector<int> right(data.begin() + middle + 1, data.begin() + high + 1);

        size_t i = 0, j = 0;
        int    k = low;

        while (i < left.size() && j < right.size()) {
            animate([&]() {
                int leftIndex  = low + static_cast<int>(i);
                int rightIndex = middle + 1 + static_cast<int>(j);
                /* The write cursor is marked before the two fronts, so that
                 * when the destination coincides with the left front - which
                 * it does at the start of every merge - the comparison stays
                 * the thing the eye is drawn to. */
                RoleMap roles(data.size());
                roles.setRange(low, high, ROLE_RANGE)
                     .setRange(low, k - 1, ROLE_SORTED)
                     .set(k, ROLE_ACTIVE)
                     .set(leftIndex, ROLE_COMPARE)
                     .set(rightIndex, ROLE_COMPARE);
                std::ostringstream status;
                status << "Merge (depth " << stats_.depth() << "):  comparing left "
                       << left[i] << " with right " << right[j]
                       << "  ->  smaller one goes to index " << k;
                viz_.frame(data, roles.get(), status.str(), stats_);
            });

            stats_.countComparison();
            int chosen;
            if (left[i] <= right[j]) chosen = left[i++];
            else                     chosen = right[j++];

            writeAt(data, k, chosen);

            animate([&]() {
                RoleMap after(data.size());
                after.setRange(low, high, ROLE_RANGE)
                     .setRange(low, k, ROLE_SORTED)
                     .set(k, ROLE_SWAP);
                std::ostringstream written;
                written << "Merge (depth " << stats_.depth() << "):  wrote " << chosen
                        << " into index " << k;
                viz_.frame(data, after.get(), written.str(), stats_);
            });
            ++k;
        }

        /* Whatever is left over is already in order - copy it straight down. */
        while (i < left.size()) {
            writeAt(data, k, left[i++]);
            drawTail(data, low, high, k, "left");
            ++k;
        }
        while (j < right.size()) {
            writeAt(data, k, right[j++]);
            drawTail(data, low, high, k, "right");
            ++k;
        }

        animate([&]() {
            RoleMap done(data.size());
            done.setRange(low, high, ROLE_SORTED);
            std::ostringstream status;
            status << "Merge (depth " << stats_.depth() << ") complete:  A["
                   << low << ".." << high << "] is now sorted.";
            viz_.frame(data, done.get(), status.str(), stats_);
        });
    }

    void drawTail(const std::vector<int>& data, int low, int high, int k,
                  const std::string& side) {
        animate([&]() {
            RoleMap roles(data.size());
            roles.setRange(low, high, ROLE_RANGE)
                 .setRange(low, k, ROLE_SORTED)
                 .set(k, ROLE_SWAP);
            std::ostringstream status;
            status << "Merge (depth " << stats_.depth() << "):  " << side
                   << " half exhausted the other side - copying "
                   << data[static_cast<size_t>(k)] << " into index " << k;
            viz_.frame(data, roles.get(), status.str(), stats_);
        });
    }
};

/* ----------------------------------------------------------------- QUICK -- */
class QuickSort : public SortingAlgorithm {
public:
    explicit QuickSort(Visualizer& v) : SortingAlgorithm(v) {}

    std::string name() const { return "Quick Sort"; }
    Complexity complexity() const {
        return Complexity("O(n log n)", "O(n log n)", "O(n^2)", "O(log n)", "Not stable",
                          "Lomuto partition. Worst case is already sorted input.");
    }
    std::string description() const {
        return "Picks the last element as a pivot, partitions everything smaller "
               "to its left and larger to its right, then recurses on both sides.";
    }

    std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_RANGE,   "partition"));
        entries.push_back(LegendEntry(ROLE_PIVOT,   "pivot"));
        entries.push_back(LegendEntry(ROLE_COMPARE, "comparing"));
        entries.push_back(LegendEntry(ROLE_ACTIVE,  "boundary"));
        entries.push_back(LegendEntry(ROLE_SWAP,    "swapping"));
        entries.push_back(LegendEntry(ROLE_SORTED,  "final place"));
        return entries;
    }

protected:
    void sort(std::vector<int>& data) {
        placed_.assign(data.size(), false);
        quickSort(data, 0, static_cast<int>(data.size()) - 1);
    }

private:
    /* Build the role map for a frame, keeping every already placed pivot green. */
    RoleMap baseRoles(size_t size, int low, int high) const {
        RoleMap roles(size, ROLE_NORMAL);
        roles.setRange(low, high, ROLE_RANGE);
        for (size_t i = 0; i < placed_.size(); ++i)
            if (placed_[i]) roles.set(static_cast<int>(i), ROLE_SORTED);
        return roles;
    }

    void quickSort(std::vector<int>& data, int low, int high) {
        if (low > high) return;
        if (low == high) {                       // a single element is trivially placed
            placed_[static_cast<size_t>(low)] = true;
            return;
        }
        stats_.enterRecursion();

        int pivotIndex = partition(data, low, high);
        placed_[static_cast<size_t>(pivotIndex)] = true;

        animate([&]() {
            RoleMap roles = baseRoles(data.size(), low, high);
            roles.set(pivotIndex, ROLE_SORTED);
            std::ostringstream status;
            status << "Pivot " << data[static_cast<size_t>(pivotIndex)]
                   << " is now at index " << pivotIndex
                   << " - everything left is smaller, everything right is larger.";
            viz_.frame(data, roles.get(), status.str(), stats_);
        });

        quickSort(data, low, pivotIndex - 1);
        quickSort(data, pivotIndex + 1, high);

        stats_.exitRecursion();
    }

    /* Lomuto partition scheme: pivot is the last element of the range. */
    int partition(std::vector<int>& data, int low, int high) {
        int pivotValue = data[static_cast<size_t>(high)];
        int boundary   = low - 1;

        animate([&]() {
            RoleMap roles = baseRoles(data.size(), low, high);
            roles.set(high, ROLE_PIVOT);
            std::ostringstream status;
            status << "Partition A[" << low << ".." << high << "] (depth "
                   << stats_.depth() << "):  pivot = A[" << high << "] = " << pivotValue;
            viz_.frame(data, roles.get(), status.str(), stats_);
        });

        for (int j = low; j < high; ++j) {
            animate([&]() {
                RoleMap roles = baseRoles(data.size(), low, high);
                roles.set(high, ROLE_PIVOT).set(j, ROLE_COMPARE);
                if (boundary >= low) roles.set(boundary, ROLE_ACTIVE);
                std::ostringstream status;
                status << "Partition A[" << low << ".." << high << "]:  is A[" << j
                       << "]=" << data[j] << " <= pivot " << pivotValue << " ?";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });

            if (lessOrEqualValue(data, j, pivotValue)) {
                ++boundary;
                RoleMap     swapping(animating() ? data.size() : 0);
                std::string message;
                animate([&]() {
                    swapping = baseRoles(data.size(), low, high);
                    swapping.set(high, ROLE_PIVOT)
                            .set(j, ROLE_SWAP).set(boundary, ROLE_SWAP);
                    std::ostringstream status;
                    status << "Partition A[" << low << ".." << high << "]:  yes - moving "
                           << data[j] << " into the 'smaller' side at index " << boundary;
                    message = status.str();
                    viz_.frame(data, swapping.get(), message, stats_);
                });

                /* Textbook Lomuto swaps unconditionally, even when boundary
                 * and j are the same slot - the count stays faithful to it. */
                swapAt(data, boundary, j);

                animate([&]() { viz_.frame(data, swapping.get(), message, stats_); });
            }
        }

        animate([&]() {
            RoleMap roles = baseRoles(data.size(), low, high);
            roles.set(high, ROLE_SWAP).set(boundary + 1, ROLE_SWAP);
            std::ostringstream status;
            status << "Partition A[" << low << ".." << high
                   << "]:  placing the pivot " << pivotValue
                   << " at index " << (boundary + 1);
            viz_.frame(data, roles.get(), status.str(), stats_);
        });
        swapAt(data, boundary + 1, high);
        return boundary + 1;
    }

    std::vector<bool> placed_;
};

/* ===========================================================================
 * SECTION 8 : THE TWO SEARCHING ALGORITHMS
 * ===========================================================================
 */

/* ---------------------------------------------------------------- LINEAR -- */
class LinearSearch : public SearchingAlgorithm {
public:
    explicit LinearSearch(Visualizer& v) : SearchingAlgorithm(v) {}

    std::string name() const { return "Linear Search"; }
    Complexity complexity() const {
        return Complexity("O(1)", "O(n)", "O(n)", "O(1)", "N/A",
                          "Works on any array, sorted or not.");
    }
    std::string description() const {
        return "Checks every element from left to right until the target turns "
               "up. No ordering required, but no shortcuts either.";
    }

protected:
    int search(const std::vector<int>& data, int target) {
        int n = static_cast<int>(data.size());
        for (int i = 0; i < n; ++i) {
            animate([&]() {
                RoleMap roles(data.size());
                roles.setRange(0, i - 1, ROLE_DISCARDED).set(i, ROLE_COMPARE);
                std::ostringstream status;
                status << "Step " << (i + 1) << " of at most " << n
                       << ":  is A[" << i << "]=" << data[i]
                       << " equal to the target " << target << " ?";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });

            if (equals(data, i, target)) {
                animate([&]() {
                    RoleMap hit(data.size());
                    hit.setRange(0, i - 1, ROLE_DISCARDED).set(i, ROLE_FOUND);
                    std::ostringstream found;
                    found << "Match! A[" << i << "] = " << target;
                    viz_.frame(data, hit.get(), found.str(), stats_);
                });
                return i;
            }
        }
        return -1;
    }
};

/* ---------------------------------------------------------------- BINARY -- */
class BinarySearch : public SearchingAlgorithm {
public:
    explicit BinarySearch(Visualizer& v) : SearchingAlgorithm(v) {}

    std::string name() const { return "Binary Search"; }
    Complexity complexity() const {
        return Complexity("O(1)", "O(log n)", "O(log n)", "O(log n)", "N/A",
                          "Requires a sorted array. Space is O(log n) here because this "
                          "implementation recurses; an iterative version would be O(1).");
    }
    std::string description() const {
        return "Repeatedly looks at the middle of the remaining range and throws "
               "away the half that cannot contain the target.";
    }

    bool requiresSortedInput() const { return true; }

    std::vector<LegendEntry> legend() const {
        std::vector<LegendEntry> entries;
        entries.push_back(LegendEntry(ROLE_RANGE,     "candidates"));
        entries.push_back(LegendEntry(ROLE_PIVOT,     "midpoint"));
        entries.push_back(LegendEntry(ROLE_DISCARDED, "eliminated"));
        entries.push_back(LegendEntry(ROLE_FOUND,     "found"));
        return entries;
    }

protected:
    int search(const std::vector<int>& data, int target) {
        return binarySearch(data, target, 0, static_cast<int>(data.size()) - 1);
    }

private:
    /* Recursive formulation - shows the halving very clearly. */
    int binarySearch(const std::vector<int>& data, int target, int low, int high) {
        if (low > high) {
            animate([&]() {
                RoleMap roles(data.size(), ROLE_DISCARDED);
                std::ostringstream status;
                status << "The search range is empty (low=" << low << " > high=" << high
                       << ") - the target " << target << " cannot be in the array.";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });
            return -1;
        }
        stats_.enterRecursion();

        int middle = low + (high - low) / 2;      // overflow safe midpoint

        /* The same role map serves every branch below: the surviving range in
         * one colour, the eliminated half greyed out, the midpoint picked out. */
        RoleMap roles(animating() ? data.size() : 0, ROLE_RANGE);
        animate([&]() {
            roles.setOutside(low, high, ROLE_DISCARDED).set(middle, ROLE_PIVOT);
            std::ostringstream status;
            status << "Range A[" << low << ".." << high << "]  (" << (high - low + 1)
                   << " candidates left):  midpoint is A[" << middle << "]="
                   << data[middle] << ", target is " << target;
            viz_.frame(data, roles.get(), status.str(), stats_);
        });

        int result;
        if (equals(data, middle, target)) {
            animate([&]() {
                RoleMap hit(data.size(), ROLE_DISCARDED);
                hit.set(middle, ROLE_FOUND);
                std::ostringstream status;
                status << "A[" << middle << "] == " << target << "  ->  found it.";
                viz_.frame(data, hit.get(), status.str(), stats_);
            });
            result = middle;
        } else if (lessThanTarget(data, middle, target)) {
            animate([&]() {
                std::ostringstream status;
                status << "A[" << middle << "]=" << data[middle] << " < " << target
                       << "  ->  discard the left half, search A[" << (middle + 1)
                       << ".." << high << "]";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });
            result = binarySearch(data, target, middle + 1, high);
        } else {
            animate([&]() {
                std::ostringstream status;
                status << "A[" << middle << "]=" << data[middle] << " > " << target
                       << "  ->  discard the right half, search A[" << low
                       << ".." << (middle - 1) << "]";
                viz_.frame(data, roles.get(), status.str(), stats_);
            });
            result = binarySearch(data, target, low, middle - 1);
        }

        stats_.exitRecursion();
        return result;
    }
};

/* ===========================================================================
 * SECTION 9 : THE APPLICATION  (menus, screens, array management)
 * ===========================================================================
 */
class DsaVisualizerApp {
public:
    DsaVisualizerApp()
        : viz_(theme_), input_(theme_),
          generator_(static_cast<unsigned int>(
              std::chrono::system_clock::now().time_since_epoch().count())) {
        buildAlgorithms();
        generateRandomArray(10);
    }

    void run() {
        showWelcome();
        while (true) {
            if (!mainMenu()) break;
        }
        showGoodbye();
    }

private:
    /* ---- construction of the polymorphic algorithm table ---------------- */
    void buildAlgorithms() {
        sorts_.push_back(std::unique_ptr<SortingAlgorithm>(new BubbleSort(viz_)));
        sorts_.push_back(std::unique_ptr<SortingAlgorithm>(new SelectionSort(viz_)));
        sorts_.push_back(std::unique_ptr<SortingAlgorithm>(new InsertionSort(viz_)));
        sorts_.push_back(std::unique_ptr<SortingAlgorithm>(new MergeSort(viz_)));
        sorts_.push_back(std::unique_ptr<SortingAlgorithm>(new QuickSort(viz_)));

        searches_.push_back(std::unique_ptr<SearchingAlgorithm>(new LinearSearch(viz_)));
        searches_.push_back(std::unique_ptr<SearchingAlgorithm>(new BinarySearch(viz_)));

        /* Whenever the program itself needs a sorted array - to prepare test
         * data, or to rescue Binary Search from unsorted input - it uses its
         * own Merge Sort rather than std::sort. Nothing here is sorted by the
         * standard library. */
        utilitySort_ = sorts_[3].get();
    }

    /* Sort a vector using this program's own Merge Sort, without animation. */
    void sortInPlace(std::vector<int>& target) {
        utilitySort_->runSilently(target);
    }

    /* ---- shared screen furniture ---------------------------------------- */
    /* Panels are drawn inside the same 2 column margin as the bar chart, and
     * add two corner characters of their own. */
    static const int kMargin = 2;

    int panelWidth() const {
        int width = Console::width() - kMargin * 2 - 2;
        if (width < 62) width = 62;
        if (width > 92) width = 92;
        return width;
    }

    void line(const std::string& content = "") const {
        std::cout << theme_.base() << content << theme_.base();
        if (Console::richOutput()) std::cout << "\x1b[K";
        std::cout << "\n";
    }

    void header(const std::string& title, const std::string& subtitle = "") const {
        const Glyphs& g = theme_.glyphs();
        int width = panelWidth();
        const std::string margin(kMargin, ' ');
        std::cout << theme_.base();
        Console::clear();
        line(margin + theme_.accent() + g.bannerTL + util::repeat(g.bannerH, width) + g.bannerTR);
        line(margin + theme_.accent() + g.bannerV + theme_.title() +
             util::padRight(" " + title, width) + theme_.accent() + g.bannerV);
        if (!subtitle.empty())
            line(margin + theme_.accent() + g.bannerV + theme_.dim() +
                 util::padRight(" " + subtitle, width) + theme_.accent() + g.bannerV);
        line(margin + theme_.accent() + g.bannerBL + util::repeat(g.bannerH, width) + g.bannerBR);
        line();
    }

    void menuItem(int number, const std::string& label,
                  const std::string& hint = "") const {
        std::ostringstream out;
        out << "   " << theme_.accent() << "[" << number << "] " << theme_.text()
            << util::padRight(label, 34);
        if (!hint.empty()) out << theme_.dim() << hint;
        line(out.str());
    }

    void sectionRule() const {
        line(theme_.dim() + "   " + util::repeat(theme_.glyphs().horizontal, panelWidth() - 2));
    }

    /* A one line preview of the working array, used on most screens. */
    void showArraySummary() const {
        const Glyphs& g = theme_.glyphs();
        std::ostringstream out;
        out << "   " << theme_.dim() << "Current array (" << data_.size() << " elements): "
            << theme_.text() << util::arrayToString(data_);
        line(out.str());

        std::ostringstream state;
        state << "   " << theme_.dim() << "State: " << theme_.text()
              << (isSorted(data_) ? "sorted ascending" : "unsorted")
              << theme_.dim() << "   " << g.bullet << "   min " << theme_.text()
              << *std::min_element(data_.begin(), data_.end())
              << theme_.dim() << "   " << g.bullet << "   max " << theme_.text()
              << *std::max_element(data_.begin(), data_.end());
        line(state.str());
    }

    static bool isSorted(const std::vector<int>& data) {
        for (size_t i = 1; i < data.size(); ++i)
            if (data[i - 1] > data[i]) return false;
        return true;
    }

    /* ---- welcome / goodbye ---------------------------------------------- */
    void showWelcome() const {
        header("DSA VISUALIZER", "Sorting & Searching Algorithms  |  Minor Project  |  C++ / STL / OOP");
        line("   " + theme_.text() + "This program animates how each algorithm actually moves data.");
        line("   " + theme_.text() + "Every comparison and every swap is drawn and counted live.");
        line();
        line("   " + theme_.dim() + "Sorting   : Bubble  " + theme_.glyphs().bullet +
             "  Selection  " + theme_.glyphs().bullet + "  Insertion  " +
             theme_.glyphs().bullet + "  Merge  " + theme_.glyphs().bullet + "  Quick");
        line("   " + theme_.dim() + "Searching : Linear  " + theme_.glyphs().bullet + "  Binary");
        line();
        if (!Console::richOutput()) {
            line("   " + theme_.warn() +
                 "[!] This terminal does not support colour / cursor control.");
            line("   " + theme_.warn() +
                 "    Animation is disabled; results and statistics still work.");
            line();
        } else if (Console::width() < 80 || Console::height() < 26) {
            std::ostringstream note;
            note << "[!] This window is " << Console::width() << " x " << Console::height()
                 << ". The chart shrinks to fit, but 110 x 42 or larger looks best.";
            line("   " + theme_.warn() + note.str());
            line("   " + theme_.warn() +
                 "    Maximise the window now if you can.");
            line();
        }
        line("   " + theme_.dim() + "During any animation:  [Space] pause   [+/-] speed   "
             "[S] step   [Q] abort");
        line();
        input_.pause();
    }

    void showGoodbye() const {
        header("GOODBYE", "Thanks for using the DSA Visualizer");
        line("   " + theme_.text() + "Every algorithm here was implemented from scratch -");
        line("   " + theme_.text() + "no std::sort, no std::find, no shortcuts.");
        line();
        std::cout << theme_.reset() << std::flush;
    }

    /* ---- main menu ------------------------------------------------------- */
    bool mainMenu() {
        header("MAIN MENU", "DSA Visualizer  |  Sorting & Searching");
        showArraySummary();
        line();
        sectionRule();
        line();
        menuItem(1, "Sorting Algorithms",        "animate Bubble / Selection / Insertion / Merge / Quick");
        menuItem(2, "Searching Algorithms",      "animate Linear / Binary search");
        menuItem(3, "Array Setup",               "type your own, or generate a test case");
        menuItem(4, "Complexity Comparison Chart", "big-O reference for all seven algorithms");
        menuItem(5, "Benchmark All Sorts",       "race all five sorts on the same data");
        menuItem(6, "Settings",                  "speed, dark/light theme, display mode");
        menuItem(7, "About & Help",              "what each algorithm does");
        line();
        menuItem(0, "Exit");
        line();

        int choice = input_.readChoice(0, 7);
        switch (choice) {
            case 1: sortingMenu();          break;
            case 2: searchingMenu();        break;
            case 3: arrayMenu();            break;
            case 4: showComplexityChart();  break;
            case 5: runBenchmark();         break;
            case 6: settingsMenu();         break;
            case 7: showAbout();            break;
            case 0: return false;
        }
        return true;
    }

    /* ---- sorting ---------------------------------------------------------- */
    void sortingMenu() {
        header("SORTING ALGORITHMS", "Pick one to watch it run on the current array");
        showArraySummary();
        line();
        sectionRule();
        line();
        for (size_t i = 0; i < sorts_.size(); ++i) {
            Complexity c = sorts_[i]->complexity();
            std::ostringstream hint;
            hint << "avg " << util::padRight(c.average, 12) << c.stability;
            menuItem(static_cast<int>(i) + 1, sorts_[i]->name(), hint.str());
        }
        line();
        menuItem(0, "Back to main menu");
        line();

        int choice = input_.readChoice(0, static_cast<int>(sorts_.size()));
        if (choice == 0) return;
        runSort(*sorts_[static_cast<size_t>(choice) - 1]);
    }

    void runSort(SortingAlgorithm& algorithm) {
        std::vector<int> original = data_;
        std::vector<int> working  = data_;

        bool completed = algorithm.run(working);

        /* The sort is the point of the exercise - prove it worked. */
        bool correct = isSorted(working);
        showSortResult(algorithm, original, working, completed, correct);

        if (input_.readYesNo("  Keep the sorted array as the working array? [Y/n] > ", true))
            data_ = working;
    }

    void showSortResult(const SortingAlgorithm& algorithm,
                        const std::vector<int>& original,
                        const std::vector<int>& sorted,
                        bool completed, bool correct) const {
        const Statistics& s = algorithm.stats();
        Complexity c        = algorithm.complexity();

        header(util::toUpper(algorithm.name()) + " - RESULT",
               completed ? "Animation finished" : "Animation aborted - results computed in full");

        line("   " + theme_.dim() + "Input  : " + theme_.text() + util::arrayToString(original));
        line("   " + theme_.dim() + "Output : " + theme_.ok()   + util::arrayToString(sorted));
        line();
        line("   " + (correct ? theme_.ok()    + "[OK]  Verified: the output is in ascending order."
                              : theme_.error() + "[!!]  The output is NOT sorted - this is a bug."));
        line();
        sectionRule();
        line();

        statLine("Comparisons",      util::withCommas(s.comparisons()));
        statLine("Swaps / shifts",   util::withCommas(s.swaps()));
        statLine("Array writes",     util::withCommas(s.writes()));
        if (s.maxDepth() > 0)
            statLine("Max recursion depth", std::to_string(s.maxDepth()));
        statLine("Execution time",   util::formatDuration(s.microseconds()) +
                                     "   (measured without animation)");
        line();
        statLine("Time  - best",     c.best);
        statLine("Time  - average",  c.average);
        statLine("Time  - worst",    c.worst);
        statLine("Space",            c.space);
        statLine("Stability",        c.stability);
        line();
        line("   " + theme_.dim() + c.note);
        line();
        input_.pause();
    }

    void statLine(const std::string& label, const std::string& value) const {
        line("   " + theme_.dim() + util::padRight(label, 22) + " : " +
             theme_.text() + value);
    }

    /* ---- searching -------------------------------------------------------- */
    void searchingMenu() {
        header("SEARCHING ALGORITHMS", "Pick one to watch it hunt for a value");
        showArraySummary();
        line();
        sectionRule();
        line();
        for (size_t i = 0; i < searches_.size(); ++i) {
            Complexity c = searches_[i]->complexity();
            std::ostringstream hint;
            hint << "avg " << util::padRight(c.average, 12)
                 << (searches_[i]->requiresSortedInput() ? "needs a sorted array" : "any array");
            menuItem(static_cast<int>(i) + 1, searches_[i]->name(), hint.str());
        }
        line();
        menuItem(0, "Back to main menu");
        line();

        int choice = input_.readChoice(0, static_cast<int>(searches_.size()));
        if (choice == 0) return;
        runSearch(*searches_[static_cast<size_t>(choice) - 1]);
    }

    void runSearch(SearchingAlgorithm& algorithm) {
        /* Binary search is only correct on sorted input - refuse to mislead
         * the user, and offer to fix it for them. */
        if (algorithm.requiresSortedInput() && !isSorted(data_)) {
            header(util::toUpper(algorithm.name()), "This algorithm needs a sorted array");
            line("   " + theme_.warn() + "[!] The current array is NOT sorted:");
            line("       " + theme_.text() + util::arrayToString(data_));
            line();
            line("   " + theme_.text() + "Binary search assumes ascending order - on unsorted data it");
            line("   " + theme_.text() + "would discard the half that actually contains the target and");
            line("   " + theme_.text() + "report a wrong answer.");
            line();
            if (!input_.readYesNo("  Sort the array first, then search? [Y/n] > ", true)) {
                line();
                line("   " + theme_.dim() + "Search cancelled.");
                line();
                input_.pause();
                return;
            }
            sortInPlace(data_);
            line();
            line("   " + theme_.ok() + "Array sorted: " + theme_.text() + util::arrayToString(data_));
            line();
            input_.pause();
        }

        header(util::toUpper(algorithm.name()), "Choose the value to search for");
        showArraySummary();
        line();
        line("   " + theme_.dim() +
             "Tip: pick a value that is not in the array to watch the failure case.");
        line();
        int target = input_.readInt("  Target value > ", limits::MIN_VALUE, limits::MAX_VALUE);

        int index = algorithm.run(data_, target);
        showSearchResult(algorithm, target, index);
    }

    void showSearchResult(const SearchingAlgorithm& algorithm,
                          int target, int index) const {
        const Statistics& s = algorithm.stats();
        Complexity c        = algorithm.complexity();

        header(util::toUpper(algorithm.name()) + " - RESULT",
               index >= 0 ? "Target located" : "Target not present");

        line("   " + theme_.dim() + "Array  : " + theme_.text() + util::arrayToString(data_));
        line("   " + theme_.dim() + "Target : " + theme_.text() + std::to_string(target));
        line();
        if (index >= 0) {
            line("   " + theme_.ok() + "[OK]  Found " + std::to_string(target) +
                 " at index " + std::to_string(index) + ".");
        } else {
            line("   " + theme_.warn() + "[--]  " + std::to_string(target) +
                 " does not occur anywhere in the array.");
        }
        line();
        sectionRule();
        line();
        statLine("Comparisons", util::withCommas(s.comparisons()));
        if (s.maxDepth() > 0)
            statLine("Max recursion depth", std::to_string(s.maxDepth()));
        statLine("Execution time", util::formatDuration(s.microseconds()) +
                                   "   (measured without animation)");
        line();
        statLine("Time  - best",    c.best);
        statLine("Time  - average", c.average);
        statLine("Time  - worst",   c.worst);
        statLine("Space",           c.space);
        line();
        line("   " + theme_.dim() + c.note);
        line();
        input_.pause();
    }

    /* ---- array setup ------------------------------------------------------ */
    void arrayMenu() {
        while (true) {
            header("ARRAY SETUP", "Build the array the algorithms will work on");
            showArraySummary();
            line();
            showMiniPreview();
            line();
            sectionRule();
            line();
            menuItem(1, "Enter a custom array",   "type your own values");
            menuItem(2, "Generate random array",  "the usual average case");
            menuItem(3, "Generate sorted array",  "best case for Bubble/Insertion, worst for Quick");
            menuItem(4, "Generate reverse array", "worst case for most sorts");
            menuItem(5, "Generate nearly sorted", "shows off Insertion Sort");
            menuItem(6, "Shuffle current array",  "same values, new order");
            menuItem(7, "Sort current array",     "prepare it for Binary Search");
            line();
            menuItem(0, "Back to main menu");
            line();

            int choice = input_.readChoice(0, 7);
            if (choice == 0) return;

            switch (choice) {
                case 1: enterCustomArray();                        break;
                case 2: generateRandomArray(askSize());            break;
                case 3: generateSortedArray(askSize(), false);     break;
                case 4: generateSortedArray(askSize(), true);      break;
                case 5: generateNearlySortedArray(askSize());      break;
                case 6: shuffleArray();                            break;
                case 7: sortInPlace(data_);                        break;
            }
        }
    }

    /* Small inline bar preview so the user can see the shape of the data. */
    void showMiniPreview() const {
        const Glyphs& g = theme_.glyphs();
        int maximum = *std::max_element(data_.begin(), data_.end());
        if (maximum < 1) maximum = 1;
        const int rows = 6;
        for (int row = rows; row >= 1; --row) {
            std::string rendered = "   " + theme_.barColour(ROLE_NORMAL);
            for (size_t i = 0; i < data_.size(); ++i) {
                int height = static_cast<int>(
                    (static_cast<double>(data_[i]) * rows) / maximum + 0.5);
                if (height < 1) height = 1;
                rendered += (height >= row ? g.block + g.block : "  ");
                rendered += " ";
            }
            line(rendered);
        }
        std::string labels = "   " + theme_.dim();
        for (size_t i = 0; i < data_.size(); ++i)
            labels += util::padCentre(std::to_string(data_[i]), 2) + " ";
        line(labels);
    }

    int askSize() {
        return input_.readInt("  How many elements? > ",
                              limits::MIN_ELEMENTS, limits::MAX_ELEMENTS);
    }

    void enterCustomArray() {
        header("CUSTOM ARRAY", "Type the values yourself");
        line("   " + theme_.dim() + "Example: " + theme_.text() + "42 8 15 4 23 16");
        line("   " + theme_.dim() + "Commas are fine too: " + theme_.text() + "42, 8, 15");
        line();
        std::vector<int> values = input_.readArray(limits::MIN_ELEMENTS, limits::MAX_ELEMENTS,
                                                  limits::MIN_VALUE, limits::MAX_VALUE);
        data_ = values;
        line();
        line("   " + theme_.ok() + "Array accepted: " + theme_.text() + util::arrayToString(data_));
        line();
        input_.pause();
    }

    void generateRandomArray(int count) {
        std::uniform_int_distribution<int> distribution(limits::MIN_VALUE, limits::MAX_VALUE);
        data_.clear();
        data_.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) data_.push_back(distribution(generator_));
    }

    void generateSortedArray(int count, bool descending) {
        generateRandomArray(count);
        sortInPlace(data_);
        if (descending) std::reverse(data_.begin(), data_.end());
    }

    void generateNearlySortedArray(int count) {
        generateSortedArray(count, false);
        /* Disturb roughly one tenth of the array. */
        int swaps = count / 10 + 1;
        std::uniform_int_distribution<int> position(0, count - 1);
        for (int i = 0; i < swaps; ++i)
            std::swap(data_[static_cast<size_t>(position(generator_))],
                      data_[static_cast<size_t>(position(generator_))]);
    }

    void shuffleArray() {
        std::shuffle(data_.begin(), data_.end(), generator_);
    }

    /* ---- complexity reference chart --------------------------------------- */
    void showComplexityChart() {
        const Glyphs& g = theme_.glyphs();
        header("COMPLEXITY COMPARISON CHART", "Big-O for every algorithm in this program");

        const int nameWidth = 17, cellWidth = 13;
        std::string ruleLine = "   " + theme_.dim() +
            util::repeat(g.horizontal, nameWidth + cellWidth * 3 + 12 + 12);

        std::ostringstream head;
        head << "   " << theme_.accent()
             << util::padRight("ALGORITHM", nameWidth)
             << util::padRight("BEST", cellWidth)
             << util::padRight("AVERAGE", cellWidth)
             << util::padRight("WORST", cellWidth)
             << util::padRight("SPACE", 12)
             << "STABILITY";
        line(head.str());
        line(ruleLine);

        for (size_t i = 0; i < sorts_.size(); ++i)
            complexityRow(*sorts_[i], nameWidth, cellWidth);
        line(ruleLine);
        for (size_t i = 0; i < searches_.size(); ++i)
            complexityRow(*searches_[i], nameWidth, cellWidth);
        line();

        /* Growth illustration: how many operations each class of algorithm
         * needs as n grows.  Bars are log scaled so they fit on screen. */
        line("   " + theme_.accent() + "HOW THE WORK GROWS WITH n" +
             theme_.dim() + "   (bar length is log scaled)");
        line();
        const int sizes[] = {10, 100, 1000};
        for (int s = 0; s < 3; ++s) {
            int n = sizes[s];
            line("   " + theme_.title() + "n = " + std::to_string(n));
            growthRow("O(log n)   binary search", std::log(static_cast<double>(n)) / std::log(2.0), ROLE_SORTED);
            growthRow("O(n)       linear search", static_cast<double>(n), ROLE_ACTIVE);
            growthRow("O(n log n) merge / quick",
                      static_cast<double>(n) * std::log(static_cast<double>(n)) / std::log(2.0), ROLE_PIVOT);
            growthRow("O(n^2)     bubble/sel/ins",
                      static_cast<double>(n) * static_cast<double>(n), ROLE_SWAP);
            line();
        }

        line("   " + theme_.dim() +
             "Read it this way: at n = 1000 an O(n^2) sort does about 1,000,000 steps,");
        line("   " + theme_.dim() +
             "while an O(n log n) sort does about 10,000 - a hundredfold difference.");
        line();
        input_.pause();
    }

    void complexityRow(const Algorithm& algorithm, int nameWidth, int cellWidth) const {
        Complexity c = algorithm.complexity();
        std::ostringstream row;
        row << "   " << theme_.text() << util::padRight(algorithm.name(), nameWidth)
            << theme_.ok()    << util::padRight(c.best, cellWidth)
            << theme_.warn()  << util::padRight(c.average, cellWidth)
            << theme_.error() << util::padRight(c.worst, cellWidth)
            << theme_.text()  << util::padRight(c.space, 12)
            << theme_.dim()   << c.stability;
        line(row.str());
    }

    void growthRow(const std::string& label, double operations, BarRole role) const {
        const Glyphs& g = theme_.glyphs();
        /* Log scale: 1 operation -> 0 blocks, 10^6 operations -> 48 blocks. */
        double scaled = std::log10(operations > 1.0 ? operations : 1.0);
        int blocks = static_cast<int>(scaled * 8.0 + 0.5);
        if (blocks < 1)  blocks = 1;
        if (blocks > 48) blocks = 48;

        std::ostringstream row;
        row << "     " << theme_.dim() << util::padRight(label, 26)
            << theme_.barColour(role) << util::repeat(g.block, blocks)
            << theme_.dim() << "  " << util::withCommas(static_cast<long long>(operations + 0.5))
            << " steps";
        line(row.str());
    }

    /* ---- benchmark --------------------------------------------------------- */
    struct BenchmarkResult {
        std::string name;
        long long   comparisons;
        long long   swaps;
        double      microseconds;
    };

    void runBenchmark() {
        header("BENCHMARK - ALL FIVE SORTS", "Same input for every algorithm, animation disabled");
        line("   " + theme_.text() + "Each sort receives an identical randomly generated array,");
        line("   " + theme_.text() + "so the comparison counts and timings are directly comparable.");
        line();
        line("   " + theme_.dim() + "Larger sizes make the difference between O(n^2) and");
        line("   " + theme_.dim() + "O(n log n) impossible to miss. Try 100, then 1000.");
        line();

        int size = input_.readInt("  Array size > ", limits::MIN_BENCH_SIZE, limits::MAX_BENCH_SIZE);

        /* Randomly ordered input: this keeps Quick Sort out of its O(n^2)
         * worst case, so recursion stays shallow even at the maximum size. */
        std::vector<int> sample;
        sample.reserve(static_cast<size_t>(size));
        std::uniform_int_distribution<int> distribution(1, size * 10);
        for (int i = 0; i < size; ++i) sample.push_back(distribution(generator_));

        line();
        line("   " + theme_.dim() + "Running . . .");

        std::vector<BenchmarkResult> results;
        for (size_t i = 0; i < sorts_.size(); ++i) {
            SortingAlgorithm& algorithm = *sorts_[i];
            std::vector<int> working = sample;
            algorithm.runSilently(working);

            BenchmarkResult result;
            result.name         = algorithm.name();
            result.comparisons  = algorithm.stats().comparisons();
            result.swaps        = algorithm.stats().swaps();

            std::vector<int> scratch;
            result.microseconds = benchmarkTime(algorithm, sample, scratch);
            results.push_back(result);

            /* Never report a number without checking the work behind it. */
            if (!isSorted(working)) {
                line("   " + theme_.error() + "[!!] " + result.name +
                     " produced an unsorted result.");
            }
        }

        showBenchmarkResults(results, size);
    }

    static double benchmarkTime(SortingAlgorithm& algorithm,
                                const std::vector<int>& sample,
                                std::vector<int>& scratch) {
        int repetitions = 1;
        double elapsed  = 0.0;
        while (repetitions < 4096) {
            double start = Console::nowMicros();
            for (int i = 0; i < repetitions; ++i) {
                scratch = sample;
                algorithm.runSilently(scratch);
            }
            elapsed = Console::nowMicros() - start;
            if (elapsed >= 20000.0) break;
            repetitions *= 2;
        }
        double baselineStart = Console::nowMicros();
        for (int i = 0; i < repetitions; ++i) scratch = sample;
        double baseline = Console::nowMicros() - baselineStart;

        double perRun = (elapsed - baseline) / repetitions;
        return perRun > 0.0 ? perRun : 0.0;
    }

    /*
     * Bars on consecutive lines touch, so a column of identically coloured
     * bars reads as one solid staircase instead of five separate rows.
     * Alternating two tones keeps each bar legible without spending an extra
     * blank line between them.  The winner keeps its own colour.
     */
    static BarRole barTone(size_t index, bool highlight) {
        if (highlight) return ROLE_SORTED;
        return (index % 2 == 0) ? ROLE_ACTIVE : ROLE_PIVOT;
    }

    void showBenchmarkResults(const std::vector<BenchmarkResult>& results, int size) const {
        const Glyphs& g = theme_.glyphs();
        header("BENCHMARK RESULTS", "n = " + std::to_string(size) + ", identical random input for all five");

        std::ostringstream head;
        head << "   " << theme_.accent() << util::padRight("ALGORITHM", 17)
             << util::padLeft("COMPARISONS", 14) << "   "
             << util::padLeft("SWAPS", 12) << "   "
             << util::padLeft("TIME", 13);
        line(head.str());
        line("   " + theme_.dim() + util::repeat(g.horizontal, 63));

        double slowest = 0.0;
        long long mostComparisons = 0;
        for (size_t i = 0; i < results.size(); ++i) {
            if (results[i].microseconds > slowest)      slowest = results[i].microseconds;
            if (results[i].comparisons  > mostComparisons) mostComparisons = results[i].comparisons;
        }
        if (slowest <= 0.0) slowest = 1.0;

        /* Fastest result gets the green highlight. */
        size_t fastest = 0;
        for (size_t i = 1; i < results.size(); ++i)
            if (results[i].microseconds < results[fastest].microseconds) fastest = i;

        for (size_t i = 0; i < results.size(); ++i) {
            std::ostringstream row;
            row << "   " << (i == fastest ? theme_.ok() : theme_.text())
                << util::padRight(results[i].name, 17)
                << util::padLeft(util::withCommas(results[i].comparisons), 14) << "   "
                << util::padLeft(util::withCommas(results[i].swaps), 12) << "   "
                << util::padLeft(util::formatDuration(results[i].microseconds), 13);
            if (i == fastest) row << theme_.ok() << "   <- fastest";
            line(row.str());
        }
        line();

        line("   " + theme_.accent() + "RELATIVE RUNNING TIME");
        line();
        for (size_t i = 0; i < results.size(); ++i) {
            int blocks = static_cast<int>((results[i].microseconds / slowest) * 44.0 + 0.5);
            if (blocks < 1) blocks = 1;
            std::ostringstream row;
            row << "     " << theme_.dim() << util::padRight(results[i].name, 17)
                << theme_.barColour(barTone(i, i == fastest))
                << util::repeat(g.block, blocks)
                << theme_.dim() << "  " << util::formatDuration(results[i].microseconds);
            line(row.str());
        }
        line();

        line("   " + theme_.accent() + "RELATIVE COMPARISON COUNT");
        line();
        if (mostComparisons <= 0) mostComparisons = 1;
        for (size_t i = 0; i < results.size(); ++i) {
            int blocks = static_cast<int>(
                (static_cast<double>(results[i].comparisons) /
                 static_cast<double>(mostComparisons)) * 44.0 + 0.5);
            if (blocks < 1) blocks = 1;
            std::ostringstream row;
            row << "     " << theme_.dim() << util::padRight(results[i].name, 17)
                << theme_.barColour(barTone(i, false)) << util::repeat(g.block, blocks)
                << theme_.dim() << "  " << util::withCommas(results[i].comparisons);
            line(row.str());
        }
        line();
        line("   " + theme_.dim() +
             "Note: timings are an average of many repeated runs, with the cost of");
        line("   " + theme_.dim() +
             "resetting the input measured separately and subtracted.");
        line();
        input_.pause();
    }

    /* ---- settings ---------------------------------------------------------- */
    void settingsMenu() {
        while (true) {
            header("SETTINGS", "Tune the visualisation to your liking");

            statLine("Animation speed", viz_.currentSpeedName() + "  (" +
                     std::to_string(viz_.currentDelay()) + " ms per frame)");
            statLine("Step mode",       viz_.stepMode() ? "ON - advance one frame per key press" : "OFF");
            statLine("Theme",           theme_.modeName() + " mode");
            statLine("Character set",   theme_.asciiMode() ? "ASCII only (maximum compatibility)"
                                                           : "Unicode block graphics");
            line();
            sectionRule();
            line();
            menuItem(1, "Change animation speed");
            menuItem(2, "Toggle step mode",  viz_.stepMode() ? "turn OFF" : "turn ON");
            menuItem(3, "Toggle dark / light theme",
                     theme_.mode() == THEME_DARK ? "switch to Light" : "switch to Dark");
            menuItem(4, "Toggle Unicode / ASCII bars",
                     theme_.asciiMode() ? "switch to Unicode" : "switch to ASCII");
            line();
            menuItem(0, "Back to main menu");
            line();

            int choice = input_.readChoice(0, 4);
            if (choice == 0) return;
            switch (choice) {
                case 1: speedMenu();                                   break;
                case 2: viz_.setStepMode(!viz_.stepMode());            break;
                case 3: theme_.toggleMode();                           break;
                case 4: theme_.setAsciiMode(!theme_.asciiMode());      break;
            }
        }
    }

    void speedMenu() {
        header("ANIMATION SPEED", "How long each frame stays on screen");
        for (int i = 0; i < Visualizer::speedCount(); ++i) {
            std::ostringstream hint;
            hint << Visualizer::speedDelay(i) << " ms per frame";
            if (i == viz_.speedIndex()) hint << "   <- current";
            menuItem(i + 1, Visualizer::speedName(i), hint.str());
        }
        line();
        line("   " + theme_.dim() +
             "You can also change speed mid animation with the + and - keys.");
        line();
        menuItem(0, "Back");
        line();

        int choice = input_.readChoice(0, Visualizer::speedCount());
        if (choice == 0) return;
        viz_.setSpeedIndex(choice - 1);
    }

    /* ---- about -------------------------------------------------------------- */
    void showAbout() {
        header("ABOUT & HELP", "What each algorithm does, and how to drive this program");

        line("   " + theme_.accent() + "SORTING");
        line();
        for (size_t i = 0; i < sorts_.size(); ++i) describe(*sorts_[i]);

        line("   " + theme_.accent() + "SEARCHING");
        line();
        for (size_t i = 0; i < searches_.size(); ++i) describe(*searches_[i]);

        sectionRule();
        line();
        line("   " + theme_.accent() + "CONTROLS DURING AN ANIMATION");
        line();
        controlHelp("Space", "pause and resume");
        controlHelp("+ / -", "speed the animation up or slow it down");
        controlHelp("S",     "step mode: one frame per key press");
        controlHelp("Q",     "abort the animation (results are still computed)");
        line();
        line("   " + theme_.dim() +
             "Every algorithm is hand written - the STL is used for containers");
        line("   " + theme_.dim() +
             "and utilities only, never to do the actual sorting or searching.");
        line();
        input_.pause();
    }

    void describe(const Algorithm& algorithm) const {
        Complexity c = algorithm.complexity();
        line("   " + theme_.title() + util::padRight(algorithm.name(), 17) +
             theme_.dim() + "best " + util::padRight(c.best, 12) +
             "avg " + util::padRight(c.average, 12) + "worst " + c.worst);
        line("   " + theme_.text() + "  " + algorithm.description());
        line();
    }

    void controlHelp(const std::string& key, const std::string& meaning) const {
        line("     " + theme_.accent() + util::padRight("[" + key + "]", 10) +
             theme_.text() + meaning);
    }

    /* ---- state -------------------------------------------------------------- */
    Theme                 theme_;
    Visualizer            viz_;
    InputReader           input_;
    std::vector<int>      data_;
    std::mt19937          generator_;
    std::vector<std::unique_ptr<SortingAlgorithm> >   sorts_;
    std::vector<std::unique_ptr<SearchingAlgorithm> > searches_;
    SortingAlgorithm*                                 utilitySort_;
};

const int DsaVisualizerApp::kMargin;

/* ===========================================================================
 * SECTION 10 : ENTRY POINT
 * ===========================================================================
 */
int main() {
    ConsoleGuard guard;                 // restores the console on every exit path

    try {
        DsaVisualizerApp application;
        application.run();
    }
    catch (const ExitApplication&) {
        /* stdin closed (Ctrl+Z / Ctrl+D or a piped script) - a normal exit. */
        std::cout << "\n\nInput stream closed - exiting.\n";
    }
    catch (const std::exception& error) {
        std::cout << "\n\nUnexpected error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
