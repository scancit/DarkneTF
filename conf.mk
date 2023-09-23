GPU=0
GPU_FAST=0
GPU_MULTI=0
OPENCV=0
SECURITY=0
OPENMP=0
ATTACK=0
DEBUG=0
BENCHMARK=0
LOSS_ONLY=0
# Choose only one (works if GPU=1): NVIDIA or ARM (for VC4CL or MaliGPU)
INTELGPU=0
NVIDIA=0
ARM=1

#Number of threads in the thread pool
THREADS_NUM=2


# If _HOST or _TA specific compilers are not specified, then use CROSS_COMPILE
ifeq ($(ARM),1)
CROSS_COMPILE ?= aarch64-linux-gnu-
endif

ifeq ($(SECURITY),1)
#Define where is export dir of ta file
TA_EXPORT ?=out
#Define where is optee_os export dir
TA_DEV_KIT_DIR ?= /root/darknet_verification/export-ta_arm64
#Define where is optee_client export dir
TEEC_EXPORT ?=/usr
endif
