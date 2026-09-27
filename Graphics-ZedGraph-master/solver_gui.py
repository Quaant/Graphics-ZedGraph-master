"""Графический интерфейс к ./myClass: параметры, запуск, таблица, графики."""

from __future__ import annotations

import csv
import os
import subprocess
import sys
import threading
from pathlib import Path
from tkinter import ttk, messagebox
import tkinter as tk

import matplotlib

matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure

ROOT = Path(__file__).resolve().parent
BIN = ROOT / "myClass"
RESULTS = ROOT / "results"
SRC = ROOT / "src" / "myClass.cpp"

TASK_PREFIX = {
    "Тест": "test",
    "Задача №1": "task1",
    "Задача №2": "task2",
}


def parse_double(text: str) -> float:
    return float(str(text).strip().replace(",", ".").replace(" ", ""))


def parse_int(text: str) -> int:
    return int(str(text).strip().replace(" ", ""))


def compile_solver() -> None:
    cmd = [
        "g++",
        "-std=c++17",
        "-O2",
        "-Iinclude",
        "-o",
        str(BIN),
        str(SRC),
    ]
    proc = subprocess.run(
        cmd,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or proc.stdout.strip() or "g++ failed")


def load_csv(path: Path) -> tuple[list[str], list[list[str]]]:
    with path.open(encoding="utf-8", newline="") as f:
        reader = csv.reader(f, delimiter=";")
        rows = list(reader)
    if not rows:
        return [], []
    return rows[0], rows[1:]


def as_float(value: str) -> float | None:
    if value is None or str(value).strip() == "":
        return None
    try:
        return float(str(value).replace(",", "."))
    except ValueError:
        return None


class SolverGui(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("РК4 — графики и таблица")
        self.geometry("1180x720")
        self.minsize(960, 640)

        self._busy = False
        self._headers: list[str] = []
        self._rows: list[list[str]] = []

        self._build()
        self.refresh_file_list()

    def _build(self) -> None:
        params = ttk.LabelFrame(self, text="Параметры ./myClass", padding=8)
        params.pack(fill="x", padx=8, pady=8)

        self.var_nmax = tk.StringVar(value="1000")
        self.var_b = tk.StringVar(value="1")
        self.var_ag = tk.StringVar(value="1")
        self.var_bg = tk.StringVar(value="1")
        self.var_mode = tk.StringVar(value="adaptive")
        self.var_task = tk.StringVar(value="Тест")
        self.var_file = tk.StringVar()

        def labeled(parent, text, var, width=10, column=0):
            ttk.Label(parent, text=text).grid(row=0, column=column, sticky="e", padx=(8, 4))
            ttk.Entry(parent, textvariable=var, width=width).grid(row=0, column=column + 1)

        labeled(params, "Nmax", self.var_nmax, 10, 0)
        labeled(params, "правая граница b", self.var_b, 10, 2)
        labeled(params, "a (из g)", self.var_ag, 8, 4)
        labeled(params, "b (из g)", self.var_bg, 8, 6)

        ttk.Label(params, text="режим").grid(row=0, column=8, padx=(12, 4))
        ttk.Combobox(
            params,
            textvariable=self.var_mode,
            values=("fixed", "adaptive", "both"),
            state="readonly",
            width=10,
        ).grid(row=0, column=9)

        self.btn_run = ttk.Button(params, text="Запуск", command=self.on_run)
        self.btn_run.grid(row=0, column=10, padx=12)

        params.columnconfigure(11, weight=1)

        ttk.Label(
            params,
            text="g(x, u, u') = a·u' + b·sin(u)     уравнение: u'' + g = 0",
        ).grid(row=1, column=0, columnspan=11, sticky="w", pady=(8, 0))

        view = ttk.Frame(self, padding=(8, 0))
        view.pack(fill="x", padx=8)

        ttk.Label(view, text="Задача").pack(side="left")
        task_box = ttk.Combobox(
            view,
            textvariable=self.var_task,
            values=tuple(TASK_PREFIX.keys()),
            state="readonly",
            width=12,
        )
        task_box.pack(side="left", padx=6)
        task_box.bind("<<ComboboxSelected>>", lambda _e: self.refresh_file_list())

        ttk.Label(view, text="файл").pack(side="left", padx=(12, 0))
        self.file_box = ttk.Combobox(
            view,
            textvariable=self.var_file,
            state="readonly",
            width=36,
        )
        self.file_box.pack(side="left", padx=6, fill="x", expand=True)
        self.file_box.bind("<<ComboboxSelected>>", lambda _e: self.load_selected())

        ttk.Button(view, text="Показать", command=self.load_selected).pack(side="left", padx=6)

        mid = ttk.Panedwindow(self, orient="horizontal")
        mid.pack(fill="both", expand=True, padx=8, pady=8)

        left = ttk.Frame(mid)
        right = ttk.Frame(mid)
        mid.add(left, weight=3)
        mid.add(right, weight=2)

        self.figure = Figure(figsize=(6.4, 4.2), dpi=100)
        self.ax = self.figure.add_subplot(111)
        self.ax.grid(True)
        self.ax.set_xlabel("x")
        self.ax.set_ylabel("u")
        self.canvas = FigureCanvasTkAgg(self.figure, master=left)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        toolbar = NavigationToolbar2Tk(self.canvas, left)
        toolbar.update()

        table_frame = ttk.Frame(right)
        table_frame.pack(fill="both", expand=True)

        self.table = ttk.Treeview(table_frame, show="headings")
        yscroll = ttk.Scrollbar(table_frame, orient="vertical", command=self.table.yview)
        xscroll = ttk.Scrollbar(table_frame, orient="horizontal", command=self.table.xview)
        self.table.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)
        self.table.grid(row=0, column=0, sticky="nsew")
        yscroll.grid(row=0, column=1, sticky="ns")
        xscroll.grid(row=1, column=0, sticky="ew")
        table_frame.rowconfigure(0, weight=1)
        table_frame.columnconfigure(0, weight=1)

        bottom = ttk.LabelFrame(self, text="Сводка и лог", padding=6)
        bottom.pack(fill="x", padx=8, pady=(0, 8))
        self.log = tk.Text(bottom, height=8, wrap="word")
        self.log.pack(fill="x")

        self.status = ttk.Label(self, text="Готово. Задайте параметры и нажмите «Запуск».")
        self.status.pack(fill="x", padx=8, pady=(0, 6))

    def log_write(self, text: str) -> None:
        self.log.insert("end", text.rstrip() + "\n")
        self.log.see("end")

    def set_busy(self, busy: bool) -> None:
        self._busy = busy
        self.btn_run.configure(state="disabled" if busy else "normal")
        self.status.configure(text="Счёт…" if busy else "Готово")

    def refresh_file_list(self, prefer: str | None = None) -> None:
        prefix = TASK_PREFIX[self.var_task.get()]
        files = sorted(RESULTS.glob(f"{prefix}_*_table.csv"))
        names = [p.name for p in files]
        self.file_box["values"] = names
        if prefer and prefer in names:
            self.var_file.set(prefer)
        elif names and self.var_file.get() not in names:
            self.var_file.set(names[0])
        elif not names:
            self.var_file.set("")

    def on_run(self) -> None:
        if self._busy:
            return
        try:
            nmax = parse_int(self.var_nmax.get())
            b = parse_double(self.var_b.get())
            ag = parse_double(self.var_ag.get())
            bg = parse_double(self.var_bg.get())
        except ValueError:
            messagebox.showerror("Ошибка", "Nmax, b, a, b должны быть числами")
            return
        if nmax <= 0:
            messagebox.showerror("Ошибка", "Nmax должно быть > 0")
            return
        if b <= 0:
            messagebox.showerror("Ошибка", "правая граница b должна быть > 0")
            return

        mode = self.var_mode.get()
        self.log.delete("1.0", "end")
        self.set_busy(True)
        threading.Thread(
            target=self._run_worker,
            args=(nmax, b, ag, bg, mode),
            daemon=True,
        ).start()

    def _run_worker(self, nmax: int, b: float, ag: float, bg: float, mode: str) -> None:
        try:
            if not BIN.exists():
                self.after(0, lambda: self.log_write("myClass не найден, собираю…"))
                compile_solver()
            cmd = [str(BIN), str(nmax), str(b), str(ag), str(bg), mode]
            self.after(0, lambda: self.log_write(" ".join(cmd)))
            proc = subprocess.run(
                cmd,
                cwd=ROOT,
                capture_output=True,
                text=True,
                env={**os.environ, "LC_ALL": "C"},
            )
            out = (proc.stdout or "") + (proc.stderr or "")
            if proc.returncode != 0:
                raise RuntimeError(out.strip() or f"код выхода {proc.returncode}")
            self.after(0, lambda: self._on_run_ok(out, mode))
        except Exception as exc:  # noqa: BLE001
            err = str(exc)
            self.after(0, lambda: self._on_run_fail(err))

    def _on_run_ok(self, out: str, mode: str) -> None:
        self.log_write(out)
        prefix = TASK_PREFIX[self.var_task.get()]
        if mode == "both":
            prefer = f"{prefix}_adaptive_table.csv"
        else:
            prefer = f"{prefix}_{mode}_table.csv"
        self.refresh_file_list(prefer=prefer)
        self.load_selected()
        self.set_busy(False)

    def _on_run_fail(self, err: str) -> None:
        self.log_write(err)
        self.set_busy(False)
        messagebox.showerror("Ошибка запуска", err)

    def load_selected(self) -> None:
        name = self.var_file.get().strip()
        if not name:
            self.status.configure(text="Нет CSV в results/ — сначала Запуск")
            return
        table_path = RESULTS / name
        stats_path = RESULTS / name.replace("_table.csv", "_stats.txt")
        if not table_path.exists():
            messagebox.showerror("Ошибка", f"Нет файла {table_path}")
            return

        headers, rows = load_csv(table_path)
        self._headers, self._rows = headers, rows
        self._fill_table(headers, rows)
        self._draw(headers, rows)

        self.log.delete("1.0", "end")
        if stats_path.exists():
            self.log_write(stats_path.read_text(encoding="utf-8"))
        self.log_write(f"Таблица: {table_path}  ({len(rows)} строк)")
        self.status.configure(text=f"Показано: {name}")

    def _fill_table(self, headers: list[str], rows: list[list[str]]) -> None:
        self.table.delete(*self.table.get_children())
        self.table["columns"] = headers
        for col in headers:
            self.table.heading(col, text=col)
            self.table.column(col, width=86, anchor="center", stretch=True)

        shown = rows
        note = ""
        limit = 8000
        if len(rows) > limit:
            shown = rows[:limit]
            note = f" (в таблице первые {limit} из {len(rows)})"
        for row in shown:
            values = row + [""] * (len(headers) - len(row))
            self.table.insert("", "end", values=values[: len(headers)])
        if note:
            self.status.configure(text=self.status.cget("text") + note)

    def _draw(self, headers: list[str], rows: list[list[str]]) -> None:
        idx = {name: i for i, name in enumerate(headers)}
        if "xi" not in idx:
            return

        def col(name: str) -> list[float]:
            j = idx[name]
            vals: list[float] = []
            for row in rows:
                if j >= len(row):
                    continue
                v = as_float(row[j])
                if v is not None:
                    vals.append(v)
            return vals

        xs = col("xi")
        self.ax.clear()
        self.ax.grid(True)
        self.ax.set_xlabel("x")

        if "vi" in idx:
            self.ax.plot(xs, col("vi"), color="tab:red", lw=1.6, label="v (RK4)")
        if "v2i" in idx:
            self.ax.plot(xs, col("v2i"), color="tab:blue", lw=1.0, ls="--", label="v2 (h/2)")
        if "ui" in idx:
            self.ax.plot(xs, col("ui"), color="tab:green", lw=1.0, ls=":", label="точное u")
        if "u'" in idx:
            self.ax.plot(xs, col("u'"), color="tab:orange", lw=1.0, label="u'")

        self.ax.legend(loc="best")
        self.figure.tight_layout()
        self.canvas.draw_idle()


def main() -> None:
    app = SolverGui()
    app.mainloop()


if __name__ == "__main__":
    if sys.version_info < (3, 9):
        sys.exit("Нужен Python 3.9+")
    main()
