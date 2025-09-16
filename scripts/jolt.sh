# screen off
su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness"

# silver cores off
su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"

./build/bin/thermo_jolt \
    -t 5 \
    -d 30 \
    -p 0 \
    -o output \
    --cpu-clock $1 \
    --ram-clock $2 \
    --pulse-cpu-clock $3 \
    --pulse-ram-clock $4

# screen on
su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness"