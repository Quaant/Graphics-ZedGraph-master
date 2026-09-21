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
//sfema (7) iz uchebnika 
class RK4{ 
    public: 
    vector <double> step(defFunction& f, double x, vector<double>&u, double h){ 
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


