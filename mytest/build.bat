@echo off

echo %TIME%

set path=D:\TDM-GCC-32\bin

gcc -std=c99 -I"../" -Wall -Wextra -o grep.exe ..\re.c grep.c

echo %TIME%

echo.
echo.
pause