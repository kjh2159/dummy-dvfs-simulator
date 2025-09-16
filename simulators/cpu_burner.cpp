// cpu_burner.cpp — Android/Termux load generator for DVFS testing
// build: 
//   ex) g++ -O0 -std=c++20 -pthread cpu_burner.cpp -o cpu_burner
// usage:
//   ex) ./cpu_burner             
//       --threads 8            # number of threads (default: # of online CPUs)
//       --duration 40         # duration time in seconds (default: 40s)
//       --burst 4             # compute burst time in seconds (default: 4s)
//       --pause 6             # pause (idle) time in seconds (default: 6s)
//       --device Pixel9      # specify phone type [Pixel9 | S24] (default: Pixel9)
//       --cpu-clock 12       # CPU clock index for DVFS (maintain) (default: -1 [off])
//       --ram-clock 11       # RAM clock index for DVFS (maintain) (default: -1 [off])
//       --output output/     # specify output directory path (default: output/)
//       --nopin              # do not pin threads to specific cores (default: pin to cores)
//       --help               # show this message
// termination:     
//   Ctrl+C

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>

#include <unistd.h>

#if defined(__linux__) || defined(__ANDROID__)
// linux header
  #include <sys/syscall.h>
  #include <sched.h>
  #include <sys/resource.h>
#elif defined(__APPLE__)
// macOS: Mach affinity tag
  #include <mach/mach.h>
  #include <mach/thread_policy.h>
#endif

#include "cmdline.h"
#include "utils/util.hpp"
#include "hardware/dvfs.h"
#include "hardware/record.h"

using namespace std::chrono;

static std::atomic<bool> g_stop{false};
static std::atomic<bool> g_work{true};
std::atomic_bool sigterm(false);

static void on_sigint(int) {
    g_stop.store(true, std::memory_order_relaxed);
}

// /sys/devices/system/cpu/online parsing
static std::vector<int> read_online_cpus() {
    std::ifstream f("/sys/devices/system/cpu/online");
    std::string s;
    if (!(f >> s)) return {}; // fail -> empty vector
    std::vector<int> cpus;

    auto add_range = [&](int a, int b){
        for (int i = a; i <= b; ++i) cpus.push_back(i);
    };

    size_t i = 0;
    while (i < s.size()) {
        // parse number
        int a = 0, b = -1;
        bool neg = false;
        if (s[i] == ',') { ++i; continue; }
        // read a
        while (i < s.size() && isdigit(s[i])) { a = a*10 + (s[i]-'0'); ++i; }
        if (i < s.size() && s[i] == '-') {
            ++i;
            b = 0;
            while (i < s.size() && isdigit(s[i])) { b = b*10 + (s[i]-'0'); ++i; }
        }
        if (b < 0) b = a;
        add_range(a, b);
        if (i < s.size() && s[i] == ',') ++i;
    }
    std::sort(cpus.begin(), cpus.end());
    cpus.erase(std::unique(cpus.begin(), cpus.end()), cpus.end());
    return cpus;
}

// affine thread to specific core
static bool pin_to_core(int core_id) {
#if defined(__linux__) || defined(__ANDROID__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core_id, &set);
     // Thread TID get
    pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
    if (sched_setaffinity(tid, sizeof(set), &set) != 0) {
        return false;
    }
    return true;

#elif defined(__APPLE__)
    // macOS has no core "pin" API.
    // instead, can give scheduler hint by affinity tag (try to put same tag threads close).
    // warning: not pin to specific core. if not required, just `return true;`.
    thread_affinity_policy_data_t policy = { (integer_t)((core_id % 16) + 1) };
    kern_return_t kr = thread_policy_set(
        mach_thread_self(),
        THREAD_AFFINITY_POLICY,
        (thread_policy_t)&policy,
        THREAD_AFFINITY_POLICY_COUNT
    );
    return kr == KERN_SUCCESS;

#else
    (void)core_id;
    return true; // 기타 OS: no-op
#endif
}

// nice increase to boost scheduling priority (may fail if not root)
static void try_bump_priority() {
    // negative nice needs root.
    // failure is ignored.
    setpriority(PRIO_PROCESS, 0, -5);
}

// busy loop: FMA-heavy floating point + LCG integer ops
static void hot_loop(std::atomic<bool>& stop_flag, std::atomic<bool>& work_flag) {
    // false sharing mitigation by align
    alignas(64) volatile double v0 = 1.000001, v1 = 0.999999, v2 = 1.000003, v3 = 0.999997;
    uint32_t rng = 123456789u;

    // overhead minimization by large chunk
    while (!stop_flag.load(std::memory_order_relaxed)) {
        if (!work_flag.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        #pragma clang loop unroll(full)
        for (int i = 0; i < 1'000'000; ++i) {
            //FMA
            v0 = v0 * 1.0000001 + 0.9999999;
            v1 = v1 * 0.9999997 + 1.0000003;
            v2 = v2 * 1.0000002 + 0.9999998;
            v3 = v3 * 0.9999996 + 1.0000004;

            //LCG
            rng = rng * 1664525u + 1013904223u;

            // value range control
            if (v0 > 1e30) v0 = 1.0;
            if (v1 < 1e-30) v1 = 1.0;
        }
        // To make not be optimized out by compiler
        // memory barrier-like effect
        asm volatile("" :: "r"(v0), "r"(v1), "r"(v2), "r"(v3), "r"(rng) : "memory");
    }
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::signal(SIGINT, on_sigint);

    /* option parsing */
    cmdline::parser cmdParser;
    cmdParser.add("help", 'h', "print this help message");
    cmdParser.add("nopin", 0, "do NOT pin threads to specific cores");
    cmdParser.add<int>("threads", 't', "number of threads (default: # of online CPUs)", false, -1);
    cmdParser.add<int>("duration", 'd', "duration time in seconds (default: 10s)", false, 10);
    cmdParser.add<int>("burst", 'b', "computation burst time in seconds (default: 5s)", false, 5);
    cmdParser.add<int>("pause", 'p', "pause (idle) time in seconds (default: 5s)", false, 5);
    cmdParser.add<std::string>("device", 0, "specify phone type [Pixel9 | S24] (default: Pixel9)", false, "Pixel9");
    cmdParser.add<std::string>("output", 'o', "specify output directory path (default: output/)", false, "output/");
    // dvfs options
    cmdParser.add<int>("cpu-clock", 'c', "CPU clock index for DVFS (default: -1 [off])", false, -1);
    cmdParser.add<int>("ram-clock", 'r', "CPU clock index for DVFS (default: -1 [off])", false, -1);
    cmdParser.parse_check(argc, argv);
    
    // get options
    bool pin = cmdParser.exist("nopin") ? false : true;
    int threads = cmdParser.get<int>("threads"); // -1 -> all online cores
    int duration_sec = cmdParser.get<int>("duration") > 0 ? cmdParser.get<int>("duration") : 0; 
    const int compute_burst_sec = cmdParser.get<int>("burst") > 0 ? cmdParser.get<int>("burst") : 0;
    const int pause_sec = cmdParser.get<int>("pause") > 0 ? cmdParser.get<int>("pause") : 0;
    const std::string device_name = cmdParser.get<std::string>("device");
    const std::string output_dir = cmdParser.get<std::string>("output");
    // dvfs options
    const int cpu_clk_idx = cmdParser.get<int>("cpu-clock");
    const int ram_clk_idx = cmdParser.get<int>("ram-clock");
    

    // TODO: kernel hard recording path refinement
    // output file join
    std::string output_hard = joinPaths(
        output_dir, 
        std::string("kernel_hard_") + std::to_string(cpu_clk_idx) + "_" + std::to_string(ram_clk_idx) + std::string(".txt")
    );

    // option validation
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-t" && i+1 < argc) {
            threads = std::max(1, atoi(argv[++i]));
        } else if (a == "-nopin") {
            pin = false;
        } else if (a == "-d" && i+1 < argc) {
            duration_sec = std::max(1, atoi(argv[++i]));
        } else if (a == "-h" || a == "--help") {
            std::cout <<
            "Usage: " << argv[0] << " [-t N] [-nopin] [-d seconds]\n"
            "  -t N       : number of threads (default: #online CPUs)\n"
            "  -nopin     : do NOT pin threads to specific cores\n"
            "  -d seconds : auto-stop after given seconds\n";
            return 0;
        }
    }

    auto cpus = read_online_cpus();
    int online = cpus.empty() ? (int)std::thread::hardware_concurrency() : (int)cpus.size();
    if (online <= 0) online = 1;

    if (threads <= 0) threads = online;
    if (!cpus.empty() && threads > (int)cpus.size()) threads = (int)cpus.size();

    std::cout << "cpu_burner: threads=" << threads
              << ", pin=" << (pin ? "yes" : "no")
              << ", duration=" << (duration_sec > 0 ? std::to_string(duration_sec) + "s" : "infinite")
              << ", online_cpus=" << online << "\n";

    try_bump_priority();

    std::atomic<bool> stop = false;
    if (duration_sec > 0) {
        std::thread([&stop, duration_sec]{
            std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
            stop.store(true, std::memory_order_relaxed);
        }).detach();
    }

    std::vector<std::thread> ths;
    ths.reserve(threads);

    // DVFS setting
    DVFS dvfs(device_name);
    dvfs.output_filename = output_hard;
    // cpu clock candidates
    std::vector<int> freq_config = dvfs.get_cpu_freqs_conf(cpu_clk_idx);
    for (auto f : freq_config) { std::cout << f << " "; } std::cout << std::endl; // to validate (print freq-configuration)
    // dvfs setting
    dvfs.set_cpu_freq(freq_config);
    dvfs.set_ram_freq(ram_clk_idx);
    // start recording
    std::thread record_thread = std::thread(record_hard, std::ref(sigterm), dvfs);

    // stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::thread phase_thread([&]{
        using namespace std::chrono_literals;
        while (!g_stop.load(std::memory_order_relaxed) &&
               !stop.load(std::memory_order_relaxed)) {

            // burst phase (compute_burst_sec)
            g_work.store(true, std::memory_order_relaxed);
            std::cout << "[BURST] " << compute_burst_sec << "s\n";
            for (int s = 0; s < compute_burst_sec &&
                 !g_stop.load(std::memory_order_relaxed) &&
                 !stop.load(std::memory_order_relaxed); ++s) {
                std::this_thread::sleep_for(1s);
            }

            // pause phase (pause_sec)
            g_work.store(false, std::memory_order_relaxed);
            std::cout << "[PAUSE] " << pause_sec << "s\n";
            for (int s = 0; s < pause_sec &&
                 !g_stop.load(std::memory_order_relaxed) &&
                 !stop.load(std::memory_order_relaxed); ++s) {
                std::this_thread::sleep_for(1s);
            }
        }
    });
    
    for (int i = 0; i < threads; ++i) {
        ths.emplace_back([&, i]{
            if (pin && !cpus.empty()) {
                int core_id = cpus[i % cpus.size()];
                (void)pin_to_core(core_id);
            }
            hot_loop(stop, g_work);
        });
    }

    // 메인에서 SIGINT 감시
    while (!g_stop.load(std::memory_order_relaxed) &&
           !stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(500ms);
    }
    stop.store(true, std::memory_order_relaxed);

    for (auto& t : ths) t.join();

    std::cout << "cpu_burner: done.\n";

    // done
    sigterm = true;
    dvfs.unset_cpu_freq();
    dvfs.unset_ram_freq();
    if (phase_thread.joinable()) phase_thread.join();
    record_thread.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return 0;
}
