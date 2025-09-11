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
