# _DDS_: Dummy DVFS Simulator

## 📙 Description

This Dummy DVFS Simulator has two functionalities:
1. CPU burner (`cpu_burner.cpp`)
2. LLM-mimicry simulator (`dummy_test.cpp`)

In the case of the CPU burner, it can lead to full utilization of the selected cores within a specified duration.
However, in the case of the LLM-mimicry simulator can lead to high CPU and RAM utilization.

## 🎮 Compatibility

|Backend|Runnable|
|-------|--------|
|Windows|❌|
|Linux (amd64/ARM)|✅|
|MacOS (ARM)|✅|
|Android (ARM)|✅|


## 🚀 Quick Start

### STEP 1: clone
```shell
git clone https://github.com/kjh2159/dummy-dvfs-sumulator.git dds
cd dds
```

### STEP 2: build
```shell
mkdir -p build output
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### STEP 3: run
```shell
./build/bin/cpu_burner -d 300 -t 5 
```

## 🪐 Details

### Abbreviation

- `N`: A nature number.
- `S`: A string (character sequence).


### 1. CPU Burner

A program to alternate compute-intensive workload and idle time for the given duration.

- `-t N`: The number of threads to activate
- `-d N` or `--duration N`: The length of duration to load
- `-b N` or `--burst N`: The basis of the length for computational workload
- `-p N` or `--pause N`: The basis of the length for idle time
- `--device S`: The device name for execution (default: Pixel9)
- `-o S` or `--output S`: The directory path to save output
- `-c N` or `--cpu-clock N`: The index number of cpu frequencies to set cpu clock
- `-r N` or `--ram-clock N`: The index number of ram frequencies to set ram clock

### 2. Thermo Jolt

A program to impose a sudden workload while maintaning CPU temerature.

- `-t N`: The number of threads to activate
- `-d N` or `--duration N`: The length of duration to load
- `-p N` or `--pulse N`: The basis of the length for pulse
- `--device S`: The device name for execution (default: Pixel9)
- `-o S` or `--output S`: The directory path to save output
- `--cpu-clock N`: The index number of cpu frequencies to set cpu clock for **temperature maintainence**
- `--ram-clock N`: The index number of ram frequencies to set ram clock for **temperature maintainence**
- `--pulse-cpu-clock N`: The index number of cpu frequencies to set cpu clock for **pulse**
- `--pulse-ram-clock N`: The index number of ram frequencies to set ram clock for **pulse**


## ✨ Future features

- [x] Perfetto measurement integration
- [x] CPU/RAM clock indices checker
