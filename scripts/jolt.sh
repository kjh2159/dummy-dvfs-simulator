# silver cores off
su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"

./build/bin/thermo_jolt \
    -t 5 \
    -d 50 \
    -p 0 \
    -o output \
    --cpu-clock $1 \
    --ram-clock $2 \
    --pulse-cpu-clock 0 \
    --pulse-ram-clock 0