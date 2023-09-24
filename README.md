# darknet_verification

### Description

This code is the companion code of paper xxx

### prerequisites

**OpenCL:** 1.2 or later
[**OP-TEE:** ](https://github.com/OP-TEE/optee_os)3.16
**Other:** gcc g++ cmake clblast
**clblast install command：** 

```shell
sudo apt install software-properties-common
sudo add-apt-repository ppa:cnugteren/clblast
sudo apt-get update
sudo apt install libclblast-dev
```

### Compile

Adjust the build configuration in conf.mk, and then compile directly with the make tool. You can compile directly on edge devices or use cross compilation.
### Run
The compiled executable darknet is in host folder. Pre-training models can be downloaded from [https://pjreddie.com/darknet/](https://pjreddie.com/darknet/)

When running trusted inference for a model for the first time, you need to execute the following command (let's use the yolov4-tiny model as an example).
```shell
./darknet store cfg/yolov4-tiny.cfg yolov4-tiny.weights
```

Object detection model single graph test command
```shell
./darknet detector test cfg/coco.data cfg/yolov4-tiny.cfg yolov4-tiny.weights data/dog.jpg
```

Object detection model multi-graph continuous test
- First of all, you need to modify the location of the valid picture list file in the file cfg/coco.data.
- Command
```shell
./darknet detector valid cfg/coco.data cfg/yolov4-tiny.cfg yolov4-tiny.weights
```

Classification model single graph test command
```shell
./darknet classifier predict cfg/imagenet1k.data cfg/resnet18.cfg resnet18.weights data/dog.jpg
```

Classification model multi-graph continuous test
- First of all, you need to modify the location of the valid picture list file in the file cfg/imagenet1k.data.
- Command
```shell
./darknet classifier label cfg/imagenet1k.data cfg/resnet18.cfg resnet18.weights
```
