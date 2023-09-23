cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
add_subdirectory("host")
#add_custom_target(ta COMMAND make -C ta CROSS_COMPILE=aarch64-linux-gnu- O=out TA_DEV_KIT_DIR=/root/darknet-rand-attack/export-ta_arm64 LDFLAGS="")