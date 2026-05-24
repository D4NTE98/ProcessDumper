# ProcessDumper

A powerful Windows process memory inspector and dumper inspired by Cheat Engine. Allows you to scan, view, and dump memory of running processes.

![Version](https://img.shields.io/badge/Version-1.0-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B-green)

## Description

**ProcessDumper** is a lightweight, console-based tool that lets you inspect and analyze the memory of any running Windows process. It combines process information gathering with advanced memory scanning capabilities similar to Cheat Engine.

**Warning**: This tool requires administrator privileges and may be detected by antivirus software as a potential hacking/tool utility.

## Features

- Search for processes by name
- Display detailed process information (PID, path, memory usage, threads)
- Memory region analysis
- Value scanning (int, float)
- String scanning (ASCII and Unicode)
- Hex memory dump with ASCII view
- Clean and user-friendly console interface

## Usage

1. Run `ProcessDumper.exe` as **Administrator**
2. Enter the name of the target process (e.g. `game.exe`)
3. Select the process if multiple matches are found
4. Use the menu to scan memory or view hex dumps

### Menu Options

- `1` → Search for int value
- `2` → Search for float value
- `3` → Search for ASCII string
- `4` → Search for Unicode string
- `5` → Hex Dump at specific address
- `6` → List all memory regions
- `0` → Exit

## Build Instructions

### Requirements
- Windows 10/11
- Visual Studio 2022 (recommended) or MinGW

### Compiling with Visual Studio
1. Create a new Console App project
2. Replace the code with the content of `main.cpp`
3. Set **Character Set** to **Use Unicode Character Set**
4. Build in **Release x64**

### Compiling with g++ (MinGW)
```bash
g++ -o ProcessDumper.exe main.cpp -static -lpsapi -lntdll -O2
