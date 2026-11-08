@echo off
chcp 65001 >nul

:: Проверка на наличие прав Администратора
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Запрашиваю права Администратора для доступа к датчикам процессора...
    powershell -Command "Start-Process -FilePath '%0' -Verb RunAs"
    exit /b
)

:: Если права получены, переходим в папку со скриптом и запускаем Python
cd /d "%~dp0"
python thefinale.py