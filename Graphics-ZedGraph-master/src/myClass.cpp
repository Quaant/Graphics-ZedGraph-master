#include "myClass.h"

#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

// g++ -std=c++17 -O2 -Iinclude -o myClass src/myClass.cpp
// ./myClass Nmax b a_g b_g [fixed|adaptive|both]


using namespace std;
namespace fs = filesystem;

namespace {

const string kResultsDir = "results";

string resultPath(const string& name)
{
    return (fs::path(kResultsDir) / name).string();
}

template <typename T>
T ask(const string& prompt, T def)
{
    cout << prompt << " [" << def << "]: ";
    string line;
    if (!getline(cin, line) || line.empty())
        return def;

    for (char& c : line)
        if (c == ',')
            c = '.';

    stringstream ss(line);
    T v{};
    if (!(ss >> v))
        return def;
    return v;
}

void printStats(const string& title, const SolverStats& s, bool withExact)
{
    cout << "\n=== " << title << " ===\n";
    cout << setprecision(10);
    cout << "n = "           << s.n << '\n';
    cout << "b - x_n = "     << s.b_minus_xn << '\n';
    cout << "max |OLP| = "   << s.maxOlp << " at x = " << s.maxOlp_atX << '\n';
    cout << "total C1 = "    << s.totalC1 << ", total C2 = " << s.totalC2 << '\n';
    cout << "max h = "       << s.maxH << " at x = " << s.maxH_atX << '\n';
    cout << "min h = "       << s.minH << " at x = " << s.minH_atX << '\n';
    cout << "reached b: "    << (s.reachedB ? "yes" : "no") << '\n';
    cout << "hit Nmax:  "    << (s.hitNmax ? "yes" : "no") << '\n';
    if (withExact) {
        cout << "max |u - v| = " << s.maxExactErr
             << " at x = "       << s.maxExactErr_atX << '\n';
    }
}

enum class TableKind { Test, Main };

void saveRun(const string& title,
             const string& prefix,
             const Solver& s,
             bool withExact,
             TableKind kind)
{
    printStats(title, s.stats(), withExact);
    if (kind == TableKind::Test)
        CsvWriter::writeTestTable(resultPath(prefix + "_table.csv"), s.table());
    else
        CsvWriter::writeMainTable(resultPath(prefix + "_table.csv"), s.table());
    CsvWriter::writeStats(resultPath(prefix + "_stats.txt"), s.stats(), withExact);
}

using ExactFn = function<vector<double>(double)>;

enum class StepMode { Fixed, Adaptive, Both };

string toLower(string s)
{
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

StepMode parseStepMode(string raw)
{
    raw = toLower(std::move(raw));
    if (raw == "fixed" || raw == "f" || raw == "0")
        return StepMode::Fixed;
    if (raw == "adaptive" || raw == "adapt" || raw == "a" || raw == "1")
        return StepMode::Adaptive;
    if (raw == "both" || raw == "all" || raw == "2")
        return StepMode::Both;
    throw invalid_argument("режим должен быть fixed, adaptive или both");
}

void runModes(defFunction& task,
              const SolverConfig& baseCfg,
              const vector<double>& u0,
              const string& titleBase,
              const string& prefix,
              bool withExact,
              TableKind kind,
              StepMode mode,
              ExactFn exact = nullptr)
{
    vector<bool> modes;
    if (mode != StepMode::Adaptive)
        modes.push_back(false);
    if (mode != StepMode::Fixed)
        modes.push_back(true);

    for (bool adaptive : modes) {
        SolverConfig c = baseCfg;
        c.adaptive = adaptive;
        const string modeName = adaptive ? "adaptive" : "fixed";

        Solver s(task, c, exact);
        s.setInitial(u0);
        s.run();
        saveRun(titleBase + " [" + modeName + "]", prefix + "_" + modeName, s, withExact, kind);
    }
}

} // namespace

int main(int argc, char** argv)
{
    int    Nmax   = 1000000;
    double xRight = 1.0;
    double a_g    = 1.0;
    double b_g    = 1.0;
    string modeStr = "adaptive";

    if (argc >= 5) {
        try {
            Nmax   = stoi(argv[1]);
            xRight = stod(argv[2]);
            a_g    = stod(argv[3]);
            b_g    = stod(argv[4]);
            if (argc >= 6)
                modeStr = argv[5];
        } catch (const exception& e) {
            cerr << "Ошибка разбора аргументов: " << e.what() << "\n"
                 << "Использование: " << argv[0]
                 << " Nmax b a_g b_g [fixed|adaptive|both]\n";
            return 1;
        }
    } else {
        cout << "Параметры (Enter — значение по умолчанию).\n"
             << "Аргументы: " << argv[0]
             << " Nmax b a_g b_g [fixed|adaptive|both]\n\n";
        Nmax   = ask("Nmax (макс. число шагов)", Nmax);
        xRight = ask("Правая граница b",        xRight);
        a_g    = ask("Параметр a функции g",    a_g);
        b_g    = ask("Параметр b функции g",    b_g);
    }

    if (argc < 6)
        modeStr = ask("Режим (fixed / adaptive / both)", modeStr);

    StepMode mode;
    try {
        mode = parseStepMode(modeStr);
    } catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    const double xLeft   = 0.0;
    const double h0      = 0.1;
    const double eps     = 1e-6;
    const double u0      = 1.0;
    const int    variant = 2;

    if (xRight <= xLeft) {
        cerr << "Ошибка: b должно быть больше a\n";
        return 1;
    }
    if (h0 <= 0.0) {
        cerr << "Ошибка: h0 должно быть > 0\n";
        return 1;
    }
    if (Nmax <= 0) {
        cerr << "Ошибка: Nmax должно быть > 0\n";
        return 1;
    }

    try {
        fs::create_directories(kResultsDir);
    } catch (const exception& e) {
        cerr << "Не удалось создать папку " << kResultsDir << ": " << e.what() << "\n";
        return 1;
    }

    SolverConfig cfg;
    cfg.a        = xLeft;
    cfg.b        = xRight;
    cfg.h0       = h0;
    cfg.Nmax     = Nmax;
    cfg.eps      = eps;
    cfg.adaptive = false;

    cout << "\nИнтервал: [" << cfg.a << ", " << cfg.b << "]"
         << ", h0 = "  << cfg.h0
         << ", Nmax = " << cfg.Nmax
         << ", eps = "  << cfg.eps << "\n";
    cout << "g(x, u, u') = a*u' + b*sin(u),  a = " << a_g
         << ", b = " << b_g << "\n";
    cout << "Режим шага: "
         << (mode == StepMode::Fixed ? "fixed"
             : mode == StepMode::Adaptive ? "adaptive" : "both")
         << "\n";
    cout << "Результаты: " << fs::absolute(kResultsDir) << "\n";

    try {
        TestFunction test(variant);
        cout << "\nТест: k = " << test.val() << "\n"; // u' = k*u, k = (-1)^N * (N/2)
        auto exactTest = [&](double x) {
            return vector<double>{ test.exact(x, u0) };
        };
        runModes(test, cfg, { u0 }, "Тест", "test", true, TableKind::Test, mode, exactTest);

        auto f = [](double x) { return x / (1.0 + x * x); };
        MainTask1 task1(f);
        runModes(task1, cfg, { u0 }, "Задача №1", "task1", false, TableKind::Main, mode);

        auto g = [a_g, b_g](double /*x*/, double u, double du) {
            return a_g * du + b_g * sin(u);
        };
        MainTask2 task2(g);
        runModes(task2, cfg, { u0, u0 }, "Задача №2", "task2", false, TableKind::Main, mode);
    } catch (const exception& e) {
        cerr << "Ошибка при расчёте или записи: " << e.what() << "\n";
        return 1;
    }

    cout << "\n=== Готово ===\n"
         << "Файлы в папке " << kResultsDir << " (*_table.csv и *_stats.txt)\n";
    return 0;
}
