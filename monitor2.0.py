import serial
import psutil
import time
import wmi
import gpustat


def get_cpu_temp():
    try:
        w = wmi.WMI(namespace="root\\OpenHardwareMonitor")
        for sensor in w.Sensor():
            if sensor.SensorType == 'Temperature' and 'CPU' in sensor.Name:
                temp = int(float(sensor.Value))
                print(f"🌡️ CPU Temp: {temp}C")
                return temp
    except Exception as e:
        print(f"❌ CPU temp error: {e}")
    return 0


def get_cpu_power():
    try:
        w = wmi.WMI(namespace="root\\OpenHardwareMonitor")
        total_power = 0
        for sensor in w.Sensor():
            if sensor.SensorType == 'Power' and any(keyword in sensor.Name for keyword in ['CPU', 'Package', 'Core']):
                power_value = float(sensor.Value)
                if power_value > 1:
                    total_power += power_value
        power = int(total_power) if total_power > 0 else 0
        print(f"⚡ CPU Power: {power}W")
        return power
    except Exception as e:
        print(f"❌ CPU power error: {e}")
    return 0


def get_gpu_stats():
    try:
        stats = gpustat.GPUStatCollection.new_query()
        if stats.gpus:
            gpu = stats.gpus[0]
            print(f"🎮 GPU: {gpu.utilization}% {gpu.temperature}C")
            return {
                'usage': gpu.utilization,
                'temp': gpu.temperature,
                'power': gpu.power_draw if hasattr(gpu, 'power_draw') and gpu.power_draw else 0,
                'memory_used': gpu.memory_used,
                'memory_total': gpu.memory_total
            }
    except Exception as e:
        print(f"❌ GPU error: {e}")
    return None


# Подключение
try:
    arduino = serial.Serial('COM7', 9600, timeout=1)
    time.sleep(2)
    print("✅ Connected to Arduino COM7")
except Exception as e:
    print(f"❌ Arduino connection failed: {e}")
    exit()

print("🚀 Starting monitoring...")

try:
    while True:
        # Получаем данные
        cpu = int(psutil.cpu_percent())
        mem = int(psutil.virtual_memory().percent)
        cpu_temp = get_cpu_temp()
        cpu_power = get_cpu_power()

        gpu = get_gpu_stats()
        if gpu and gpu['memory_total'] > 0:
            gpu_mem_percent = int((gpu['memory_used'] / gpu['memory_total']) * 100)
        else:
            gpu_mem_percent = 0

        # ПРОСТОЙ ФОРМАТ ДАННЫХ
        data = f"CPU:{cpu},{cpu_temp},{cpu_power},GPU:{gpu['usage'] if gpu else 0},{gpu['temp'] if gpu else 0},{gpu['power'] if gpu else 0},RAM:{mem},{psutil.virtual_memory().used / 1024 ** 3:.1f},{psutil.virtual_memory().total / 1024 ** 3:.1f},GPUMEM:{gpu_mem_percent}"

        print(f"📨 Sending: {data}")

        # Отправляем
        arduino.write((data + '\n').encode())

        time.sleep(2)

except KeyboardInterrupt:
    print("\n🛑 Stopping...")
    arduino.close()