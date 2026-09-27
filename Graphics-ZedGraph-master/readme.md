#Гайд 
```bash 
g++ -std=c++17 -O2 -Iinclude -o myClass src/myClass.cpp

./myClass <Nmax> <b> <a_g> <b_g> fixed/adaptive/both
```

где Nmax - ограничение по шагам
    b - правай граница
    a_g, b_g - коэффициенты в функции для задачи 2
    fixed/adaptive/both - как именно считаем (с фиксированным/адаптивным/и так и так) шагом

```bash 
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install matplotlib
```

 убедись, что `myClass` скомпилирован

  ```bash
   python3 solver_gui.py
   ```
