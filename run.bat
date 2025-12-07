
@echo off
echo Compiling ExpenseSplitter_SQL.cpp ...
g++ ExpenseSplitter_SQL.cpp -lsqlite3 -o ExpenseSplitter.exe
if %errorlevel% neq 0 (
    echo.
    echo Compilation failed. Make sure:
    echo - g++ is installed and in PATH
    echo - SQLite dev library (sqlite3) is installed and linkable with -lsqlite3
    pause
    exit /b 1
)
echo.
echo Running ExpenseSplitter.exe ...
ExpenseSplitter.exe
pause
