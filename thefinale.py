import serial
import serial.tools.list_ports
import psutil
import time
import openpyxl
from openpyxl.chart import LineChart, Reference
from datetime import datetime
import os
import sys

# Подключение DLL библиотеки LibreHardwareMonitor
dll_path = os.path.abspath("dlls/LibreHardwareMonitorLib.dll")
if not os.path.exists(dll_path):
    print(f"ОШИБКА: Файл драйвера не найден!")
    print(f"Поместите 'LibreHardwareMonitorLib.dll' и все остальные .dll файлы в папку: {os.path.dirname(dll_path)}")
    input("Нажмите Enter для выхода...")
    sys.exit()

import clr

clr.AddReference(dll_path)
from LibreHardwareMonitor import Hardware

# Инициализация железа через драйвер
computer = Hardware.Computer()
computer.IsCpuEnabled = True
computer.IsGpuEnabled = True
computer.Open()


def get_hardware_data():
    cpu_temp = 0
    cpu_power = 0
    gpu_stats = {'usage': 0, 'temp': 0, 'power': 0, 'mem_percent': 0}

    for hw in computer.Hardware:
        hw.Update()
        hw_type = str(hw.HardwareType).lower()

        # Парсинг процессора (УНИВЕРСАЛЬНЫЙ ДЛЯ INTEL И AMD)
        if "cpu" in hw_type:
            for sensor in hw.Sensors:
                if sensor.Value is None: continue
                s_type = str(sensor.SensorType).lower()
                s_name = str(sensor.Name).lower()

                if s_type == "temperature":
                    if "package" in s_name or "tctl" in s_name or "tdie" in s_name:
                        cpu_temp = int(sensor.Value)

                    elif "core" in s_name and cpu_temp == 0:
                        cpu_temp = int(sensor.Value)

                elif s_type == "power" and "package" in s_name:
                    cpu_power = int(sensor.Value)

            if cpu_temp == 0:
                for sensor in hw.Sensors:
                    if sensor.Value is not None and str(sensor.SensorType).lower() == "temperature":
                        cpu_temp = int(sensor.Value)
                        break

        # Парсинг видеокарты
        elif "gpu" in hw_type:
            for sensor in hw.Sensors:
                if sensor.Value is None: continue
                s_type = str(sensor.SensorType).lower()
                s_name = str(sensor.Name).lower()

                if s_type == "temperature" and "core" in s_name:
                    gpu_stats['temp'] = int(sensor.Value)
                elif s_type == "load":
                    if "core" in s_name:
                        gpu_stats['usage'] = int(sensor.Value)
                    elif "memory" in s_name:
                        gpu_stats['mem_percent'] = int(sensor.Value)
                elif s_type == "power":
                    if gpu_stats['power'] == 0: gpu_stats['power'] = int(sensor.Value)

    return cpu_temp, cpu_power, gpu_stats


def find_arduino():
    """Автоматический поиск COM-порта с Arduino"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "CH340" in port.description or "Arduino" in port.description or "Serial" in port.description:
            return port.device
    return ports[0].device if ports else None


# Инициализация подключения
PORT = find_arduino()
arduino = None

if not PORT:
    print("Arduino не найдена. Проверьте подключение USB!")
    input("Нажмите Enter для выхода...")
    sys.exit()

while True:
    try:
        arduino = serial.Serial(PORT, 9600, timeout=0.1)
        time.sleep(2)
        print(f"Успешно подключено к {PORT} (Автоопределение)!")
        break
    except Exception as e:
        print(f"Ожидание порта {PORT}... ({e})")
        time.sleep(3)

print("Мониторинг запущен! Нажмите SELECT для лога, RIGHT для экрана Максимумов.")

# Переменные логирования
wb = None
ws = None
is_logging = False
last_button_time = 0
filename = ""
row_idx = 2

try:
    while True:
        current_time_ms = time.time()

        if arduino.in_waiting > 0:
            try:
                cmd = arduino.readline().decode().strip()
                if (current_time_ms - last_button_time) > 1.5:

                    if cmd == "START_LOG" and not is_logging:
                        is_logging = True
                        last_button_time = current_time_ms
                        filename = datetime.now().strftime("telemetry_%Y%m%d_%H%M%S.xlsx")
                        row_idx = 2

                        wb = openpyxl.Workbook()
                        ws = wb.active
                        ws.title = "Телеметрия нагрузки"
                        ws.append(['Время', 'Загрузка CPU (%)', 'Темп CPU (°C)', 'Мощность CPU (Вт)',
                                   'Загрузка GPU (%)', 'Темп GPU (°C)', 'Мощность GPU (Вт)',
                                   'Загрузка RAM (%)', 'Занято RAM (Гб)', 'Всего RAM (Гб)', 'Загрузка VRAM (%)'])
                        print(f"\n[НПК] >>> НАЧАТА ЗАПИСЬ В EXCEL: {filename} <<<")

                    elif cmd == "STOP_LOG" and is_logging:
                        is_logging = False
                        last_button_time = current_time_ms

                        if wb and ws:
                            chart = LineChart()
                            chart.title = "Динамика температур во время теста"
                            chart.y_axis.title = "Температура (°C)"
                            chart.y_axis.scaling.orientation = "minMax"
                            chart.y_axis.delete = False
                            chart.y_axis.majorGridlines = openpyxl.chart.axis.ChartLines()

                            chart.x_axis.title = "Время"
                            chart.x_axis.textRotation = -45
                            chart.x_axis.delete = False
                            chart.x_axis.tickLblSkip = 30
                            chart.style = None
                            chart.width = 22
                            chart.height = 13

                            data_cpu = Reference(ws, min_col=3, min_row=1, max_row=row_idx - 1)
                            data_gpu = Reference(ws, min_col=6, min_row=1, max_row=row_idx - 1)
                            chart.add_data(data_cpu, titles_from_data=True)
                            chart.add_data(data_gpu, titles_from_data=True)

                            cats = Reference(ws, min_col=1, min_row=2, max_row=row_idx - 1)
                            chart.set_categories(cats)

                            for series in chart.series:
                                series.smooth = True

                            ws.add_chart(chart, "M2")
                            wb.save(filename)
                            wb = None
                            ws = None
                        print(f"\n[НПК] >>> ФАЙЛ СОХРАНЁН. ГРАФИК ПОСТРОЕН! <<<")
            except Exception as e:
                print(f"Ошибка команды: {e}")

        # Сбор данных напрямую через DLL и psutil
        cpu_temp, cpu_power, gpu_stats = get_hardware_data()
        cpu = int(psutil.cpu_percent())

        # Оптимизация опроса памяти
        mem_info = psutil.virtual_memory()
        mem_percent = int(mem_info.percent)
        mem_used = int(mem_info.used / (1024 ** 3))
        mem_total = int(mem_info.total / (1024 ** 3))

        # Запись в Excel
        if is_logging and ws:
            current_time = datetime.now().strftime("%H:%M:%S")
            try:
                ws.append([
                    current_time, cpu, cpu_temp, cpu_power, gpu_stats['usage'],
                    gpu_stats['temp'], gpu_stats['power'], mem_percent,
                    mem_used, mem_total, gpu_stats['mem_percent']
                ])
                row_idx += 1
                print("●", end="", flush=True)
            except Exception as e:
                print(f"\nОшибка записи: {e}")

        # Отправка пакета на Ардуино
        data_packet = f"<CPU:{cpu},{cpu_temp},{cpu_power},GPU:{gpu_stats['usage']},{gpu_stats['temp']},{gpu_stats['power']},RAM:{mem_percent},{mem_used},{mem_total},GPUMEM:{gpu_stats['mem_percent']}\n"

        try:
            print(f"Отправка: {data_packet.strip()}")  # Отладка: видим, что уходит в провод
            arduino.write(data_packet.encode('ascii'))
            arduino.flush()  # Принудительно выталкиваем данные
        except Exception as e:
            print(f"Ошибка передачи: {e}")

        time.sleep(2)

except KeyboardInterrupt:
    print("\nМониторинг остановлен.")
finally:
    if wb: wb.save(filename)
    if arduino and arduino.is_open: arduino.close()
    computer.Close()
