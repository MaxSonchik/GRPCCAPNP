import numpy as np
import matplotlib.pyplot as plt

# Аналитическое решение (оно корректно для данного ДУ)
def exact_solution(x_val):
    # Оставил одну из ваших строк с переносом, чтобы сохранить ближе к оригиналу,
    # но обычно лучше разбивать на отдельные переменные или использовать скобки для переноса.
    term1 = np.exp(-5 * x_val) * (10/13 * np.cos(5 * np.sqrt(3) * x_val) +
                                (8 / (13 * np.sqrt(3))) * np.sin(5 * np.sqrt(3) * x_val))
    term2 = (5/13) * np.sin(2 * x_val) - (10/13) * np.cos(2 * x_val)
    return term1 + term2

# Система уравнений для численных методов
def f_ode_y_prime(x, y, z): # Это y' = z
    return z

def f_ode_z_prime(x, y, z): # Это z' = (10 * sin(2x) - 2z - 20y) / 0.2
    return (10 * np.sin(2 * x) - 2 * z - 20 * y) / 0.2

# Метод Эйлера
def euler_method(y_prime_func, z_prime_func, x0_val, y0_val, z0_val, h_step, steps_val):
    x_arr = np.zeros(steps_val + 1)
    y_arr = np.zeros(steps_val + 1)
    z_arr = np.zeros(steps_val + 1)
    x_arr[0], y_arr[0], z_arr[0] = x0_val, y0_val, z0_val

    for i in range(steps_val):
        # y_prime_func(x_arr[i], y_arr[i], z_arr[i]) это просто z_arr[i]
        y_arr[i + 1] = y_arr[i] + h_step * z_arr[i] # Более явно используем z_arr[i]
        z_arr[i + 1] = z_arr[i] + h_step * z_prime_func(x_arr[i], y_arr[i], z_arr[i])
        x_arr[i + 1] = x_arr[i] + h_step
    return x_arr, y_arr

# Метод Рунге-Кутты 4-го порядка
def runge_kutta_4(y_prime_func, z_prime_func, x0_val, y0_val, z0_val, h_step, steps_val):
    x_arr = np.zeros(steps_val + 1)
    y_arr = np.zeros(steps_val + 1)
    z_arr = np.zeros(steps_val + 1)
    x_arr[0], y_arr[0], z_arr[0] = x0_val, y0_val, z0_val

    for i in range(steps_val):
        # k_y - приращения для y, k_z - приращения для z

        # y_prime_func(x_arr[i], y_arr[i], z_arr[i]) это просто z_arr[i]
        k1_y = h_step * z_arr[i]
        k1_z = h_step * z_prime_func(x_arr[i], y_arr[i], z_arr[i])

        # y_prime_func в средней точке использует z_arr[i] + k1_z / 2
        k2_y = h_step * (z_arr[i] + k1_z / 2)
        k2_z = h_step * z_prime_func(x_arr[i] + h_step / 2, y_arr[i] + k1_y / 2, z_arr[i] + k1_z / 2)

        # Исправлена синтаксическая ошибка 7k3 -> k3_y
        # y_prime_func в средней точке использует z_arr[i] + k2_z / 2
        k3_y = h_step * (z_arr[i] + k2_z / 2)
        k3_z = h_step * z_prime_func(x_arr[i] + h_step / 2, y_arr[i] + k2_y / 2, z_arr[i] + k2_z / 2)

        # y_prime_func в конечной точке использует z_arr[i] + k3_z
        k4_y = h_step * (z_arr[i] + k3_z)
        k4_z = h_step * z_prime_func(x_arr[i] + h_step, y_arr[i] + k3_y, z_arr[i] + k3_z)

        y_arr[i + 1] = y_arr[i] + (k1_y + 2 * k2_y + 2 * k3_y + k4_y) / 6
        z_arr[i + 1] = z_arr[i] + (k1_z + 2 * k2_z + 2 * k3_z + k4_z) / 6
        x_arr[i + 1] = x_arr[i] + h_step
    return x_arr, y_arr

# Начальные условия
x0 = 0.0
y0 = 0.0 # y(0) = 0
z0 = 0.0 # y'(0) = 0
T_final = 1.0

# Параметры для разных шагов
h_values = [0.2, 0.1, 0.05] # Расположим от большего к меньшему для соответствия маркерам
# Единая цветовая схема и маркеры для шагов h
# h=0.2 (крупный шаг) -> red, 'D'
# h=0.1 (средний шаг) -> green, 's'
# h=0.05 (мелкий шаг) -> blue, 'o'
plot_params = {
    0.2: {'color': 'red', 'marker': 'D'},
    0.1: {'color': 'green', 'marker': 's'},
    0.05: {'color': 'blue', 'marker': 'o'}
}


# --- График 1: Сравнение всех решений ---
plt.figure(figsize=(12, 8)) # Уменьшил размер для стандартного отображения
plt.axhline(0, color='gray', lw=0.5) # Ось x=0

# Точное решение
x_exact_plot = np.linspace(x0, T_final, 200) # Больше точек для гладкости
y_exact_plot = exact_solution(x_exact_plot)
plt.plot(x_exact_plot, y_exact_plot, 'k-', linewidth=2, label='Аналитическое', zorder=10)

# Решения Эйлера
for h in h_values:
    params = plot_params[h]
    steps = int(round(T_final / h)) # Более точный расчет шагов
    if steps == 0: continue
    x_euler, y_euler = euler_method(f_ode_y_prime, f_ode_z_prime, x0, y0, z0, h, steps)
    plt.plot(x_euler, y_euler, color=params['color'], marker=params['marker'], linestyle='--',
             markersize=5, label=f'Эйлер (h={h})', zorder=5)

# Решения Рунге-Кутты
for h in h_values:
    params = plot_params[h]
    steps = int(round(T_final / h))
    if steps == 0: continue
    x_rk, y_rk = runge_kutta_4(f_ode_y_prime, f_ode_z_prime, x0, y0, z0, h, steps)
    plt.plot(x_rk, y_rk, color=params['color'], marker=params['marker'], linestyle=':',
             markersize=5, fillstyle='none', markeredgewidth=1.5, label=f'РК4 (h={h})', zorder=7)


plt.title('Сравнение всех решений', fontsize=14)
plt.xlabel('x', fontsize=12)
plt.ylabel('y(x)', fontsize=12)
plt.xticks(np.arange(0, T_final + 0.1, 0.1))
plt.ylim(-0.5, 1.0)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend(fontsize=10, framealpha=1, loc='lower right')
plt.tight_layout()
plt.show()

# --- График 2: Погрешности методов ---
plt.figure(figsize=(12, 8))

# График погрешности метода Эйлера
for h in h_values:
    params = plot_params[h]
    steps = int(round(T_final / h))
    if steps == 0: continue
    x_euler, y_euler = euler_method(f_ode_y_prime, f_ode_z_prime, x0, y0, z0, h, steps)
    y_exact_at_euler_points = exact_solution(x_euler)
    absolute_error_euler = np.abs(y_exact_at_euler_points - y_euler)

    # Для логарифмической шкалы, если первая точка имеет нулевую ошибку, можем ее пропустить
    plot_x, plot_err = x_euler, absolute_error_euler
    if absolute_error_euler[0] < 1e-18 and len(x_euler) > 1:
         plot_x, plot_err = x_euler[1:], absolute_error_euler[1:]
    if len(plot_x) > 0:
        plt.plot(plot_x, plot_err, color=params['color'], marker=params['marker'], linestyle='--',
                 markersize=5, label=f'Погр. Эйлера (h={h})')

# График погрешности метода Рунге-Кутты
for h in h_values:
    params = plot_params[h]
    steps = int(round(T_final / h))
    if steps == 0: continue
    x_rk, y_rk = runge_kutta_4(f_ode_y_prime, f_ode_z_prime, x0, y0, z0, h, steps)
    y_exact_at_rk_points = exact_solution(x_rk)
    absolute_error_rk = np.abs(y_exact_at_rk_points - y_rk)

    plot_x, plot_err = x_rk, absolute_error_rk
    if absolute_error_rk[0] < 1e-18 and len(x_rk) > 1:
         plot_x, plot_err = x_rk[1:], absolute_error_rk[1:]
    if len(plot_x) > 0:
        plt.plot(plot_x, plot_err, color=params['color'], marker=params['marker'], linestyle=':',
                 markersize=5, fillstyle='none', markeredgewidth=1.5, label=f'Погр. РК4 (h={h})')


plt.title('Абсолютная погрешность методов', fontsize=14)
plt.xlabel('x', fontsize=12)
plt.ylabel('Абсолютная погрешность', fontsize=12)
plt.yscale('log') # Логарифмический масштаб для погрешностей очень важен
plt.legend(fontsize=10, loc='best')
plt.grid(True, which="both", linestyle='--', alpha=0.7) # 'which="both"' для лог. шкалы
plt.tight_layout()
plt.show()

# --- Дополнительный анализ: интегральная погрешность (RMSE) ---
print("\nСреднеквадратичная ошибка (RMSE):")
for h in h_values:
    steps = int(round(T_final / h))
    if steps == 0: continue

    # Для метода Эйлера
    x_euler, y_euler = euler_method(f_ode_y_prime, f_ode_z_prime, x0, y0, z0, h, steps)
    y_exact_euler = exact_solution(x_euler)
    error_euler = np.sqrt(np.mean((y_exact_euler - y_euler)**2)) # RMSE

    # Для метода Рунге-Кутты
    x_rk, y_rk = runge_kutta_4(f_ode_y_prime, f_ode_z_prime, x0, y0, z0, h, steps)
    y_exact_rk = exact_solution(x_rk)
    error_rk = np.sqrt(np.mean((y_exact_rk - y_rk)**2)) # RMSE

    print(f"h = {h:.2f}:") # Используем .2f для h, как в вашем выводе
    print(f"  Метод Эйлера: {error_euler:.6f}")
    print(f"  Рунге-Кутта: {error_rk:.6f}")
