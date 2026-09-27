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

public:
    explicit MainTask1(std::function<double(double)> f) : f_(std::move(f)) {}

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

        for (size_t i = 0; i < n; ++i) tmp[i] = u[i] + 0.5 * h * k1[i];
        k2 = f(x + 0.5 * h, tmp);

        for (size_t i = 0; i < n; ++i) tmp[i] = u[i] + 0.5 * h * k2[i];
        k3 = f(x + 0.5 * h, tmp);

        for (size_t i = 0; i < n; ++i) tmp[i] = u[i] + h * k3[i];
        k4 = f(x + h, tmp);

        for (size_t i = 0; i < n; ++i)
            res[i] = u[i] + (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);

        return res;
    }
};

// ============================================================
// Параметры решателя задаваемые пользоваетелем 
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
    vector<double> v;
    vector<double> v2;
    vector<double> diff;
    double olp  = 0.0;
    double h    = 0.0;
    int    c1   = 0;
    int    c2   = 0;
    vector<double> u_exact;
};

// ============================================================
// Статистика
// ============================================================
struct SolverStats {
    int    n          = 0; //количество шагов 
    double b_minus_xn = 0.0; // сколько нехватило до правой границы 
    double maxOlp     = 0.0; // макс. значение олп 
    double maxOlp_atX = 0.0; //в каком х достигли максОЛП
    double maxH       = 0.0; //макс H 
    double maxH_atX   = 0.0;// в каком x макс H
    double minH       = 0.0; 
    double minH_atX   = 0.0;
    int    totalC1    = 0; // сколько раз за шаг сделали olp > eps 
    int    totalC2    = 0; // сколько раз за шаг сделали olp < eps / 32 
    bool   reachedB   = false; // достигли правой границы
    bool   hitNmax    = false; //пробили потолок по Nmax 
    double maxExactErr    = 0.0; //макс разность между u_exact и v 
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
        stats_.reachedB = abs(stats_.b_minus_xn) < 1e-9;
        
        stats_.hitNmax = stats_.hitNmax || (stats_.n >= cfg_.Nmax);

        bool firstH = true;
        for (const auto& r : table_) {
            if (r.i == 0) continue;
            if (r.olp > stats_.maxOlp) { // считаем maxOLP для статистики 
                stats_.maxOlp = r.olp;
                stats_.maxOlp_atX = r.x;
            }
            // считаем maxH для статистики 
            if (firstH || r.h > stats_.maxH) { stats_.maxH = r.h; stats_.maxH_atX = r.x; }
            if (firstH || r.h < stats_.minH) { stats_.minH = r.h; stats_.minH_atX = r.x; }
            firstH = false;

            //считаем максимальную ошибку 
            if (!r.u_exact.empty() && !r.v.empty()) {
                double e = 0.0;
                for (size_t j = 0; j < r.v.size(); ++j)
                    e = std::max(e, std::abs(r.u_exact[j] - r.v[j]));
                if (e > stats_.maxExactErr) {
                    stats_.maxExactErr = e;
                    stats_.maxExactErr_atX = r.x;
                }
            }
        }
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
            initial_.assign(f_.razmernost(), 0.0);

        double h = cfg_.h0;
        double x = cfg_.a;
        vector<double> u = initial_;
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
        while (x < cfg_.b - 1e-15) { // тут пиздуем пока не дойдем до
                                     // правой границы 
            if (i >= cfg_.Nmax) { stats_.hitNmax = true; break; } // проверяем что сделали н больше Nmax шагов 

            int c1 = 0;
            int c2 = 0;

            //не совсем выкупаю эти 2 условия 
            // типо первое для того чтобы если вышли за b, то поставим x в b(хз ваще зачем)
            // а второе типо шаг занулился, но я хз когда такое возможно
            double hStep = h;
            if (x + hStep > cfg_.b) hStep = cfg_.b - x;
            if (hStep <= 0.0) break;

            vector<double> v, v2, diff;
            double olp = 0.0;


// Идея: мы не знаем точного решения, поэтому не можем посчитать реальную
// локальную погрешность. Но можем получить ДВА численных приближения к
// одному и тому же значению u(x + h):
//
//   v  — одним шагом h          (грубее)
//   v2 — двумя шагами h/2       (точнее)
//
// По правилу Рунге для метода 4-го порядка:
//       |u_точное - v| ≈ |v - v2| / (2^4 - 1) = |v - v2| / 15
//
// Эту величину называем ОЛП (оценка локальной погрешности).
// Сравниваем её с допуском eps:
//
//   ОЛП >  eps       -> шаг ВЕЛИК: делим h пополам, пересчитываем (C1++)
//   ОЛП <  eps / 32  -> шаг МАЛ:   принимаем, но следующий шаг удваиваем (C2++)
//   иначе            -> шаг ХОРОШ: принимаем как есть
//
// Порог eps/32 (а не eps) нужен как гистерезис: иначе шаг будет "дрожать"
// на каждом шаге — то удваиваться, то делиться.
//
// Если adaptive == false, внутренний цикл делает ровно один проход и выходит.
// Это режим ПОСТОЯННОГО шага — шаг не меняется, C1 = C2 = 0.
            // ---------- подбор шага ----------
            while (true) {
                if (x + hStep > cfg_.b) hStep = cfg_.b - x;
                if (hStep <= 0.0) break;

                v = RK4::step(f_, x, u, hStep);  // полный шаг 

                vector<double> vh = RK4::step(f_, x, u, 0.5 * hStep); // первый полушаг 
                v2 = RK4::step(f_, x + 0.5 * hStep, vh, 0.5 * hStep); //второй полушаг 

                // оценка локальной погрешности 
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

                if (!cfg_.adaptive) break; //если брек, то шаг - постоянный 
                // адаптивный шаг
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

};

class CsvWriter {
public:
    // Таблица тестовой задачи: с колонкой ui (точное решение)
    static void writeTestTable(const std::string& path,
                               const std::vector<StepRecord>& rows)
    {
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Не удалось открыть файл: " + path);

        out << std::setprecision(10);
        out << "i;xi;vi;v2i;vi-v2i;OLP;hi;C1;C2;ui\n";

        for (const auto& r : rows) {
            if (r.v.size() != 1) {
                throw std::runtime_error(
                    "writeTestTable: ожидается скалярная задача (v.size()==1)");
            }
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

    // Таблица основной задачи: без ui; для системы — с колонками u и u'
    static void writeMainTable(const std::string& path,
                               const std::vector<StepRecord>& rows)
    {
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Не удалось открыть файл: " + path);

        out << std::setprecision(10);

        const bool isSystem = !rows.empty() && rows[0].v.size() > 1;

        out << "i;xi;vi;v2i;vi-v2i;OLP;hi;C1;C2";
        if (isSystem) out << ";u;u'";
        out << '\n';

        for (const auto& r : rows) {
            out << r.i << ';'
                << r.x << ';'
                << r.v[0] << ';'
                << r.v2[0] << ';'
                << r.diff[0] << ';'
                << r.olp << ';'
                << r.h << ';'
                << r.c1 << ';'
                << r.c2;
            if (isSystem) out << ';' << r.v[0] << ';' << r.v[1];
            out << '\n';
        }
    }

    // Сводка. withExact = true — печатать строку про max|u-v|
    static void writeStats(const std::string& path,
                           const SolverStats& s,
                           bool withExact = false)
    {
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Не удалось открыть файл: " + path);

        out << std::setprecision(10);
        out << "n = " << s.n << '\n';
        out << "b - x_n = " << s.b_minus_xn << '\n';
        out << "max |OLP| = " << s.maxOlp
            << " at x = " << s.maxOlp_atX << '\n';
        out << "total C1 (delenij)  = " << s.totalC1 << '\n';
        out << "total C2 (udvoenij) = " << s.totalC2 << '\n';
        out << "max h_i = " << s.maxH << " at x = " << s.maxH_atX << '\n';
        out << "min h_i = " << s.minH << " at x = " << s.minH_atX << '\n';
        out << "reached b: " << (s.reachedB ? "yes" : "no") << '\n';
        out << "hit Nmax:  " << (s.hitNmax  ? "yes" : "no") << '\n';
        if (withExact) {
            out << "max |u_i - v_i| = " << s.maxExactErr
                << " at x = " << s.maxExactErr_atX << '\n';
        }
    }
};