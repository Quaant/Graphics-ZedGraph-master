#pragma once 
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std; 

// ============================================================
// Базовый интерфейс правой части
// ============================================================
class defFunction {
public:
    // БЫЛО: virtual vector<double> operator()(double x, vector<double>&u) = 0;
    //       virtual int razmernost() = 0;
    // СТАЛО: добавили virtual ~defFunction() = default;
    // ПОЧЕМУ: без виртуального деструктора удаление через указатель на базу — UB.
    //         Для лабы не критично, но правильно.
    virtual ~defFunction() = default;

    virtual vector<double> operator()(double x, vector<double>& u) = 0;
    virtual int razmernost() = 0;
};

// ============================================================
// Тестовая задача: du/dx = (-1)^N * (N/2) * u
// Для варианта 2: k = 1, u(x) = u0 * exp(x)
// ============================================================
class TestFunction : public defFunction {
    double value;
public:
    // БЫЛО: TestFunction(){ value = 1; }
    // СТАЛО: explicit TestFunction(int variant = 2)
    //            : value((variant % 2 ? -1.0 : 1.0) * variant / 2.0) {}
    // ПОЧЕМУ: жёсткое value = 1 работает только для варианта 2.
    //         Параметризация позволяет проверить любой вариант.
    //         По умолчанию variant = 2 — чтобы старый код не сломался.
    explicit TestFunction(int variant = 2)
        : value((variant % 2 ? -1.0 : 1.0) * variant / 2.0) {}

    int razmernost() override {
        return 1;
    }

    vector<double> operator()(double, vector<double>& u) override {
        return {value * u[0]};
    }

    double val() { return value; }

    double exact(double x, double u0) { return u0 * exp(value * x); }
};

// ============================================================
// Основная задача №1: du/dx = f(x)*u^2 + u - u^3*sin(10x)
// ============================================================
class MainTask1 : public defFunction {
    function<double(double)> f_;

// БЫЛО: конструктор и методы были без public: — класс по умолчанию private.
// СТАЛО: добавили public:
// ПОЧЕМУ: без этого MainTask1 нельзя создать снаружи — компилятор не даст
//         вызвать конструктор, и объект не построится.
public:
    explicit MainTask1(std::function<double(double)> f) : f_(std::move(f)) {}

    // БЫЛО: int razmernost(){ return 1; }
    // СТАЛО: int razmernost() override { return 1; }
    // ПОЧЕМУ: override — страховка от опечатки в сигнатуре. Если базовый метод
    //         изменится, компилятор сообщит, что override не найден.
    int razmernost() override {
        return 1;
    }

    vector<double> operator()(double x, vector<double>& u) override {
        const double v = u[0];
        const double du = f_(x) * v * v + v - v * v * v * std::sin(10.0 * x);
        return { du };
    }
};

// ============================================================
// Основная задача №2: u'' + g(x, u, u') = 0
// Сводим к системе: y1 = u, y2 = u'
//   y1' = y2
//   y2' = -g(x, y1, y2)
// ============================================================
class MainTask2 : public defFunction {
    function<double(double, double, double)> g_;
public:
    explicit MainTask2(std::function<double(double, double, double)> g)
        : g_(std::move(g)) {}

    int razmernost() override { return 2; }

    vector<double> operator()(double x, vector<double>& u) override {
        const double y1 = u[0];   // u
        const double y2 = u[1];   // u'
        return { y2, -g_(x, y1, y2) };
    }
};

// ============================================================
// Схема (7) из учебника: классический РК4
// ============================================================
class RK4 {
public:
    static vector<double> step(defFunction& f, double x, vector<double>& u, double h) {
        size_t n = u.size();
        vector<double> k1(n), k2(n), k3(n), k4(n), tmp(n), res(n);

        k1 = f(x, u);

        for (std::size_t i = 0; i < n; ++i) tmp[i] = u[i] + 0.5 * h * k1[i];
        k2 = f(x + 0.5 * h, tmp);

        for (std::size_t i = 0; i < n; ++i) tmp[i] = u[i] + 0.5 * h * k2[i];
        k3 = f(x + 0.5 * h, tmp);

        for (std::size_t i = 0; i < n; ++i) tmp[i] = u[i] + h * k3[i];
        k4 = f(x + h, tmp);

        for (std::size_t i = 0; i < n; ++i)
            res[i] = u[i] + (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);

        return res;
    }
};

// ============================================================
// Параметры решателя
// ============================================================
struct SolverConfig {
    double a    = 0.0;
    double b    = 1.0;
    double h0   = 0.1;
    int    Nmax = 1000000;

    // БЫЛО: полей eps и adaptive не было.
    // СТАЛО: добавили.
    // ПОЧЕМУ: без них нельзя включить адаптивный режим и сравнивать OLP с допуском.
    //         Компилятор ругался на cfg_.eps и cfg_.adaptive в run().
    double eps      = 1e-6;
    bool   adaptive = false;
};

// ============================================================
// Строка таблицы
// ============================================================
struct StepRecord {
    int    i    = 0;
    double x    = 0.0;
    std::vector<double> v;
    std::vector<double> v2;
    std::vector<double> diff;
    double olp  = 0.0;
    double h    = 0.0;
    int    c1   = 0;
    int    c2   = 0;
    std::vector<double> u_exact;
};

// ============================================================
// Статистика
// ============================================================
struct SolverStats {
    int    n          = 0;
    double b_minus_xn = 0.0;
    double maxOlp     = 0.0;
    double maxOlp_atX = 0.0;
    double maxH       = 0.0;
    double maxH_atX   = 0.0;
    double minH       = 0.0;
    double minH_atX   = 0.0;
    int    totalC1    = 0;
    int    totalC2    = 0;
    bool   reachedB   = false;
    bool   hitNmax    = false;
    double maxExactErr    = 0.0;
    double maxExactErr_atX = 0.0;
};

// ============================================================
// Решатель
// ============================================================
class Solver {
    using ExactFn = function<vector<double>(double)>;
    defFunction& f_;
    SolverConfig cfg_;
    ExactFn exact_;
    vector<double> initial_;
    vector<StepRecord> table_;
    SolverStats stats_;

    void finalizeStats() {
        stats_.n = static_cast<int>(table_.size()) - 1;
        stats_.b_minus_xn = cfg_.b - table_.back().x;
        stats_.reachedB = std::abs(stats_.b_minus_xn) < 1e-9;

        // БЫЛО: stats_.hitNmax = (stats_.n >= cfg_.Nmax);
        // СТАЛО: stats_.hitNmax = stats_.hitNmax || (stats_.n >= cfg_.Nmax);
        // ПОЧЕМУ: run() уже мог выставить hitNmax = true при break по Nmax.
        //         Простое присваивание затирало флаг, если n < Nmax.
        stats_.hitNmax = stats_.hitNmax || (stats_.n >= cfg_.Nmax);

        bool firstH = true;
        for (const auto& r : table_) {
            if (r.i == 0) continue;
            if (r.olp > stats_.maxOlp) {
                stats_.maxOlp = r.olp;
                stats_.maxOlp_atX = r.x;
            }
            if (firstH || r.h > stats_.maxH) { stats_.maxH = r.h; stats_.maxH_atX = r.x; }
            if (firstH || r.h < stats_.minH) { stats_.minH = r.h; stats_.minH_atX = r.x; }
            firstH = false;

            if (!r.u_exact.empty() && !r.v.empty()) {
                double e = 0.0;
                for (std::size_t j = 0; j < r.v.size(); ++j)
                    e = std::max(e, std::abs(r.u_exact[j] - r.v[j]));
                if (e > stats_.maxExactErr) {
                    stats_.maxExactErr = e;
                    stats_.maxExactErr_atX = r.x;
                }
            }
        }
        // БЫЛО: stats_.totalC1 = 0;
        //       stats_.totalC2 = 0;
        // СТАЛО: эти строки удалены.
        // ПОЧЕМУ: run() уже накапливает totalC1/totalC2 через +=.
        //         Обнуление в конце затирало всю статистику.
    }

public:
    Solver(defFunction& f, SolverConfig cfg, ExactFn exact = nullptr)
        : f_(f), cfg_(cfg), exact_(exact) {}

    void setInitial(vector<double> u0) {
        initial_ = u0;
    }

    void run() {
        table_.clear();
        stats_ = SolverStats{};

        if (initial_.empty())
            // БЫЛО: initial_.assign(f_.dimension(), 0.0);
            // СТАЛО: initial_.assign(f_.razmernost(), 0.0);
            // ПОЧЕМУ: в базовом классе метод называется razmernost(), а не dimension().
            //         Компилятор не находил dimension() и падал с ошибкой.
            initial_.assign(f_.razmernost(), 0.0);

        double h = cfg_.h0;
        double x = cfg_.a;
        std::vector<double> u = initial_;
        int i = 0;

        // ---------- начальная запись (i = 0) ----------
        {
            StepRecord rec;
            rec.i = 0;
            rec.x = x;
            rec.v = u;
            rec.v2 = u;
            rec.diff.assign(u.size(), 0.0);
            rec.olp = 0.0;
            rec.h = 0.0;
            rec.c1 = 0;
            rec.c2 = 0;
            if (exact_) rec.u_exact = exact_(x);
            table_.push_back(rec);
        }

        // ---------- основной цикл по времени ----------
        while (x < cfg_.b - 1e-15) {
            if (i >= cfg_.Nmax) { stats_.hitNmax = true; break; }

            int c1 = 0;
            int c2 = 0;

            double hStep = h;
            if (x + hStep > cfg_.b) hStep = cfg_.b - x;
            if (hStep <= 0.0) break;

            std::vector<double> v, v2, diff;
            double olp = 0.0;

            // ---------- подбор шага ----------
            while (true) {
                if (x + hStep > cfg_.b) hStep = cfg_.b - x;
                if (hStep <= 0.0) break;

                v = RK4::step(f_, x, u, hStep);

                std::vector<double> vh = RK4::step(f_, x, u, 0.5 * hStep);
                v2 = RK4::step(f_, x + 0.5 * hStep, vh, 0.5 * hStep);

                diff.resize(v.size());
                double maxAbsDiff = 0.0;
                for (std::size_t j = 0; j < v.size(); ++j) {
                    diff[j] = v[j] - v2[j];
                    maxAbsDiff = std::max(maxAbsDiff, std::abs(diff[j]));
                }

                // БЫЛО: const double p = 4.0;
                //       olp = maxAbsDiff / (std::pow(2.0, p) - 1.0);
                // СТАЛО: olp = maxAbsDiff / 15.0;
                // ПОЧЕМУ: 2^4 - 1 = 15 — константа для РК4.
                //         std::pow работает, но избыточен и медленнее.
                //         Формула та же: S = |V - Ṽ| / (2^p - 1).
                olp = maxAbsDiff / 15.0;

                if (!cfg_.adaptive) break;

                if (olp > cfg_.eps) {
                    hStep *= 0.5;
                    c1++;
                    if (c1 > 100) break;
                    continue;
                }

                if (olp < cfg_.eps / 32.0) {
                    h = hStep * 2.0;
                    c2++;
                } else {
                    h = hStep;
                }
                break;
            }

            if (hStep <= 0.0) break;

            // ---------- принятый шаг: запись в таблицу ----------
            StepRecord rec;
            rec.i    = i + 1;
            rec.x    = x + hStep;
            rec.v    = v;
            rec.v2   = v2;
            rec.diff = diff;
            rec.olp  = olp;
            rec.h    = hStep;
            rec.c1   = c1;
            rec.c2   = c2;
            if (exact_) rec.u_exact = exact_(x + hStep);
            table_.push_back(rec);

            stats_.totalC1 += c1;
            stats_.totalC2 += c2;

            u = v;
            x += hStep;
            ++i;
        }

        finalizeStats();
    }

    const std::vector<StepRecord>& table() const { return table_; }
    const SolverStats& stats() const { return stats_; }

// БЫЛО: } (без точки с запятой)
// СТАЛО: };
// ПОЧЕМУ: класс в C++ заканчивается точкой с запятой.
//         Без неё — ошибка компиляции "expected ';' after class definition".
};