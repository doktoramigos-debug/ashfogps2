@echo off
rem Нужен Docker Desktop. Собирает ashfog.elf в этой папке.
docker run --rm -v "%cd%":/src -w /src ps2dev/ps2dev:latest make clean all
if exist ashfog.elf (echo OK: ashfog.elf) else (echo СБОРКА НЕ УДАЛАСЬ)
pause
