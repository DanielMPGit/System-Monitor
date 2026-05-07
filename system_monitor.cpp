#include <windows.h>
#include <iostream>
#include <iomanip>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/table.hpp>
using namespace ftxui;

MEMORYSTATUSEX getMemoryStatus() {
    MEMORYSTATUSEX memory;
    memory.dwLength = sizeof(memory);
    GlobalMemoryStatusEx(&memory);
    return memory;
}

double getDouble(FILETIME time){
    return (double)((unsigned long long)time.dwHighDateTime << 32 | time.dwLowDateTime);
}
std::tuple<ULARGE_INTEGER, ULARGE_INTEGER, ULARGE_INTEGER> getDiskInfo(LPCSTR path){
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalBytes, &totalFreeBytes);
    return std::make_tuple(freeBytesAvailable, totalBytes, totalFreeBytes);
}

std::tuple<FILETIME,FILETIME,FILETIME,FILETIME,FILETIME,FILETIME> getFileTimes(){
    static FILETIME prevIdleTime = {}, prevKernelTime = {}, prevUserTime = {};
    FILETIME idleTime, kernelTime, userTime;
    GetSystemTimes(&idleTime, &kernelTime, &userTime);
    auto result = std::make_tuple(idleTime, kernelTime, userTime, prevIdleTime, prevKernelTime, prevUserTime);
    prevIdleTime = idleTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;
    return result;
}

std::string getCPULoad(FILETIME idleTime, FILETIME kernelTime, FILETIME userTime, FILETIME prevIdleTime, FILETIME prevKernelTime, FILETIME prevUserTime){
    double idle   = getDouble(idleTime)   - getDouble(prevIdleTime);
    double kernel = getDouble(kernelTime) - getDouble(prevKernelTime);
    double user   = getDouble(userTime)   - getDouble(prevUserTime);
    double total = kernel + user;
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << ((total - idle) / total)*100;
    return stream.str();
}

std::string getSystemTime(FILETIME kernelTime, FILETIME userTime, FILETIME prevKernelTime, FILETIME prevUserTime){
    double kernel = getDouble(kernelTime) - getDouble(prevKernelTime);
    double user   = getDouble(userTime)   - getDouble(prevUserTime);
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << (kernel + user)/10000000.0;
    return stream.str();
}

std::string getMemoryGB(double data){
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << data/(1024.0 * 1024 * 1024);
    return stream.str();
}

std::string getMemoryMB(double data){
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << data/(1024.0 * 1024);
    return stream.str();
}

std::string getMemoryKB(double data){
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << (data / 1024.0);
    return stream.str();
}
Element listDrives(){
    DWORD drives = GetLogicalDrives();
    Elements drive_element;
    
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            std::string letra = std::string(1, (char)('A' + i)) + ":\\";
            std::wstring wletra(letra.begin(), letra.end());
            auto [free, total, totalFree] = getDiskInfo(letra.c_str());
            drive_element.push_back(hbox({ 
                text("Disk: " + letra) | size(WIDTH, EQUAL, 12), separator(),
                text("Free for User: " + getMemoryGB(free.QuadPart)+" GB") | size(WIDTH, EQUAL, 25), separator(),
                text("Total: " + getMemoryGB(total.QuadPart)+" GB") | size(WIDTH, EQUAL, 20), separator(),
                text("Free Total: " + getMemoryGB(totalFree.QuadPart)+" GB") | size(WIDTH, EQUAL, 20),
            }));
        }
    }
    
    return vbox(drive_element);
}

int main() {
    getFileTimes();
    Sleep(1000);
    auto [idleTime, kernelTime, userTime, prevIdleTime, prevKernelTime, prevUserTime] = getFileTimes();
    Element doc = vbox({
        vbox({
            text("Memory") | bold,
            separator(),
            hbox({
                text("Total Memory: "+ getMemoryGB(getMemoryStatus().ullTotalPhys)+" GB ")
            }),
            hbox({
                text("Free Memory GB: "+ getMemoryGB(getMemoryStatus().ullAvailPhys)+" GB")
            }),
            hbox({
                text("Used Memory %: "+ std::to_string(getMemoryStatus().dwMemoryLoad) +"%")
            }),
        }) ,
        separator(),
        vbox({
            text("CPU") | bold,
            separator(),
            hbox({
                text("CPU Usage: "+ getCPULoad(idleTime, kernelTime, userTime, prevIdleTime, prevKernelTime, prevUserTime)+" % ")
            }),
            hbox({
                text("CPU Time: "+ getSystemTime(kernelTime, userTime, prevKernelTime, prevUserTime)+" s ")
            }),
        }) ,
        separator(),
        vbox({
            text("Disk Information") | bold,
            separator(),
            listDrives(),
        }) ,
    })| border;

    auto screen = Screen::Create(
        Dimension::Full(),
        Dimension::Fit(doc)
    );

    Render(screen, doc);
    screen.Print();
    return 0;
}