@echo off
rem Необязательно: проверка геометрии на ПК (нужен gcc/MinGW). Результат - frame.ppm
gcc -O2 -o test_render.exe src\test_render.c src\render.c src\game.c -lm && test_render.exe hospital 4.8 -7.2 0 frame.ppm
