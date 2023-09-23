#######################################################################################################
# CLBlast can be installed as follows on Ubuntu 20.04:
# sudo add-apt-repository ppa:cnugteren/clblast
# sudo apt-get update
# sudo apt install libclblast-dev

# Setup Mali-GPU OpenCL
# git clone https://github.com/krrishnarraj/libopencl-stub
# cd libopencl-stub; mkdir b; cd b; cmake ..; make; make install; cd ..; rm -r b;
#
# Setup VC4CL for RPI
# https://github.com/doe300/VC4CL/wiki/How-to-get
#
# Install OpenCV4 from the Ubuntu 20.04 Repository
# sudo apt update
# sudo apt install libopencv-dev

include ../conf.mk

VPATH=./src/:./examples
SLIB=libdarknet.so
ALIB=libdarknet.a
EXEC=darknet
OBJDIR=./obj/

CC=$(CROSS_COMPILE)gcc
CPP=$(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar
ARFLAGS=rcs
OPTS=
COMMON= -Iinclude/ -Isrc/
CFLAGS=-Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-result -Wno-deprecated-declarations -Wno-unused-function -Wfatal-errors -fPIC

ifeq ($(SECURITY),1)
COMMON+= -DSECURITY -DDEFAULT_MAX_THREAD_NUM=$(THREADS_NUM)
endif

ifeq ($(ATTACK),1)
COMMON+= -DATTACK
endif

ifeq ($(ARM), 1)
LDFLAGS= -lm -lpthread
else
LDFLAGS= -lm -lz -lpthread
endif

ifeq ($(DEBUG), 1)
OPTS=-O0 -g
else
ifeq ($(ARM), 1)
OPTS=-O2
else
OPTS=-O3
endif

endif

ifeq ($(OPENMP), 1)
CFLAGS+= -openmp
COMMON+= -fopenmp
endif

CFLAGS+=$(OPTS)

ifeq ($(OPENCV), 1) 
COMMON+= -DOPENCV
CFLAGS+= -DOPENCV
LDFLAGS+= `pkg-config --libs opencv4 2> /dev/null || pkg-config --libs opencv`
COMMON+= `pkg-config --cflags opencv4 2> /dev/null || pkg-config --cflags opencv`
endif

ifeq ($(ARM), 1)
ifeq ($(GPU), 1)
COMMON+= -DGPU -DOPENCL -DCL_TARGET_OPENCL_VERSION=120 -DARM 
CFLAGS+= -DGPU -DOPENCL -DARM -I/usr/include/ -I/usr/local/include/
LDFLAGS+= -L/usr/local/lib -L/usr/lib/aarch64-linux-gnu -lOpenCL -lclblast 
LDFLAGS+= -L/usr/lib
else
COMMON+= -DARM
CFLAGS+= -DARM -I/usr/include/ -I/usr/local/include/
LDFLAGS+= -L/usr/local/lib -L/usr/lib/aarch64-linux-gnu -lz
LDFLAGS+= -L/usr/lib
endif
endif

ifeq ($(GPU), 1)
ifeq ($(INTELGPU),1)
COMMON+= -DGPU -DOPENCL -DCL_TARGET_OPENCL_VERSION=120
CFLAGS+= -DGPU -DOPENCL -I/usr/include/
LDFLAGS+= -L/usr/lib/x86_64-linux-gnu/ -lOpenCL -L/usr/lib64 -lclblast -L/usr/local/lib
endif
ifeq ($(NVIDIA), 1)
COMMON+= -DGPU -DOPENCL
CFLAGS+= -DGPU -DOPENCL -I/usr/include/ -I/usr/local/cuda/include/
LDFLAGS+= -L/usr/local/cuda/lib64 -lOpenCL -L/usr/lib64 -lclblast -L/usr/local/lib
endif
endif

ifeq ($(GPU_FAST), 1)
COMMON+= -DGPU_FAST
CFLAGS+= -DGPU_FAST
endif

ifeq ($(GPU_MULTI), 1)
COMMON+= -DGPU_MULTI
CFLAGS+= -DGPU_MULTI
endif

ifeq ($(BENCHMARK), 1)
COMMON+= -DBENCHMARK
CFLAGS+= -DBENCHMARK
endif

ifeq ($(LOSS_ONLY), 1)
COMMON+= -DLOSS_ONLY
CFLAGS+= -DLOSS_ONLY
endif

OBJ=gemm.o utils.o opencl.o threadpool.o convolutional_layer.o list.o softmax_layer.o
OBJ += image.o activations.o im2col.o col2im.o blas.o maxpool_layer.o data.o matrix.o network.o
OBJ += cost_layer.o parser.o option_list.o detection_layer.o route_layer.o box.o
OBJ += normalization_layer.o layer.o local_layer.o shortcut_layer.o activation_layer.o demo.o
OBJ += batchnorm_layer.o tree.o yolo_layer.o yolo4_layer.o upsample_layer.o logistic_layer.o
OBJ += image_opencv.o avgpool_layer.o verfication_host.o tee_file_tool.o
EXECOBJA=detector.o darknet.o classifier.o attack_simulate.o
ifeq ($(GPU), 1) 
LDFLAGS+= -lstdc++
OBJ+=convolutional_kernels.o activation_kernels.o im2col_kernels.o col2im_kernels.o avgpool_layer_kernels.o blas_kernels.o maxpool_layer_kernels.o 
else
LDFLAGS+= -lstdc++
OBJ+=cpu.o
endif

ifeq ($(SECURITY),1)
CFLAGS += -I../ta1/include -I../ta2/include -I$(TEEC_EXPORT)/include
LDFLAGS += -L$(TEEC_EXPORT)/lib -lteec
endif

EXECOBJ = $(addprefix $(OBJDIR), $(EXECOBJA))
OBJS = $(addprefix $(OBJDIR), $(OBJ))
DEPS = $(wildcard src/*.h) Makefile include/darknet.h

#all: obj backup results $(SLIB) $(ALIB) $(EXEC)
all: obj  results $(SLIB) $(ALIB) $(EXEC)


$(EXEC): $(EXECOBJ) $(ALIB)
	$(CC) $(COMMON) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(ALIB)

$(ALIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(SLIB): $(OBJS)
	$(CC) $(CFLAGS) -shared $^ -o $@ $(LDFLAGS)

$(OBJDIR)%.o: %.cpp $(DEPS)
	$(CPP) $(COMMON) $(CFLAGS) -c $< -o $@

$(OBJDIR)%.o: %.c $(DEPS)
	$(CC) $(COMMON) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj
backup:
	mkdir -p backup
results:
	mkdir -p results

.PHONY: clean

clean:
	rm -rf $(OBJS) $(SLIB) $(ALIB) $(EXEC) $(EXECOBJ)
