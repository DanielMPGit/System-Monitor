#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/table.hpp>
#include <fstream>
#include <filesystem>
#include <sys/statvfs.h>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <deque>
#include <mutex>
#include <algorithm>
using namespace ftxui;

std::string readLine(const std::string& path, const std::string& key) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(key) == 0) {
            std::istringstream ss(line);
            std::string k, v;
            ss >> k >> v;
            return v;
        }
    }
    return "0";
}

struct MemSample {
    double mem_pct, swp_pct;
    double mem_used_gb, mem_total_gb;
    double swp_used_gb, swp_total_gb;
};

MemSample readMemory() {
    double mem_total = 0, mem_avail = 0, swp_total = 0, swp_free = 0;
    std::ifstream file("/proc/meminfo");
    std::string line;
    while (std::getline(file, line)) {
        auto parse = [&](const std::string& key, double& dest) {
            if (line.find(key) == 0) {
                std::istringstream ss(line);
                std::string k; ss >> k >> dest;
            }
        };
        parse("MemTotal:",     mem_total);
        parse("MemAvailable:", mem_avail);
        parse("SwapTotal:",    swp_total);
        parse("SwapFree:",     swp_free);
    }
    MemSample s;
    s.mem_total_gb = mem_total / 1048576.0;
    s.mem_used_gb  = (mem_total - mem_avail) / 1048576.0;
    s.mem_pct      = mem_total > 0 ? 100.0 * (mem_total - mem_avail) / mem_total : 0;
    s.swp_total_gb = swp_total / 1048576.0;
    s.swp_used_gb  = (swp_total - swp_free) / 1048576.0;
    s.swp_pct      = swp_total > 0 ? 100.0 * (swp_total - swp_free) / swp_total : 0;
    return s;
}

Element historyGraph(const std::deque<double>& history, int width, Color col) {
    return graph([&](int w, int h) {
        std::vector<int> out(w);

        int size = history.size();
        if (size == 0) {
            for (int i = 0; i < w; ++i) out[i] = 0;
            return out;
        }

        for (int i = 0; i < w; ++i) {
            int idx = (int)((double)i / w * size);

            if (idx >= size){
                idx = size - 1;
            }
            double norm = history[idx] / 100.0;
            norm = std::clamp(norm, 0.0, 1.0);

            out[i] = (int)(norm * h);
        }

        return out;
    }) | color(col);
}

Element MemoryInfo(const MemSample& s,const std::deque<double>& mem_hist,const std::deque<double>& swp_hist) {
    auto label = [](const std::string& name, double pct, double used, double total, Color c) {
        std::ostringstream ps, us, ts;
        ps << std::fixed << std::setprecision(0) << pct << "%";
        us << std::fixed << std::setprecision(1) << used;
        ts << std::fixed << std::setprecision(0) << total;
        return hbox({
            text(name + " ") | bold,
            text(ps.str())   | color(c) | size(WIDTH, EQUAL, 5),
            text(us.str() + "GB/" + ts.str() + "GB") | color(Color::GrayLight),
        });
    };

    Color mem_col = s.mem_pct >= 85 ? Color::Red : (s.mem_pct >= 60 ? Color::Yellow : Color::Green);
    Color swp_col = s.swp_pct >= 85 ? Color::Red : (s.swp_pct >= 60 ? Color::Yellow : Color::Cyan);

    return vbox({
        text("Memory Usage") | bold,
        separator(),
        label("Main", s.mem_pct, s.mem_used_gb, s.mem_total_gb, mem_col),
        label("Swap", s.swp_pct, s.swp_used_gb, s.swp_total_gb, swp_col),
        separator(),
        historyGraph(mem_hist, 0, mem_col),
        historyGraph(swp_hist, 0, swp_col),
    });
}

Element temperatures() {
    Elements rows;
    for (auto& hwmon : std::filesystem::directory_iterator("/sys/class/hwmon/")) {
        std::ifstream name_f(hwmon.path() / "name");
        std::string name; std::getline(name_f, name);
        std::ifstream temp_f(hwmon.path() / "temp1_input");
        int m;
        if (temp_f >> m){
            rows.push_back(hbox({ text(name) | flex, text(std::to_string(m / 1000) + "ºC") }));
        }
    }
    return vbox({ text("Temperatures") | bold, separator(), vbox(rows) });
}

Element disks() {
    Elements rows;
    std::ifstream mounts("/proc/mounts");
    std::string device, mountpoint, fstype, rest;
    while (mounts >> device >> mountpoint >> fstype) {
        std::getline(mounts, rest);

        if (fstype == "tmpfs" || fstype == "devtmpfs" || fstype == "proc" ||
            fstype == "sysfs" || fstype == "cgroup" || fstype == "devpts" ||
            fstype == "efivarfs" || fstype == "squashfs" || fstype == "overlay" ||
            fstype == "cgroup2" || fstype == "pstore" || fstype == "bpf" ||
            fstype == "securityfs" || fstype == "configfs") {
                continue;
            }

        if (device.substr(0, 5) != "/dev/") {
            continue;
        }

        struct statvfs st;
        if (statvfs(mountpoint.c_str(), &st) != 0) {
            continue;
        }

        unsigned long long total = st.f_blocks * st.f_frsize;
        unsigned long long avail = st.f_bavail * st.f_frsize;
        unsigned long long used  = total - avail;

        if (total < 1024ULL * 1024) {
            continue;
        }

        std::string name = std::filesystem::path(device).filename();
        rows.push_back(hbox({
            text(name)                                             | size(WIDTH, EQUAL, 10),
            text(mountpoint)                                       | size(WIDTH, EQUAL, 15),
            text(std::to_string((int)(used * 100 / total)) + "%") | size(WIDTH, EQUAL, 8),
            text(std::to_string(avail / (1024*1024*1024)) + " GB"),
        }));
    }
    return vbox({
        text("Disks") | bold, separator(),
        hbox({
            text("Disk")  | size(WIDTH, EQUAL, 10) | bold,
            text("Mount") | size(WIDTH, EQUAL, 15) | bold,
            text("Used")  | size(WIDTH, EQUAL,  8) | bold,
            text("Free")  | bold,
        }),
        separator(), vbox(rows),
    });
}

Element cpuLoad() {
    using CpuStats = std::array<long, 8>;
    auto readAllCpus = [&]() {
        std::vector<CpuStats> res;
        std::ifstream file("/proc/stat");
        std::string line;
        while (std::getline(file, line)) {
            if (line.size() > 3 && line.substr(0,3) == "cpu" && std::isdigit(line[3])) {
                std::istringstream ss(line);
                std::string lbl; CpuStats s{};
                ss >> lbl >> s[0]>>s[1]>>s[2]>>s[3]>>s[4]>>s[5]>>s[6]>>s[7];
                res.push_back(s);
            }
        }
        return res;
    };
    auto cpuPercent = [](const CpuStats& p, const CpuStats& c) {
        long pi=p[3]+p[4], ci=c[3]+c[4];
        long pt=p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7];
        long ct=c[0]+c[1]+c[2]+c[3]+c[4]+c[5]+c[6]+c[7];
        long dt=ct-pt;
        return dt==0?0.0:100.0*(dt-(ci-pi))/dt;
    };
    auto prev = readAllCpus();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto curr = readAllCpus();
    Elements rows;
    for (size_t i = 0; i < curr.size(); i++) {
        std::ostringstream pct;
        int gaugeSize = Terminal::Size().dimx - 36;
        double gaugepct = cpuPercent(prev[i], curr[i]);
        pct << std::fixed << std::setprecision(1) << gaugepct;
        int filled = static_cast<int>((gaugepct / 100.0) * gaugeSize);
        Elements gaugeElements;
        gaugeElements.push_back(text("["));
        for (int j = 0; j < gaugeSize; j++) {
            if (j < filled)
                gaugeElements.push_back(text("#") | color(Color::Cyan));
            else
                gaugeElements.push_back(text("-") | color(Color::GrayDark));
        }
        gaugeElements.push_back(text("]"));
        Elements row;
        row.push_back(text("CPU" + std::to_string(i) + ": " + pct.str() + "% ") | size(WIDTH, EQUAL, 15));
        row.push_back(separator());
        row.push_back(hbox(gaugeElements) | flex);
        rows.push_back(hbox(row));
    }
    return vbox(rows) | size(HEIGHT, EQUAL, (int)curr.size());
}
Component menu(ScreenInteractive& screen) {
    static auto entries = std::make_shared<std::vector<std::string>>(std::vector<std::string>{"MQTT", "Utilities", "Exit"});
    static auto selected = std::make_shared<int>(0);
    static MenuOption option;
    option.on_enter = [&] {
        if (*selected == 2) {
            screen.ExitLoopClosure()();
        }
    };
    static auto m = Menu(entries.get(), selected.get(), option);
    return m;
}

int main() {
    const int HISTORY    = 200;
    const int REFRESH_MS = 700;

    MemSample sample = readMemory();
    std::deque<double> mem_hist(HISTORY, sample.mem_pct);
    std::deque<double> swp_hist(HISTORY, sample.swp_pct);

    std::mutex mtx;
    auto screen = ScreenInteractive::Fullscreen();

    std::thread updater([&] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(REFRESH_MS));
            auto s = readMemory();
            {
                std::lock_guard<std::mutex> lk(mtx);
                sample = s;

                mem_hist.pop_front();
                mem_hist.push_back(s.mem_pct);

                swp_hist.pop_front();
                swp_hist.push_back(s.swp_pct);
            }
            screen.PostEvent(Event::Custom);
        }
    });
    updater.detach();

    auto m = menu(screen);
    auto renderer = Renderer(m, [&] {
        std::lock_guard<std::mutex> lk(mtx);
        return vbox({
            hbox({
                vbox({text("CPU") | bold, separator(), cpuLoad()}) | flex,
                separator(),
                m->Render() | hcenter | vcenter | size(WIDTH, EQUAL, 15),
            }) | border,
            hbox({
                vbox({ disks() | border, temperatures() | border }),
                MemoryInfo(sample, mem_hist, swp_hist) | flex | border,
            }),
        });
    });

    auto component = CatchEvent(renderer, [&](Event e) {
        if (e == Event::Character('q') || e == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return 0;
}
