# Graph (Linux)

Порт окна `MyForm` (WinForms + ZedGraph) на Linux: Python + Tkinter + Matplotlib.

Исходный C++/CLI на Linux не собирается. GUI рисует график и таблицу; счёт лучше вынести в C++.

## Запуск

```bash
cd Graphics-ZedGraph-master
.venv/bin/python graph_linux.py
```

Если окружения нет:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python graph_linux.py
```

## Как пользоваться окном

| Элемент | Назначение |
|---|---|
| **a, b, h** (верхний ряд) | Интервал и шаг для расчёта |
| **Draw** | Считает точки, заполняет таблицу, рисует кривые |
| таблица **X, F_1, F_2** | Значения с округлением `floor(v * 1000) / 1000` |
| **a, b** (нижний ряд) + **Zoom** | Только масштаб оси X, без пересчёта |

Значения можно вводить с запятой (`0,1`) или с точкой (`0.1`).

Сейчас формулы внутри Python:

- `F1(x) = sin(x)` — красная линия, маркеры `+`
- `F2(x) = sin(2x)` — синяя линия без маркеров

После Draw ось X: `[a - 0.1, b + 0.1]`.

## Связка с вычислительным бэкендом на C++

Схема:

```
[окно Python]  -- a, b, h -->  [C++]  -- x, f1, f2 -->  [таблица + график]
```

По **Draw** Python не считает функции сам, а вызывает C++ и рисует ответ.
**Zoom** трогать не нужно.

Контракт:

- вход: `a`, `b`, `h` (`double`);
- выход: три массива одной длины;
- узлы: `x = a, a+h, a+2h, ...` пока `x <= b`;
- округление таблицы оставить в Python.

### Вариант 1. Программа + subprocess (проще)

C++ печатает строки `x f1 f2`. Сборка:

```bash
g++ -O2 -o compute compute.cpp
./compute 0 1 0.1
```

Вызов из Python:

```python
import subprocess

def compute(a, b, h):
    out = subprocess.check_output(
        ["./compute", str(a), str(b), str(h)],
        text=True,
    )
    xs, y1, y2 = [], [], []
    for line in out.splitlines():
        x, f1, f2 = map(float, line.split())
        xs.append(x)
        y1.append(f1)
        y2.append(f2)
    return xs, y1, y2
```

В `button1_click` вместо `self.f1` / `self.f2`:

```python
xs, y1, y2 = compute(xmin, xmax, h)
```

### Вариант 2. Библиотека `.so` + ctypes

```cpp
extern "C" int compute(double a, double b, double h,
                       double* x, double* f1, double* f2, int maxn);
```

```bash
g++ -shared -fPIC -O2 -o libcompute.so compute.cpp
```

Быстрее: без нового процесса и без парсинга текста.

### Вариант 3. pybind11

Имеет смысл, когда функций много. Для двух кривых достаточно вариантов 1–2.

HTTP/сокеты здесь не нужны.

## Порядок работ

1. Написать `compute.cpp`, проверить `./compute 0 1 0.1`.
2. Подставить вызов в **Draw**.
3. Дальше менять только C++, GUI не трогать.

## Файлы

| Файл | Что это |
|---|---|
| `graph_linux.py` | Окно Linux |
| `requirements.txt` | matplotlib |
| `MyForm.h` / `MyForm.cpp` | Старый Windows-проект |
