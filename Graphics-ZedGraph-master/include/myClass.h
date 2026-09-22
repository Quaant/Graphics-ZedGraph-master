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

class defFunction{ 
    public:
    virtual vector<double> operator()(double x, vector<double>&u) = 0; 
    virtual int razmernost() = 0; 
};

class TestFunction : public defFunction{ 
    double value; 
    public: 
    TestFunction(){ 
        value = 1; 
    }
    int razmernost() override 
    {
        return 1;
    }
    vector <double> operator()(double, vector<double> &u) override{ 
        return {value * u[0]}; 
    }
    double val(){return value;}   

    double exact(double x, double u0) {return u0 * exp(value * x);}
};

class MainTask1 : public defFunction{
    function<double(double)> f_;

    MainTask1(std::function<double(double)> f) : f_(std::move(f)) {}

    int razmernost(){ 
        return 1; 
    }    

    vector<double> operator()(double x, vector<double>& u)  override {
        const double v = u[0];
        const double du = f_(x) * v * v + v - v * v * v * std::sin(10.0 * x);
        return { du };
    }
}; 

class MainTask2 : public defFunction {
    function<double(double, double, double)> g_;
public:
    MainTask2(std::function<double(double, double, double)> g) : g_(std::move(g)) {}

    int razmernost() override { return 2; }
     
    //svodim k sisteme 1-go porydka 
    // y1 = u
    // y2 = u'= y1' 
    // y2' = -g(x,y1,y2)

    vector<double> operator()(double x,
                                   vector<double>& u)  override {
        const double y1 = u[0];   // u
        const double y2 = u[1];   // u'
        return { y2, -g_(x, y1, y2) };
    }
};
//shema (7) iz uchebnika 
class RK4{ 
    public: 
    static vector <double> step(defFunction& f, double x, vector<double>&u, double h){ 
        size_t n = u.size();
        vector<double> k1(n), k2(n), k3(n), k4(n), tmp(n), res(n);
        // k1 = f(x, v)
        k1 = f(x, u);

        // k2 = f(x + h/2, v + h/2 * k1)
        for (std::size_t i = 0; i < n; ++i) tmp[i] = u[i] + 0.5 * h * k1[i];
        k2 = f(x + 0.5 * h, tmp);

        // k3 = f(x + h/2, v + h/2 * k2)
        for (std::size_t i = 0; i < n; ++i) tmp[i] = u[i] + 0.5 * h * k2[i];
        k3 = f(x + 0.5 * h, tmp);

        // k4 = f(x + h, v + h * k3)
        for (std::size_t i = 0; i < n; ++i) tmp[i] = u[i] + h * k3[i];
        k4 = f(x + h, tmp);

        // v_new = v + h/6 * (k1 + 2*k2 + 2*k3 + k4)
        for (std::size_t i = 0; i < n; ++i)
            res[i] = u[i] + (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);

        return res;
    }
};

//parametri zadavaimei polzovatelem 
struct SolverConfig {
    double a    = 0.0;
    double b    = 1.0;
    double h0   = 0.1;
    int    Nmax = 1000000;
};
//tablica 
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
//statistika 
struct SolverStats {
    int    n          = 0;
    double b_minus_xn = 0.0;
    double maxOlp     = 0.0;
    double maxOlp_atX = 0.0; //pri kakom x dostigli maxOlp 
    double maxH       = 0.0;
    double maxH_atX   = 0.0;//pri kakom x dostigli max H
    double minH       = 0.0;
    double minH_atX   = 0.0;
    int    totalC1    = 0;
    int    totalC2    = 0;
    bool   reachedB   = false;// dostigli pravuju granicu 
    bool   hitNmax    = false; // dostigli li Nmax
    double maxExactErr    = 0.0;
    double maxExactErr_atX = 0.0;
};


class Solver{ 
    using ExactFn = function<vector <double>(double)>;
    defFunction& f_; // ssilka na pravuju chast' 
    SolverConfig cfg_; // nastroiki 
    ExactFn exact_; // tochnoe reshenie 
    vector<double> initial_;// nachalnoe uslovie u0 
    vector<StepRecord> table_;// tablica
    SolverStats stats_;//statistika 

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


    public: 


    Solver(defFunction& f, SolverConfig cfg, ExactFn exact = nullptr) : f_(f), cfg_(cfg), exact_(exact) {} 

    void setInitial(vector<double> u0){ 
        initial_ = u0; 
    }
    
    void run(){ 
        table_.clear(); 
        stats_ = SolverStats{}; 
        if(initial_.empty()){ 
            initial_.assign(f_.razmernost(), 0.); 
        }
        double h = cfg_.h0; 
        vector <double> u = initial_; 
        double x = cfg_.a; 
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
//==============================================================================//
            // dalshe ne razbiral 
            vector<double> v  = RK4::step(f_, x, u, hStep);
            vector<double> vh = RK4::step(f_, x, u, 0.5 * hStep);
            vector<double> v2 = RK4::step(f_, x + 0.5 * hStep, vh, 0.5 * hStep);
        
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
}
 