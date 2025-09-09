// perfetto_async.cpp
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

extern char **environ;

struct PerfettoHandle {
    pid_t pid = -1;                // --background perfetto PID
    std::string detach_key;        // --detach session key
};

static int spawn_proc(const std::vector<std::string>& argv) {
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (auto& s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
    cargv.push_back(nullptr);
    pid_t pid;
    if (posix_spawnp(&pid, argv[0].c_str(), nullptr, nullptr, cargv.data(), environ) != 0) {
      return -1;
    }
    return pid;
}

// Mode A: --background beginning (PID returned)
std::optional<PerfettoHandle> start_background(
    const std::string& config_pbtx,
    const std::string& out_path,
    bool use_su = false) {

    std::vector<std::string> argv;
    if (use_su) {
        argv = {"/system/bin/su","-c",
                "/system/bin/perfetto --background --txt -c " + config_pbtx + " -o " + out_path};
    } else {
        argv = {"/system/bin/perfetto","--background","--txt","-c",config_pbtx,"-o",out_path};
    }
  
    int pid = spawn_proc(argv);
    if (pid <= 0) return std::nullopt;
    PerfettoHandle h; h.pid = pid;
    return h;
}

// Mode A terminate: PID Signal (clean shutdown)
bool stop_background(const PerfettoHandle& h) {
  if (h.pid <= 0) return false;
  // SIGTERM → if fails, consider SIGKILL
  if (kill(h.pid, SIGTERM) != 0) return false;
  // recover zombie (non-blocking)
  int status = 0; waitpid(h.pid, &status, WNOHANG);
  return true;
}

// Mode B: --detach=KEY beginning (KEY returned)
std::optional<PerfettoHandle> start_detached(
    const std::string& config_pbtx,
    const std::string& out_path,
    const std::string& key,
    bool use_su = false) {

    // detached mode requires "write_into_file:true in config"
    std::vector<std::string> argv;
    if (use_su) {
        argv = {"/system/bin/su","-c",
                "/system/bin/perfetto --txt -c " + config_pbtx +
                " --detach=" + key + " -o " + out_path};
    } else {
        argv = {"/system/bin/perfetto","--txt","-c",config_pbtx,
                "--detach="+key,"-o",out_path};
    }
    int pid = spawn_proc(argv);
    if (pid <= 0) return std::nullopt;
    PerfettoHandle h; h.detach_key = key;
    return h;
}

// Mode B termination: --attach=KEY --stop call (graceful shutdown)
bool stop_detached(const PerfettoHandle& h, bool use_su = false) {
  if (h.detach_key.empty()) return false;
  std::vector<std::string> argv;
  if (use_su) {
    argv = {"/system/bin/su","-c",
            "/system/bin/perfetto --attach=" + h.detach_key + " --stop"};
  } else {
    argv = {"/system/bin/perfetto","--attach="+h.detach_key,"--stop"};
  }
  return spawn_proc(argv) > 0;
}

int main(int argc, char** argv) {
  // requirements: /sdcard/ access permission in Termux → `termux-setup-storage` prequisite
  const std::string cfg = "/sdcard/Download/power.pbtx";
  const std::string out = "/sdcard/Download/trace.perfetto-trace";

  bool rooted = (access("/system/bin/su", X_OK) == 0 ||
                 access("/system/xbin/su", X_OK) == 0);

  // --- Mode A: background ---
  auto h = start_background(cfg, out, rooted);
  if (!h) { std::cerr << "failed to start perfetto\n"; return 1; }
  // ... task here ...

  // if early termination is needed:
  stop_background(*h);

  // --- or Mode B: detached ---
  // auto h2 = start_detached("/sdcard/Download/power_detached.pbtx", out, "my_sess", rooted);
  // ... task ...
  // stop_detached(*h2, rooted);

  return 0;
}