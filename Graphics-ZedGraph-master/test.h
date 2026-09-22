// ode_lab.hpp
// Лаба: РК4 для задачи Коши. Этап 1 — без контроля погрешности (постоянный шаг).
// Схема (7) — классический РК4.
// Сборка: g++ -std=c++17 -O2 main.cpp -o ode_lab

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
// 1. Интерфейс правой части: du/dx = f(x, u)
// ============================================================
class OdeFunction {
public:
    virtual ~OdeFunction() = default;
    virtual std::vector<double> operator()(double x,
                                           const std::vector<double>& u) const = 0;
    virtual int dimension() const = 0;
};

// ============================================================
// 2. Тестовая задача: du/dx = (-1)^N * (N/2) * u,  u(0) = u0
// ============================================================
class TestFunction : public OdeFunction {
    double k_;
public:
    explicit TestFunction(int variant)
        : k_((variant % 2 ? -1.0 : 1.0) * variant / 2.0) {}

    int dimension() const override { return 1; }

    std::vector<double> operator()(double /*x*/,
                                   const std::vector<double>& u) const override {
        return { k_ * u[0] };
    }

    double k() const { return k_; }

    // точное решение: u(x) = u0 * exp(k*x)
    double exact(double x, double u0) const { return u0 * std::exp(k_ * x); }
};

// ============================================================
// 3. Один шаг РК4 — классическая схема (7)
// ============================================================
class RungeKutta4 {
public:
    static std::vector<double> step(const OdeFunction& f,
                                    double x,
                                    const std::vector<double>& u,
                                    double h)
    {
        const std::size_t n = u.size();
        std::vector<double> k1(n), k2(n), k3(n), k4(n), tmp(n), res(n);

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
// 4. Структуры данных
// ============================================================
struct SolverConfig {
    double a    = 0.0;
    double b    = 1.0;
    double h0   = 0.1;
    int    Nmax = 1000000;
};

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
// 5. Интегратор (постоянный шаг)
// ============================================================
class Solver {
public:
    using ExactFn = std::function<std::vector<double>(double)>;

    Solver(const OdeFunction& f, SolverConfig cfg, ExactFn exact = nullptr)
        : f_(f), cfg_(cfg), exact_(std::move(exact)) {}

    void setInitial(const std::vector<double>& u0) { initial_ = u0; }

    void run() {
        table_.clear();
        stats_ = SolverStats{};

        if (initial_.empty())
            initial_.assign(f_.dimension(), 0.0);

        const double h = cfg_.h0;
        double x = cfg_.a;
        std::vector<double> u = initial_;
        int i = 0;

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

        while (x < cfg_.b - 1e-15) {
            if (i >= cfg_.Nmax) { stats_.hitNmax = true; break; }

            double hStep = h;
            if (x + hStep > cfg_.b) hStep = cfg_.b - x;
            if (hStep <= 0.0) break;

            std::vector<double> v  = RungeKutta4::step(f_, x, u, hStep);
            std::vector<double> vh = RungeKutta4::step(f_, x, u, 0.5 * hStep);
            std::vector<double> v2 = RungeKutta4::step(f_, x + 0.5 * hStep, vh, 0.5 * hStep);

            double olp = 0.0;
            std::vector<double> diff(v.size());
            for (std::size_t j = 0; j < v.size(); ++j) {
                diff[j] = v[j] - v2[j];
                olp = std::max(olp, std::abs(diff[j]));
            }

            StepRecord rec;
            rec.i    = i + 1;
            rec.x    = x + hStep;
            rec.v    = v;
            rec.v2   = v2;
            rec.diff = diff;
            rec.olp  = olp;
            rec.h    = hStep;
            rec.c1   = 0;
            rec.c2   = 0;
            if (exact_) rec.u_exact = exact_(x + hStep);
            table_.push_back(rec);

            u = v;
            x += hStep;
            ++i;
        }

        finalizeStats();
    }

    const std::vector<StepRecord>& table() const { return table_; }
    const SolverStats& stats() const { return stats_; }

private:
    const OdeFunction& f_;
    SolverConfig cfg_;
    ExactFn exact_;
    std::vector<double> initial_;
    std::vector<StepRecord> table_;
    SolverStats stats_;

    void finalizeStats() {
        stats_.n = static_cast<int>(table_.size()) - 1;
        stats_.b_minus_xn = cfg_.b - table_.back().x;
        stats_.reachedB = std::abs(stats_.b_minus_xn) < 1e-9;
        stats_.hitNmax = (stats_.n >= cfg_.Nmax);

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
        stats_.totalC1 = 0;
        stats_.totalC2 = 0;
    }
};

// ============================================================
// 6. Вывод таблиц и статистики
// ============================================================
class CsvWriter {
public:
    static void writeTestTable(const std::string& path,
                               const std::vector<StepRecord>& rows)
    {
        std::ofstream out(path);
        out << std::setprecision(10);
        out << "i;xi;vi;v2i;vi-v2i;OLP;hi;C1;C2;ui\n";
        for (const auto& r : rows) {
            out << r.i << ';'
                << r.x << ';'
                << r.v[0] << ';'
                << r.v2[0] << ';'
                << r.diff[0] << ';'
                << r.olp << ';'
                << r.h << ';'
                << r.c1 << ';'
                << r.c2 << ';';
            if (!r.u_exact.empty()) out << r.u_exact[0];
            out << '\n';
        }
    }

    static void writeStats(const std::string& path, const SolverStats& s,
                           bool withExact = false)
    {
        std::ofstream out(path);
        out << std::setprecision(10);
        out << "n = " << s.n << '\n';
        out << "b - x_n = " << s.b_minus_xn << '\n';
        out << "max |OLP| = " << s.maxOlp
            << " at x = " << s.maxOlp_atX << '\n';
        out << "total C1 (deleniy)  = " << s.totalC1 << '\n';
        out << "total C2 (udvoeniy) = " << s.totalC2 << '\n';
        out << "max h_i = " << s.maxH << " at x = " << s.maxH_atX << '\n';
        out << "min h_i = " << s.minH << " at x = " << s.minH_atX << '\n';
        out << "reached b: " << (s.reachedB ? "yes" : "no") << '\n';
        out << "hit Nmax:  " << (s.hitNmax ? "yes" : "no") << '\n';
        if (withExact) {
            out << "max |u_i - v_i| = " << s.maxExactErr
                << " at x = " << s.maxExactErr_atX << '\n';
        }
    }
};

// ============================================================
// 7. main-сценарий
// ============================================================
inline int runLab() {
    const int    variant = 2;
    const double u0      = 1.0;
    const double a       = 0.0;
    const double b       = 1.0;
    const double h       = 0.1;
    const int    Nmax    = 1000000;

    TestFunction test(variant);
    std::cout << "Test: k = " << test.k() << "\n";

    auto exactTest = [&](double x) {
        return std::vector<double>{ test.exact(x, u0) };
    };

    SolverConfig cfg{ a, b, h, Nmax };
    Solver s(test, cfg, exactTest);
    s.setInitial({ u0 });
    s.run();

    CsvWriter::writeTestTable("test_table_fixed.csv", s.table());
    CsvWriter::writeStats("test_stats_fixed.txt", s.stats(), true);

    std::cout << "n = " << s.stats().n
              << ", b - x_n = " << s.stats().b_minus_xn
              << ", max|OLP| = " << s.stats().maxOlp
              << ", max|u-v| = " << s.stats().maxExactErr << "\n";
    std::cout << "Done.\n";
    return 0;
}