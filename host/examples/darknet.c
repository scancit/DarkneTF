#include "darknet.h"
#include "tee_flie_tool.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "verfication_host.h"

extern void test_detector(char *datacfg, char *cfgfile, char *weightfile, char *filename, float thresh, float hier_thresh, char *outfile, int fullscreen);
extern void run_detector(int argc, char **argv);
extern void run_classifier(int argc, char **argv);

long numops(network *net)
{
    int i;
    long ops = 0;
    for (i = 0; i < net->n; ++i)
    {
        layer l = net->layers[i];
        if (l.type == CONVOLUTIONAL)
        {
            ops += 2l * l.n * l.size * l.size * l.c / l.groups * l.out_h * l.out_w;
        }
    }
    return ops;
}

void speed(char *cfgfile, int tics)
{
    if (tics == 0)
        tics = 1000;
    network *net = parse_network_cfg(cfgfile);
    set_batch_network(net, 1);
    int i;
    double time = what_time_is_it_now();
    image im = make_image(net->w, net->h, net->c * net->batch);
    for (i = 0; i < tics; ++i)
    {
        network_predict(net, im.data);
    }
    double t = what_time_is_it_now() - time;
    long ops = numops(net);
    printf("\n%d evals, %f Seconds\n", tics, t);
    printf("Floating Point Operations: %.2f Bn\n", (float)ops / 1000000000.);
    printf("FLOPS: %.2f Bn\n", (float)ops / 1000000000. * tics / t);
    printf("Speed: %f sec/eval\n", t / tics);
    printf("Speed: %f Hz\n", tics / t);
}

void operations(char *cfgfile)
{
    gpu_index = -1;
    network *net = parse_network_cfg(cfgfile);
    long ops = numops(net);
    printf("Floating Point Operations: %ld\n", ops);
    printf("Floating Point Operations: %.2f Bn\n", (float)ops / 1000000000.);
}

layer normalize_layer(layer l, int n)
{
    int j;
    l.batch_normalize = 1;
    l.scales = calloc(n, sizeof(float));
    for (j = 0; j < n; ++j)
    {
        l.scales[j] = 1;
    }
    l.rolling_mean = calloc(n, sizeof(float));
    l.rolling_variance = calloc(n, sizeof(float));
    return l;
}

void statistics_net(char *cfgfile, char *weightfile)
{
    gpu_index = -1;
    network *net = load_network(cfgfile, weightfile, 0);
    int i;
    for (i = 0; i < net->n; ++i)
    {
        layer l = net->layers[i];
        printf("\n");
    }
}

void visualize(char *cfgfile, char *weightfile)
{
    network *net = load_network(cfgfile, weightfile, 0);
    visualize_network(net);
}

#ifdef SECURITY
void store_security_weight_file(char *cfg,char *weights){
    TEEC_Result res=cfg_to_tee_file(cfg);
    if(res!=TEEC_SUCCESS){
        return;
    }
    network* net=parse_network_cfg(cfg);
    weight_to_tee_flie(net,weights,0,net->n);
    
}
#endif

int main(int argc, char *argv[])
{
    // test_box();
    // test_convolutional_layer();
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <function>\n", argv[0]);
        return 0;
    }
    int ngpus = 0;
    int *gpus = 0;
    if (read_arg(argc, argv, "-nogpu"))
    {
        ngpus = 0;
        gpu_index = -1;
    }
    else if (read_arg(argc, argv, "-i"))
    {
        gpus = calloc(1, sizeof(int));
        gpus[0] = find_int_arg(argc, argv, "-i", 0);
        ngpus = 1;
        gpu_index = 1;
    }
    else if (read_arg(argc, argv, "-gpus"))
    {
        char *gpu_list = read_char_arg(argc, argv, "-gpus", *argv);
        gpus = read_intlist(gpu_list, &ngpus, gpu_index);
        gpu_index = ngpus;
    }
    else
    {
        gpus = calloc(1, sizeof(int));
        gpus[0] = 0;
        ngpus = 1;
        gpu_index = 1;
    }

#ifndef GPU
    gpu_index = -1;
#else
    if (gpu_index >= 0)
    {
        gpusg = gpus;
        ngpusg = ngpus;
        opencl_init(gpus, ngpus);
    }
#endif

    if (0 == strcmp(argv[1], "detector"))
        run_detector(argc, argv);
    else if (0 == strcmp(argv[1], "detect"))
    {
        char *filename = (argc > 5) ? argv[5] : 0;
        float thresh = find_float_arg(argc, argv, "-thresh", .5);
        char *outfile = find_char_arg(argc, argv, "-out", 0);
        int fullscreen = find_arg(argc, argv, "-fullscreen");
        test_detector(argv[2], argv[3], argv[4], filename, thresh, .5, outfile, fullscreen);
    }
    else if (0 == strcmp(argv[1], "statistics"))
        statistics_net(argv[2], argv[3]);
    else if (0 == strcmp(argv[1], "ops"))
        operations(argv[2]);
    else if (0 == strcmp(argv[1], "speed"))
        speed(argv[2], (argc > 3 && argv[3]) ? atoi(argv[3]) : 0);
    else if (0 == strcmp(argv[1], "visualize"))
        visualize(argv[2], (argc > 3) ? argv[3] : 0);
    else if (0 == strcmp(argv[1], "classifier")){
        run_classifier(argc, argv);
    }
#ifdef SECURITY
    else if(0==strcmp(argv[1],"store"))
    {
        store_security_weight_file(argv[2],argv[3]);
    }
#endif
    else
        fprintf(stderr, "Not an option: %s\n", argv[1]);
#ifdef GPU
    if (gpu_index >= 0)
    {
        opencl_deinit(gpusg, ngpusg);
    }
    free(gpusg);
#endif
    return 0;
}
