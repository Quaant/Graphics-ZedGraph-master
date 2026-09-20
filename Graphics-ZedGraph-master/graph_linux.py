#!/usr/bin/env python3
"""Linux-порт MyForm (C++/CLI + WinForms + ZedGraph)."""

import math
import tkinter as tk
from tkinter import ttk, messagebox

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure


def parse_double(text):
    """Convert::ToDouble с русской запятой, как в исходном коде."""
    return float(str(text).strip().replace(",", ".").replace(" ", ""))


class MyForm(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("MyForm")
        self.geometry("858x497")
        self.minsize(858, 497)
        self.resizable(False, False)

        # --- график (zedGraphControl1) ---
        self.figure = Figure(figsize=(5.01, 3.27), dpi=100)
        self.ax = self.figure.add_subplot(111)
        self.ax.grid(True)
        self.canvas = FigureCanvasTkAgg(self.figure, master=self)
        self.canvas.get_tk_widget().place(x=38, y=30, width=501, height=327)

        # --- таблица (dataGridView1): X, F_1, F_2 ---
        table_frame = tk.Frame(self)
        table_frame.place(x=559, y=30, width=274, height=327)

        columns = ("X", "F_1", "F_2")
        self.data_grid = ttk.Treeview(
            table_frame, columns=columns, show="headings", height=16
        )
        self.data_grid.heading("X", text="X")
        self.data_grid.heading("F_1", text="F_1")
        self.data_grid.heading("F_2", text="F_2")
        self.data_grid.column("X", width=50, anchor="center")
        self.data_grid.column("F_1", width=100, anchor="center")
        self.data_grid.column("F_2", width=100, anchor="center")

        scrollbar = ttk.Scrollbar(
            table_frame, orient="vertical", command=self.data_grid.yview
        )
        self.data_grid.configure(yscrollcommand=scrollbar.set)
        self.data_grid.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # --- a, b, h (Draw) ---
        tk.Label(self, text="a").place(x=59, y=394)
        self.text_box1 = tk.Entry(self, width=6)
        self.text_box1.insert(0, "0")
        self.text_box1.place(x=78, y=394, width=48, height=20)

        tk.Label(self, text="b").place(x=171, y=396)
        self.text_box2 = tk.Entry(self, width=6)
        self.text_box2.insert(0, "1")
        self.text_box2.place(x=190, y=393, width=49, height=20)

        tk.Label(self, text="h").place(x=287, y=398)
        self.text_box3 = tk.Entry(self, width=8)
        self.text_box3.insert(0, "0,1")
        self.text_box3.place(x=306, y=394, width=61, height=20)

        # --- a, b (Zoom) ---
        tk.Label(self, text="a").place(x=59, y=438)
        self.text_box5 = tk.Entry(self, width=6)
        self.text_box5.insert(0, "0")
        self.text_box5.place(x=78, y=436, width=48, height=20)

        tk.Label(self, text="b").place(x=171, y=440)
        self.text_box4 = tk.Entry(self, width=6)
        self.text_box4.insert(0, "1")
        self.text_box4.place(x=190, y=437, width=49, height=20)

        tk.Button(self, text="Draw", command=self.button1_click).place(
            x=633, y=386, width=142, height=29
        )
        tk.Button(self, text="Zoom", command=self.button2_click).place(
            x=633, y=437, width=142, height=29
        )

    def f1(self, x):
        return math.sin(x)

    def f2(self, x):
        return math.sin(2 * x)

    def button1_click(self):
        try:
            xmin = parse_double(self.text_box1.get())
            xmax = parse_double(self.text_box2.get())
            h = parse_double(self.text_box3.get())
        except ValueError:
            messagebox.showerror("Ошибка", "a, b и h должны быть числами")
            return

        if h == 0:
            messagebox.showerror("Ошибка", "шаг h не должен быть равен 0")
            return

        xmin_limit = xmin - 0.1
        xmax_limit = xmax + 0.1

        xs, y1, y2 = [], [], []

        for item in self.data_grid.get_children():
            self.data_grid.delete(item)

        i = 0
        x = xmin
        # как в исходнике: for (double x = xmin; x <= xmax; x += h)
        while x <= xmax + 1e-12:
            v1 = self.f1(x)
            v2 = self.f2(x)
            xs.append(x)
            y1.append(v1)
            y2.append(v2)
            self.data_grid.insert(
                "",
                "end",
                values=(
                    x,
                    math.floor(v1 * 1000) / 1000,
                    math.floor(v2 * 1000) / 1000,
                ),
            )
            i += 1
            x = xmin + i * h

        self.ax.clear()
        self.ax.grid(True)
        self.ax.plot(xs, y1, color="red", marker="+", linestyle="-", label="F1(x)")
        self.ax.plot(xs, y2, color="blue", linestyle="-", label="F2(x)")
        self.ax.legend()
        self.ax.set_xlim(xmin_limit, xmax_limit)
        self.canvas.draw()

    def button2_click(self):
        try:
            xmin = parse_double(self.text_box5.get())
            xmax = parse_double(self.text_box4.get())
        except ValueError:
            messagebox.showerror("Ошибка", "a и b должны быть числами")
            return

        self.ax.set_xlim(xmin, xmax)
        self.canvas.draw()


if __name__ == "__main__":
    app = MyForm()
    app.mainloop()
