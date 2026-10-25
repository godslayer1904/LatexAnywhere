@echo off
rem 用 PATH 里的 Python 启动原型（没有黑窗口）
cd /d "%~dp0"
start "" pythonw -u formula_box.py
