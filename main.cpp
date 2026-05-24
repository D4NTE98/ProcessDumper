#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>

struct ProcessInfo
{
    DWORD pid;
    std::wstring name;
    std::wstring path;
};

struct MemoryRegion
{
    uintptr_t base;
    SIZE_T size;
    DWORD protect;
    std::wstring protectStr;
};

std::wstring toLower(std::wstring s)
{
    for (auto& c : s) c = towlower(c);
    return s;
}

std::vector<ProcessInfo> findProcessesByName(const std::wstring& targetName)
{
    std::vector<ProcessInfo> results;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return results;

    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
    std::wstring lowered = toLower(targetName);

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (toLower(entry.szExeFile) == lowered)
            {
                ProcessInfo pi = { entry.th32ProcessID, entry.szExeFile, L"" };
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
                if (h)
                {
                    wchar_t buf[MAX_PATH] = {};
                    DWORD sz = MAX_PATH;
                    QueryFullProcessImageNameW(h, 0, buf, &sz);
                    CloseHandle(h);
                    pi.path = buf;
                }
                else
                {
                    pi.path = L"<access denied>";
                }
                results.push_back(pi);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return results;
}

HANDLE OpenProcessSafe(DWORD pid)
{
    return OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, pid);
}

std::vector<MemoryRegion> GetMemoryRegions(HANDLE hProcess)
{
    std::vector<MemoryRegion> regions;
    MEMORY_BASIC_INFORMATION mbi = {};
    uintptr_t addr = 0;

    while (VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi)))
    {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        {
            std::wstring prot;
            if (mbi.Protect & PAGE_EXECUTE_READWRITE) prot = L"ERW";
            else if (mbi.Protect & PAGE_READWRITE) prot = L"RW";
            else if (mbi.Protect & PAGE_EXECUTE_READ) prot = L"ER";
            else prot = L"R";

            regions.push_back({ (uintptr_t)mbi.BaseAddress, mbi.RegionSize, mbi.Protect, prot });
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }
    return regions;
}

void HexDump(HANDLE hProcess, uintptr_t address, SIZE_T size = 512)
{
    std::vector<BYTE> buffer(size);
    SIZE_T bytesRead;

    if (!ReadProcessMemory(hProcess, (LPCVOID)address, buffer.data(), size, &bytesRead))
    {
        std::wcout << L"Cannot read memory!\n";
        return;
    }

    std::wcout << L"\nHex Dump @ 0x" << std::hex << address << L"\n";
    for (SIZE_T i = 0; i < bytesRead; i += 16)
    {
        std::wcout << std::setw(8) << std::hex << (address + i) << L"  ";

        for (int j = 0; j < 16; ++j)
        {
            if (i + j < bytesRead)
                std::wcout << std::setw(2) << std::setfill(L'0') << (int)buffer[i + j] << L" ";
            else
                std::wcout << L"   ";
        }

        std::wcout << L" |";
        for (int j = 0; j < 16; ++j)
        {
            if (i + j < bytesRead)
            {
                BYTE c = buffer[i + j];
                std::wcout << (c >= 32 && c <= 126 ? (wchar_t)c : L'.');
            }
        }
        std::wcout << L"|\n";
    }
    std::wcout << std::dec;
}

template<typename T>
std::vector<uintptr_t> ScanForValue(HANDLE hProcess, T value, const std::vector<MemoryRegion>& regions)
{
    std::vector<uintptr_t> results;
    for (const auto& region : regions)
    {
        if (region.size > 50000000) continue;

        std::vector<BYTE> buffer(region.size);
        SIZE_T bytesRead;

        if (ReadProcessMemory(hProcess, (LPCVOID)region.base, buffer.data(), region.size, &bytesRead))
        {
            for (SIZE_T i = 0; i <= bytesRead - sizeof(T); i += 4)
            {
                if (*(T*)(buffer.data() + i) == value)
                {
                    results.push_back(region.base + i);
                }
            }
        }
    }
    return results;
}

std::vector<uintptr_t> ScanForString(HANDLE hProcess, const std::string& str, const std::vector<MemoryRegion>& regions, bool unicode)
{
    std::vector<uintptr_t> results;
    size_t len = unicode ? str.length() * 2 : str.length();

    for (const auto& region : regions)
    {
        std::vector<BYTE> buffer(region.size);
        SIZE_T bytesRead;

        if (ReadProcessMemory(hProcess, (LPCVOID)region.base, buffer.data(), region.size, &bytesRead))
        {
            for (SIZE_T i = 0; i < bytesRead - len; ++i)
            {
                bool found = true;
                for (size_t j = 0; j < str.length(); ++j)
                {
                    if (unicode)
                    {
                        if (buffer[i + j * 2] != (BYTE)str[j] || buffer[i + j * 2 + 1] != 0)
                        {
                            found = false;
                            break;
                        }
                    }
                    else
                    {
                        if (buffer[i + j] != (BYTE)str[j])
                        {
                            found = false;
                            break;
                        }
                    }
                }
                if (found)
                    results.push_back(region.base + i);
            }
        }
    }
    return results;
}

void CheatEngineMode(HANDLE hProcess, const ProcessInfo& proc)
{
    std::wcout << L"\n=== Cheat Engine Mode - " << proc.name << L" (PID: " << proc.pid << L") ===\n";

    auto regions = GetMemoryRegions(hProcess);

    while (true)
    {
        std::wcout << L"\n1. Search for int\n";
        std::wcout << L"2. Search for float\n";
        std::wcout << L"3. Search for string (ASCII)\n";
        std::wcout << L"4. Search for string (Unicode)\n";
        std::wcout << L"5. Hex Dump\n";
        std::wcout << L"6. List memory regions\n";
        std::wcout << L"0. Exit\n> ";

        int choice;
        std::cin >> choice;

        if (choice == 0) break;

        if (choice == 6)
        {
            std::wcout << L"\nFound memory regions: " << regions.size() << L"\n";
            for (const auto& r : regions)
            {
                std::wcout << L"0x" << std::hex << r.base << L" - 0x" << (r.base + r.size)
                          << L" [" << r.protectStr << L"] " << std::dec << r.size / 1024 << L" KB\n";
            }
            continue;
        }

        if (choice == 5)
        {
            uintptr_t addr;
            std::wcout << L"Enter address (hex): 0x";
            std::cin >> std::hex >> addr;
            HexDump(hProcess, addr);
            continue;
        }

        std::vector<uintptr_t> results;

        if (choice == 1)
        {
            int val;
            std::wcout << L"Int value: ";
            std::cin >> val;
            results = ScanForValue(hProcess, val, regions);
        }
        else if (choice == 2)
        {
            float val;
            std::wcout << L"Float value: ";
            std::cin >> val;
            results = ScanForValue(hProcess, val, regions);
        }
        else if (choice == 3 || choice == 4)
        {
            std::string str;
            std::wcout << L"String: ";
            std::cin.ignore();
            std::getline(std::cin, str);
            results = ScanForString(hProcess, str, regions, choice == 4);
        }

        std::wcout << L"\nFound " << results.size() << L" results:\n";
        for (size_t i = 0; i < std::min<size_t>(results.size(), 50); ++i)
        {
            std::wcout << i + 1 << L". 0x" << std::hex << results[i] << L"\n";
        }
        if (results.size() > 50)
            std::wcout << L"... and " << results.size() - 50 << L" more\n";
    }
}

int wmain()
{
    SetConsoleOutputCP(CP_UTF8);
    std::wcout << L"=== Process Inspector + Cheat Engine Mode ===\n\n";

    std::wstring name;
    std::wcout << L"Enter process name (e.g. game.exe): ";
    std::getline(std::wcin, name);

    auto processes = findProcessesByName(name);

    if (processes.empty())
    {
        std::wcout << L"Process not found!\n";
        return 1;
    }

    ProcessInfo selected = processes[0];
    if (processes.size() > 1)
    {
        std::wcout << L"Found " << processes.size() << L" processes:\n";
        for (size_t i = 0; i < processes.size(); ++i)
        {
            std::wcout << i + 1 << L". " << processes[i].name << L" (PID: " << processes[i].pid << L")\n";
        }
        std::wcout << L"Choose: ";
        int idx;
        std::cin >> idx;
        if (idx > 0 && idx <= (int)processes.size())
            selected = processes[idx - 1];
    }

    HANDLE hProcess = OpenProcessSafe(selected.pid);
    if (!hProcess)
    {
        std::wcout << L"Cannot open process!\n";
        return 1;
    }

    CheatEngineMode(hProcess, selected);

    CloseHandle(hProcess);
    std::wcout << L"\nFinished.\n";
    return 0;
}
