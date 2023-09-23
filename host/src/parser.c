#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "activation_layer.h"
#include "logistic_layer.h"
#include "activations.h"
#include "batchnorm_layer.h"
#include "blas.h"
#include "convolutional_layer.h"
#include "cost_layer.h"
#include "detection_layer.h"
#include "list.h"
#include "local_layer.h"
#include "maxpool_layer.h"
#include "normalization_layer.h"
#include "option_list.h"
#include "parser.h"
#include "yolo_layer.h"
#include "yolo4_layer.h"
#include "route_layer.h"
#include "upsample_layer.h"
#include "shortcut_layer.h"
#include "utils.h"
#include "tee_flie_tool.h"
#include "softmax_layer.h"
#include "avgpool_layer.h"

typedef struct{
    char *type;
    list *options;
}section;

list *read_cfg(char *filename);

LAYER_TYPE string_to_layer_type(char * type)
{

    if (strcmp(type, "[shortcut]")==0) return SHORTCUT;
    if (strcmp(type, "[cost]")==0) return COST;
    if (strcmp(type, "[detection]")==0) return DETECTION;
    if (strcmp(type, "[yolo]")==0) return YOLO;
    if (strcmp(type, "[yolo4]")==0) return YOLO4;
    if (strcmp(type, "[yolo7]")==0) return YOLO7;
    if (strcmp(type, "[local]")==0) return LOCAL;
    if (strcmp(type, "[conv]")==0
            || strcmp(type, "[convolutional]")==0) return CONVOLUTIONAL;
    if (strcmp(type, "[activation]")==0) return ACTIVE;
    if (strcmp(type, "[logistic]")==0) return LOGXENT;
    if (strcmp(type, "[net]")==0
            || strcmp(type, "[network]")==0) return NETWORK;
    if (strcmp(type, "[max]")==0
            || strcmp(type, "[maxpool]")==0) return MAXPOOL;
    if (strcmp(type, "[avg]")==0
            || strcmp(type, "[avgpool]")==0) return AVGPOOL;
    if (strcmp(type, "[lrn]")==0
            || strcmp(type, "[normalization]")==0) return NORMALIZATION;
    if (strcmp(type, "[batchnorm]")==0) return BATCHNORM;
    if (strcmp(type, "[soft]")==0
            || strcmp(type, "[softmax]")==0) return SOFTMAX;
    if (strcmp(type, "[route]")==0) return ROUTE;
    if (strcmp(type, "[upsample]")==0) return UPSAMPLE;
    return BLANK;
}

void free_section(section *s)
{
    free(s->type);
    node *n = s->options->front;
    while(n){
        kvp *pair = (kvp *)n->val;
        free(pair->key);
        free(pair);
        node *next = n->next;
        free(n);
        n = next;
    }
    free(s->options);
    free(s);
}

void parse_data(char *data, float *a, int n)
{
    int i;
    if(!data) return;
    char *curr = data;
    char *next = data;
    int done = 0;
    for(i = 0; i < n && !done; ++i){
        while(*++next !='\0' && *next != ',');
        if(*next == '\0') done = 1;
        *next = '\0';
        sscanf(curr, "%g", &a[i]);
        curr = next+1;
    }
}

typedef struct size_params{
    int batch;
    int inputs;
    int h;
    int w;
    int c;
    int index;
    int time_steps;
    int train;
    network *net;
} size_params;

local_layer parse_local(list *options, size_params params)
{
    int n = option_find_int(options, "filters",1);
    int size = option_find_int(options, "size",1);
    int stride = option_find_int(options, "stride",1);
    int pad = option_find_int(options, "pad",0);
    char *activation_s = option_find_str(options, "activation", "logistic");
    ACTIVATION activation = get_activation(activation_s);

    int batch,h,w,c;
    h = params.h;
    w = params.w;
    c = params.c;
    batch=params.batch;
    if(!(h && w && c)) error("Layer before local layer must output image.");

    local_layer layer = make_local_layer(batch,h,w,c,n,size,stride,pad,activation);

    return layer;
}

convolutional_layer parse_convolutional(list *options, size_params params)
{
    int n = option_find_int(options, "filters",1);
    int size = option_find_int(options, "size",1);
    int stride = option_find_int(options, "stride",1);
    int pad = option_find_int_quiet(options, "pad",0);
    int padding = option_find_int_quiet(options, "padding",0);
    int groups = option_find_int_quiet(options, "groups", 1);
    if(pad) padding = size/2;

    char *activation_s = option_find_str(options, "activation", "logistic");
    ACTIVATION activation = get_activation(activation_s);

    int batch,h,w,c;
    h = params.h;
    w = params.w;
    c = params.c;
    batch=params.batch;
    if(!(h && w && c)) error("Layer before convolutional layer must output image.");
    int batch_normalize = option_find_int_quiet(options, "batch_normalize", 0);

    convolutional_layer layer = make_convolutional_layer(batch,h,w,c,n,groups,size,stride,padding,activation, batch_normalize, params.net->adam);
    layer.flipped = option_find_int_quiet(options, "flipped", 0);
    layer.dot = option_find_float_quiet(options, "dot", 0);

    return layer;
}

layer parse_softmax(list *options, size_params params)
{
    int groups = option_find_int_quiet(options, "groups",1);
    layer l = make_softmax_layer(params.batch, params.inputs, groups);
    l.temperature = option_find_float_quiet(options, "temperature", 1);
    char *tree_file = option_find_str(options, "tree", 0);
    if (tree_file) l.softmax_tree = read_tree(tree_file);
    l.w = params.w;
    l.h = params.h;
    l.c = params.c;
    l.spatial = option_find_float_quiet(options, "spatial", 0);
    l.noloss =  option_find_int_quiet(options, "noloss", 0);
    l.workspace_size = 0;
    return l;
}

int *parse_yolo_mask(char *a, int *num)
{
    int *mask = 0;
    if(a){
        int len = strlen(a);
        int n = 1;
        int i;
        for(i = 0; i < len; ++i){
            if (a[i] == ',') ++n;
        }
        mask = calloc(n, sizeof(int));
        for(i = 0; i < n; ++i){
            int val = atoi(a);
            mask[i] = val;
            a = strchr(a, ',')+1;
        }
        *num = n;
    }
    return mask;
}

layer parse_yolo(list *options, size_params params)
{
    int classes = option_find_int(options, "classes", 20);
    int total = option_find_int(options, "num", 1);
    int num = total;

    char *a = option_find_str(options, "mask", 0);
    int *mask = parse_yolo_mask(a, &num);
    layer l = make_yolo_layer(params.batch, params.w, params.h, num, total, mask, classes);
    assert(l.outputs == params.inputs);

    l.max_boxes = option_find_int_quiet(options, "max",90);
    l.jitter = option_find_float(options, "jitter", .2);

    l.ignore_thresh = option_find_float(options, "ignore_thresh", .5);
    l.truth_thresh = option_find_float(options, "truth_thresh", 1);
    l.random = option_find_int_quiet(options, "random", 0);

    char *map_file = option_find_str(options, "map", 0);
    if (map_file) l.map = read_map(map_file);

    a = option_find_str(options, "anchors", 0);
    if(a){
        int len = (int) strlen(a);
        int n = 1;
        int i;
        for(i = 0; i < len; ++i){
            if (a[i] == ',') ++n;
        }
        for(i = 0; i < n; ++i){
            float bias = (float) atof(a);
            l.biases[i] = bias;
            a = strchr(a, ',')+1;
        }
    }
    return l;
}

int *parse_yolo4_mask(char *a, int *num)
{
    int *mask = 0;
    if(a){
        int len = (int) strlen(a);
        int n = 1;
        int i;
        for(i = 0; i < len; ++i){
            if (a[i] == ',') ++n;
        }
        mask = calloc(n, sizeof(int));
        for(i = 0; i < n; ++i){
            int val = atoi(a);
            mask[i] = val;
            a = strchr(a, ',')+1;
        }
        *num = n;
    }
    return mask;
}

float *get_classes_multipliers_y4(char *cpc, const int classes, const float max_delta)
{
    float *classes_multipliers = NULL;
    if (cpc) {
        int classes_counters = classes;
        int *counters_per_class = parse_yolo4_mask(cpc, &classes_counters);
        if (classes_counters != classes) {
            printf(" number of values in counters_per_class = %d doesn't match with classes = %d \n", classes_counters, classes);
            exit(0);
        }
        float max_counter = 0;
        int i;
        for (i = 0; i < classes_counters; ++i) {
            if (counters_per_class[i] < 1) counters_per_class[i] = 1;
            if (max_counter < counters_per_class[i]) max_counter = counters_per_class[i];
        }
        classes_multipliers = (float *)calloc(classes_counters, sizeof(float));
        for (i = 0; i < classes_counters; ++i) {
            classes_multipliers[i] = max_counter / counters_per_class[i];
            if(classes_multipliers[i] > max_delta) classes_multipliers[i] = max_delta;
        }
        free(counters_per_class);
        printf(" classes_multipliers: ");
        for (i = 0; i < classes_counters; ++i) printf("%.1f, ", classes_multipliers[i]);
        printf("\n");
    }
    return classes_multipliers;
}

float *get_classes_multipliers(char *cpc, const int classes, const float max_delta)
{
    float *classes_multipliers = NULL;
    if (cpc) {
        int classes_counters = classes;
        int *counters_per_class = parse_yolo_mask(cpc, &classes_counters);
        if (classes_counters != classes) {
            printf(" number of values in counters_per_class = %d doesn't match with classes = %d \n", classes_counters, classes);
            exit(0);
        }
        float max_counter = 0;
        int i;
        for (i = 0; i < classes_counters; ++i) {
            if (counters_per_class[i] < 1) counters_per_class[i] = 1;
            if (max_counter < counters_per_class[i]) max_counter = counters_per_class[i];
        }
        classes_multipliers = (float *)calloc(classes_counters, sizeof(float));
        for (i = 0; i < classes_counters; ++i) {
            classes_multipliers[i] = max_counter / counters_per_class[i];
            if(classes_multipliers[i] > max_delta) classes_multipliers[i] = max_delta;
        }
        free(counters_per_class);
        printf(" classes_multipliers: ");
        for (i = 0; i < classes_counters; ++i) printf("%.1f, ", classes_multipliers[i]);
        printf("\n");
    }
    return classes_multipliers;
}

layer parse_yolo4(list *options, size_params params,int is_yolo4)
{
    int classes = option_find_int(options, "classes", 20);
    int total = option_find_int(options, "num", 1);
    int num = total;
    char *a = option_find_str(options, "mask", 0);
    int *mask = parse_yolo4_mask(a, &num);
    int max_boxes = option_find_int_quiet(options, "max", 200);
    layer l = make_yolo4_layer(params.batch, params.w, params.h, num, total, mask, classes, max_boxes,is_yolo4);
    if (l.outputs != params.inputs) {
        printf("Error: l.outputs == params.inputs \n");
        printf("filters= in the [convolutional]-layer doesn't correspond to classes= or mask= in [yolo4]-layer \n");
        exit(EXIT_FAILURE);
    }
    //assert(l.outputs == params.inputs);

    l.show_details = option_find_int_quiet(options, "show_details", 1);
    l.max_delta = option_find_float_quiet(options, "max_delta", FLT_MAX);   // set 10
    char *cpc = option_find_str(options, "counters_per_class", 0);
    l.classes_multipliers = get_classes_multipliers_y4(cpc, classes, l.max_delta);

    l.label_smooth_eps = option_find_float_quiet(options, "label_smooth_eps", 0.0f);
    l.scale_x_y = option_find_float_quiet(options, "scale_x_y", 1);
    l.objectness_smooth = option_find_int_quiet(options, "objectness_smooth", 0);
    l.new_coords = option_find_int_quiet(options, "new_coords", 0);
    l.iou_normalizer = option_find_float_quiet(options, "iou_normalizer", 0.75f);
    l.obj_normalizer = option_find_float_quiet(options, "obj_normalizer", 1);
    l.cls_normalizer = option_find_float_quiet(options, "cls_normalizer", 1);
    l.delta_normalizer = option_find_float_quiet(options, "delta_normalizer", 1);
    char *iou_loss = option_find_str_quiet(options, "iou_loss", "mse");   //  "iou");

    if (strcmp(iou_loss, "mse") == 0) l.iou_loss = MSE;
    else if (strcmp(iou_loss, "giou") == 0) l.iou_loss = GIOU;
    else if (strcmp(iou_loss, "diou") == 0) l.iou_loss = DIOU;
    else if (strcmp(iou_loss, "ciou") == 0) l.iou_loss = CIOU;
    else l.iou_loss = IOU;
    fprintf(stderr, "[yolo4] params: iou loss: %s (%d), iou_norm: %2.2f, obj_norm: %2.2f, cls_norm: %2.2f, delta_norm: %2.2f, scale_x_y: %2.2f\n",
            iou_loss, l.iou_loss, l.iou_normalizer, l.obj_normalizer, l.cls_normalizer, l.delta_normalizer, l.scale_x_y);

    char *iou_thresh_kind_str = option_find_str_quiet(options, "iou_thresh_kind", "iou");
    if (strcmp(iou_thresh_kind_str, "iou") == 0) l.iou_thresh_kind = IOU;
    else if (strcmp(iou_thresh_kind_str, "giou") == 0) l.iou_thresh_kind = GIOU;
    else if (strcmp(iou_thresh_kind_str, "diou") == 0) l.iou_thresh_kind = DIOU;
    else if (strcmp(iou_thresh_kind_str, "ciou") == 0) l.iou_thresh_kind = CIOU;
    else {
        fprintf(stderr, " Wrong iou_thresh_kind = %s \n", iou_thresh_kind_str);
        l.iou_thresh_kind = IOU;
    }

    l.beta_nms = option_find_float_quiet(options, "beta_nms", 0.6f);
    char *nms_kind = option_find_str_quiet(options, "nms_kind", "default");
    if (strcmp(nms_kind, "default") == 0) l.nms_kind = DEFAULT_NMS;
    else {
        if (strcmp(nms_kind, "greedynms") == 0) l.nms_kind = GREEDY_NMS;
        else if (strcmp(nms_kind, "diounms") == 0) l.nms_kind = DIOU_NMS;
        else l.nms_kind = DEFAULT_NMS;
        printf("nms_kind: %s (%d), beta = %f \n", nms_kind, l.nms_kind, l.beta_nms);
    }

    l.jitter = option_find_float(options, "jitter", .2f);
    l.resize = option_find_float_quiet(options, "resize", 1.0f);
    l.focal_loss = option_find_int_quiet(options, "focal_loss", 0);

    l.ignore_thresh = option_find_float(options, "ignore_thresh", .5f);
    l.truth_thresh = option_find_float(options, "truth_thresh", 1);
    l.iou_thresh = option_find_float_quiet(options, "iou_thresh", 1); // recommended to use iou_thresh=0.213 in [yolo4]
    l.random = option_find_float_quiet(options, "random", 0);

    l.track_history_size = option_find_int_quiet(options, "track_history_size", 5);
    l.sim_thresh = option_find_float_quiet(options, "sim_thresh", 0.8f);
    l.dets_for_track = option_find_int_quiet(options, "dets_for_track", 1);
    l.dets_for_show = option_find_int_quiet(options, "dets_for_show", 1);
    l.track_ciou_norm = option_find_float_quiet(options, "track_ciou_norm", 0.01f);
    int embedding_layer_id = option_find_int_quiet(options, "embedding_layer", 999999);
    if (embedding_layer_id < 0) embedding_layer_id = params.index + embedding_layer_id;
    if (embedding_layer_id != 999999) {
        printf(" embedding_layer_id = %d, ", embedding_layer_id);
        layer le = params.net->layers[embedding_layer_id];
        l.embedding_layer_id = embedding_layer_id;
        l.embedding_output = (float*)calloc(le.batch * le.outputs, sizeof(float));
        l.embedding_size = le.n / l.n;
        printf(" embedding_size = %d \n", l.embedding_size);
        if (le.n % l.n != 0) {
            printf(" Warning: filters=%d number in embedding_layer=%d isn't divisible by number of anchors %d \n", le.n, embedding_layer_id, l.n);
            getchar();
        }
    }

    char *map_file = option_find_str(options, "map", 0);
    if (map_file) l.map = read_map(map_file);

    a = option_find_str(options, "anchors", 0);
    if (a) {
        int len = strlen(a);
        int n = 1;
        int i;
        for (i = 0; i < len; ++i) {
            if (a[i] == '#') break;
            if (a[i] == ',') ++n;
        }
        for (i = 0; i < n && i < total*2; ++i) {
            float bias = atof(a);
            l.biases[i] = bias;
            a = strchr(a, ',') + 1;
        }
    }
    return l;
}

detection_layer parse_detection(list *options, size_params params)
{
    int coords = option_find_int(options, "coords", 1);
    int classes = option_find_int(options, "classes", 1);
    int rescore = option_find_int(options, "rescore", 0);
    int num = option_find_int(options, "num", 1);
    int side = option_find_int(options, "side", 7);
    detection_layer layer = make_detection_layer(params.batch, params.inputs, num, side, classes, coords, rescore);

    layer.sqrt = option_find_int(options, "sqrt", 0);

    layer.max_boxes = option_find_int_quiet(options, "max",90);
    layer.coord_scale = option_find_float(options, "coord_scale", 1);
    layer.forced = option_find_int(options, "forced", 0);
    layer.object_scale = option_find_float(options, "object_scale", 1);
    layer.noobject_scale = option_find_float(options, "noobject_scale", 1);
    layer.class_scale = option_find_float(options, "class_scale", 1);
    layer.jitter = option_find_float(options, "jitter", .2);
    layer.random = option_find_int_quiet(options, "random", 0);
    return layer;
}

cost_layer parse_cost(list *options, size_params params)
{
    char *type_s = option_find_str(options, "type", "sse");
    COST_TYPE type = get_cost_type(type_s);
    float scale = option_find_float_quiet(options, "scale",1);
    cost_layer layer = make_cost_layer(params.batch, params.inputs, type, scale);
    layer.ratio =  option_find_float_quiet(options, "ratio",0);
    layer.noobject_scale =  option_find_float_quiet(options, "noobj", 1);
    layer.thresh =  option_find_float_quiet(options, "thresh",0);
    return layer;
}

maxpool_layer parse_maxpool(list *options, size_params params)
{
    int stride = option_find_int(options, "stride",1);
    int size = option_find_int(options, "size",stride);
    int padding = option_find_int_quiet(options, "padding", size-1);

    int batch,h,w,c;
    h = params.h;
    w = params.w;
    c = params.c;
    batch=params.batch;
    if(!(h && w && c)) error("Layer before maxpool layer must output image.");

    maxpool_layer layer = make_maxpool_layer(batch,h,w,c,size,stride,padding);
    return layer;
}

avgpool_layer parse_avgpool(list *options, size_params params)
{
    int batch,w,h,c;
    w = params.w;
    h = params.h;
    c = params.c;
    batch=params.batch;
    if(!(h && w && c)) error("Layer before avgpool layer must output image.");

    avgpool_layer layer = make_avgpool_layer(batch,w,h,c);
    return layer;
}

layer parse_normalization(list *options, size_params params)
{
    float alpha = option_find_float(options, "alpha", .0001);
    float beta =  option_find_float(options, "beta" , .75);
    float kappa = option_find_float(options, "kappa", 1);
    int size = option_find_int(options, "size", 5);
    layer l = make_normalization_layer(params.batch, params.w, params.h, params.c, size, alpha, beta, kappa);
    return l;
}

layer parse_batchnorm(list *options, size_params params)
{
    layer l = make_batchnorm_layer(params.batch, params.w, params.h, params.c);
    return l;
}

layer parse_shortcut(list *options, size_params params, network *net)
{
    char *l = option_find(options, "from");
    int index = atoi(l);
    if(index < 0) index = params.index + index;

    int batch = params.batch;
    layer from = net->layers[index];

    layer s = make_shortcut_layer(batch, index, params.w, params.h, params.c, from.out_w, from.out_h, from.out_c);

    char *activation_s = option_find_str(options, "activation", "linear");
    ACTIVATION activation = get_activation(activation_s);
    s.activation = activation;
    s.alpha = option_find_float_quiet(options, "alpha", 1);
    s.beta = option_find_float_quiet(options, "beta", 1);
    return s;
}


layer parse_logistic(list *options, size_params params)
{
    layer l = make_logistic_layer(params.batch, params.inputs);
    l.h = l.out_h = params.h;
    l.w = l.out_w = params.w;
    l.c = l.out_c = params.c;
    return l;
}

layer parse_activation(list *options, size_params params)
{
    char *activation_s = option_find_str(options, "activation", "linear");
    ACTIVATION activation = get_activation(activation_s);

    layer l = make_activation_layer(params.batch, params.inputs, activation);

    l.h = l.out_h = params.h;
    l.w = l.out_w = params.w;
    l.c = l.out_c = params.c;

    return l;
}

layer parse_upsample(list *options, size_params params, network *net)
{

    int stride = option_find_int(options, "stride",2);
    layer l = make_upsample_layer(params.batch, params.w, params.h, params.c, stride);
    l.scale = option_find_float_quiet(options, "scale", 1);
    return l;
}

route_layer parse_route(list *options, size_params params, network *net)
{
    params.net = net;
    char *l = option_find(options, "layers");
    if(!l) error("Route Layer must specify input layers");
    int len = strlen(l);
    int n = 1;
    int i;
    for(i = 0; i < len; ++i){
        if (l[i] == ',') ++n;
    }

    int* layers = (int*)calloc(n, sizeof(int));
    int* sizes = (int*)calloc(n, sizeof(int));
    for(i = 0; i < n; ++i){
        int index = atoi(l);
        l = strchr(l, ',')+1;
        if(index < 0) index = params.index + index;
        layers[i] = index;
        sizes[i] = params.net->layers[index].outputs;
    }
    int batch = params.batch;

    int groups = option_find_int_quiet(options, "groups", 1);
    int group_id = option_find_int_quiet(options, "group_id", 0);

    route_layer layer = make_route_layer(batch, n, layers, sizes, groups, group_id);

    convolutional_layer first = params.net->layers[layers[0]];
    layer.out_w = first.out_w;
    layer.out_h = first.out_h;
    layer.out_c = first.out_c;
    for(i = 1; i < n; ++i){
        int index = layers[i];
        convolutional_layer next = params.net->layers[index];
        if(next.out_w == first.out_w && next.out_h == first.out_h){
            layer.out_c += next.out_c;
        }else{
            fprintf(stderr, " The width and height of the input layers are different. \n");
            layer.out_h = layer.out_w = layer.out_c = 0;
        }
    }
    layer.out_c = layer.out_c / layer.groups;

    layer.w = first.w;
    layer.h = first.h;
    layer.c = layer.out_c;

    if (n > 3) fprintf(stderr, " \t    ");
    else if (n > 1) fprintf(stderr, " \t            ");
    else fprintf(stderr, " \t\t            ");

    fprintf(stderr, "           ");
    if (layer.groups > 1) fprintf(stderr, "%d/%d", layer.group_id, layer.groups);
    else fprintf(stderr, "   ");
    fprintf(stderr, " -> %4d x%4d x%4d \n", layer.out_w, layer.out_h, layer.out_c);

    return layer;
}

learning_rate_policy get_policy(char *s)
{
    if (strcmp(s, "random")==0) return RANDOM;
    if (strcmp(s, "poly")==0) return POLY;
    if (strcmp(s, "constant")==0) return CONSTANT;
    if (strcmp(s, "step")==0) return STEP;
    if (strcmp(s, "exp")==0) return EXP;
    if (strcmp(s, "sigmoid")==0) return SIG;
    if (strcmp(s, "steps")==0) return STEPS;
    if (strcmp(s, "sgdr")==0) return SGDR;
    fprintf(stderr, "Couldn't find policy %s, going with constant\n", s);
    return CONSTANT;
}

void parse_net_options(list *options, network *net)
{
    net->max_batches = option_find_int(options, "max_batches", 0);
    net->batch = option_find_int(options, "batch",1);
    net->learning_rate = option_find_float(options, "learning_rate", .001);
    net->learning_rate_min = option_find_float_quiet(options, "learning_rate_min", .00001);
    net->batches_per_cycle = option_find_int_quiet(options, "sgdr_cycle", net->max_batches);
    net->batches_cycle_mult = option_find_int_quiet(options, "sgdr_mult", 2);
    net->momentum = option_find_float(options, "momentum", .9);
    net->decay = option_find_float(options, "decay", .0001);
    int subdivs = option_find_int(options, "subdivisions",1);
    net->time_steps = option_find_int_quiet(options, "time_steps",1);
    net->notruth = option_find_int_quiet(options, "notruth",0);
    net->track = option_find_int_quiet(options, "track", 0);
    net->augment_speed = option_find_int_quiet(options, "augment_speed", 2);
    net->init_sequential_subdivisions = net->sequential_subdivisions = option_find_int_quiet(options, "sequential_subdivisions", subdivs);
    if (net->sequential_subdivisions > subdivs) net->init_sequential_subdivisions = net->sequential_subdivisions = subdivs;
    net->try_fix_nan = option_find_int_quiet(options, "try_fix_nan", 0);
    net->batch /= subdivs;          // mini_batch
    const int mini_batch = net->batch;
    net->batch *= net->time_steps;  // mini_batch * time_steps
    net->subdivisions = subdivs;    // number of mini_batches
    net->random = option_find_int_quiet(options, "random", 0);
    *net->seen = 0;
    *net->cur_iteration = 0;
    net->loss_scale = option_find_float_quiet(options, "loss_scale", 1);
    net->dynamic_minibatch = option_find_int_quiet(options, "dynamic_minibatch", 0);
    net->optimized_memory = option_find_int_quiet(options, "optimized_memory", 0);
    net->workspace_size_limit = (size_t)1024*1024 * option_find_float_quiet(options, "workspace_size_limit_MB", 1024);  // 1024 MB by default

    net->adam = option_find_int_quiet(options, "adam", 0);
    if(net->adam){
        net->B1 = option_find_float(options, "B1", .9);
        net->B2 = option_find_float(options, "B2", .999);
        net->eps = option_find_float(options, "eps", .0000001);
    }

    net->h = option_find_int_quiet(options, "height",0);
    net->w = option_find_int_quiet(options, "width",0);
    net->c = option_find_int_quiet(options, "channels",0);
    net->inputs = option_find_int_quiet(options, "inputs", net->h * net->w * net->c);
    net->max_crop = option_find_int_quiet(options, "max_crop",net->w*2);
    net->min_crop = option_find_int_quiet(options, "min_crop",net->w);
    net->flip = option_find_int_quiet(options, "flip", 1);
    net->blur = option_find_int_quiet(options, "blur", 0);
    net->gaussian_noise = option_find_int_quiet(options, "gaussian_noise", 0);
    net->mixup = option_find_int_quiet(options, "mixup", 0);
    int cutmix = option_find_int_quiet(options, "cutmix", 0);
    int mosaic = option_find_int_quiet(options, "mosaic", 0);
    if (mosaic && cutmix) net->mixup = 4;
    else if (cutmix) net->mixup = 2;
    else if (mosaic) net->mixup = 3;
	net->max_ratio = option_find_float_quiet(options, "max_ratio", (float) net->max_crop / net->w);
	net->min_ratio = option_find_float_quiet(options, "min_ratio", (float) net->min_crop / net->w);
	net->center = option_find_int_quiet(options, "center",0);
	net->clip = option_find_float_quiet(options, "clip", 0);
    net->letter_box = option_find_int_quiet(options, "letter_box", 0);
    net->mosaic_bound = option_find_int_quiet(options, "mosaic_bound", 0);
    net->contrastive = option_find_int_quiet(options, "contrastive", 0);
    net->contrastive_jit_flip = option_find_int_quiet(options, "contrastive_jit_flip", 0);
    net->contrastive_color = option_find_int_quiet(options, "contrastive_color", 0);
    net->unsupervised = option_find_int_quiet(options, "unsupervised", 0);
    if (net->contrastive && mini_batch < 2) {
        printf(" Error: mini_batch size (batch/subdivisions) should be higher than 1 for Contrastive loss \n");
        exit(0);
    }
    net->label_smooth_eps = option_find_float_quiet(options, "label_smooth_eps", 0.0f);
    net->resize_step = option_find_float_quiet(options, "resize_step", 32);
    net->attention = option_find_int_quiet(options, "attention", 0);
    net->adversarial_lr = option_find_float_quiet(options, "adversarial_lr", 0);
    net->max_chart_loss = option_find_float_quiet(options, "max_chart_loss", 20.0);

    net->angle = option_find_float_quiet(options, "angle", 0);
    net->aspect = option_find_float_quiet(options, "aspect", 1);
    net->saturation = option_find_float_quiet(options, "saturation", 1);
    net->exposure = option_find_float_quiet(options, "exposure", 1);
    net->hue = option_find_float_quiet(options, "hue", 0);

    if(!net->inputs && !(net->h && net->w && net->c)) error("No input parameters supplied");

    char *policy_s = option_find_str(options, "policy", "constant");
    net->policy = get_policy(policy_s);
    net->burn_in = option_find_int_quiet(options, "burn_in", 0);
    net->power = option_find_float_quiet(options, "power", 4);
    if(net->policy == STEP){
        net->step = option_find_int(options, "step", 1);
        net->scale = option_find_float(options, "scale", 1);
    } else if (net->policy == STEPS || net->policy == SGDR){
        char *l = option_find(options, "steps");
        char *p = option_find(options, "scales");
        char *s = option_find(options, "seq_scales");
        if(net->policy == STEPS && (!l || !p)) error("STEPS policy must have steps and scales in cfg file");

        if (l) {
            int len = strlen(l);
            int n = 1;
            int i;
            for (i = 0; i < len; ++i) {
                if (l[i] == '#') break;
                if (l[i] == ',') ++n;
            }
            int* steps = (int*)calloc(n, sizeof(int));
            float* scales = (float*)calloc(n, sizeof(float));
            float* seq_scales = (float*)calloc(n, sizeof(float));
            for (i = 0; i < n; ++i) {
                float scale = 1.0;
                if (p) {
                    scale = atof(p);
                    p = strchr(p, ',') + 1;
                }
                float sequence_scale = 1.0;
                if (s) {
                    sequence_scale = atof(s);
                    s = strchr(s, ',') + 1;
                }
                int step = atoi(l);
                l = strchr(l, ',') + 1;
                steps[i] = step;
                scales[i] = scale;
                seq_scales[i] = sequence_scale;
            }
            net->scales = scales;
            net->steps = steps;
            net->seq_scales = seq_scales;
            net->num_steps = n;
        }
    } else if (net->policy == EXP){
        net->gamma = option_find_float(options, "gamma", 1);
    } else if (net->policy == SIG){
        net->gamma = option_find_float(options, "gamma", 1);
        net->step = option_find_int(options, "step", 1);
    } else if (net->policy == POLY || net->policy == RANDOM){
        //net->power = option_find_float(options, "power", 1);
    }
    net->max_batches = option_find_int(options, "max_batches", 0);
}

int is_network(section *s)
{
    return (strcmp(s->type, "[net]")==0
            || strcmp(s->type, "[network]")==0);
}

network *parse_network_cfg(char *filename)
{
    list *sections = read_cfg(filename);
    node *n = sections->front;
    if(!n) error("Config file has no sections");
    network *net = make_network(sections->size - 1);
#if GPU
    if (gpu_index >= 0) {
        net->gpu_index = opencl_device_id_t;
    }
#endif
    size_params params;

    section *s = (section *)n->val;
    list *options = s->options;
    if(!is_network(s)) error("First section must be [net] or [network]");
    parse_net_options(options, net);

    params.h = net->h;
    params.w = net->w;
    params.c = net->c;
    params.inputs = net->inputs;
    params.batch = net->batch;
    params.time_steps = net->time_steps;
    params.net = net;

    size_t workspace_size = 0;
    n = n->next;
    int count = 0;
    free_section(s);
    fprintf(stderr, "layer     filters    size              input                output\n");
    while(n){
        params.index = count;
        fprintf(stderr, "%5d ", count);
        s = (section *)n->val;
        options = s->options;
        layer l = {0};
        LAYER_TYPE lt = string_to_layer_type(s->type);
        if(lt == CONVOLUTIONAL){
            l = parse_convolutional(options, params);
        }else if(lt == LOCAL){
            l = parse_local(options, params);
        }else if(lt == ACTIVE){
            l = parse_activation(options, params);
        }else if(lt == LOGXENT){
            l = parse_logistic(options, params);
        }else if(lt == COST){
            l = parse_cost(options, params);
        }else if(lt == YOLO){
            l = parse_yolo(options, params);
        }else if (lt == YOLO4) {
            l = parse_yolo4(options, params,1);
            l.keep_delta_gpu = 1;
        }else if(lt == YOLO7){
             l = parse_yolo4(options, params,0);
            l.keep_delta_gpu = 1;
        }
        else if(lt == DETECTION){
            l = parse_detection(options, params);
        }else if(lt == NORMALIZATION){
            l = parse_normalization(options, params);
        }else if(lt == BATCHNORM){
            l = parse_batchnorm(options, params);
        }else if(lt == MAXPOOL){
            l = parse_maxpool(options, params);
        }else if(lt == ROUTE){
            l = parse_route(options, params, net);
        }else if(lt == UPSAMPLE){
            l = parse_upsample(options, params, net);
        }else if(lt == SHORTCUT){
            l = parse_shortcut(options, params, net);
        }else if(lt == SOFTMAX){
            l = parse_softmax(options, params);
        }else if(lt == AVGPOOL){
            l = parse_avgpool(options, params);
        }else{
            fprintf(stderr, "Type not recognized: %s\n", s->type);
        }
        l.clip = net->clip;
        l.truth = option_find_int_quiet(options, "truth", 0);
        l.onlyforward = option_find_int_quiet(options, "onlyforward", 0);
        l.stopbackward = option_find_int_quiet(options, "stopbackward", 0);
        l.dontsave = option_find_int_quiet(options, "dontsave", 0);
        l.dontload = option_find_int_quiet(options, "dontload", 0);
        l.numload = option_find_int_quiet(options, "numload", 0);
        l.dontloadscales = option_find_int_quiet(options, "dontloadscales", 0);
        l.learning_rate_scale = option_find_float_quiet(options, "learning_rate", 1);
        l.smooth = option_find_float_quiet(options, "smooth", 0);
        option_unused(options);
        net->layers[count] = l;
        if (l.workspace_size > workspace_size) workspace_size = l.workspace_size;
        free_section(s);
        n = n->next;
        ++count;
        if(n){
            params.h = l.out_h;
            params.w = l.out_w;
            params.c = l.out_c;
            params.inputs = l.outputs;
        }
    }
    free_list(sections);
    layer out = get_network_output_layer(net);
    net->outputs = out.outputs;
    net->truths = out.outputs;
    if(net->layers[net->n-1].truths) net->truths = net->layers[net->n-1].truths;
    net->output = out.output;
    net->input = calloc(net->inputs*net->batch, sizeof(float));
    net->truth = calloc(net->truths*net->batch, sizeof(float));
    //TODO: CHECK! (1)
    //net->delta = calloc(net->outputs*net->batch, sizeof(float));
#ifdef GPU
    if (gpu_index >= 0) {
        net->output_gpu = out.output_gpu;
        net->input_gpu = opencl_make_array(net->input, net->inputs * net->batch);
        net->truth_gpu = opencl_make_array(net->truth, net->truths * net->batch);
        //TODO: CHECK! (1)
        //net->delta_gpu = opencl_make_array(net->delta, net->outputs * net->batch);
    }
#endif
    if(workspace_size){
        //printf("%ld\n", workspace_size);
#ifdef GPU
        if(gpu_index >= 0){
            net->workspace = calloc(workspace_size, sizeof(float));
            net->workspace_gpu = opencl_make_array(net->workspace, workspace_size);
        }else {
            net->workspace = calloc(workspace_size, sizeof(float));
        }
#else
        net->workspace = calloc(workspace_size, sizeof(float));
#endif
    }
    return net;
}

void set_train_only_bn(network net)
{
    int train_only_bn = 0;
    int i;
    for (i = net.n - 1; i >= 0; --i) {
        if (net.layers[i].train_only_bn) train_only_bn = net.layers[i].train_only_bn;  // set l.train_only_bn for all previous layers
        if (train_only_bn) {
            net.layers[i].train_only_bn = train_only_bn;
        }
    }
}

network parse_network_cfg_custom(char *filename, int batch, int time_steps)
{
    list *sections = read_cfg(filename);
    node *n = sections->front;
    if(!n) error("Config file has no sections");
    network *netp = make_network(sections->size - 1);
    network net = *netp;
    net.gpu_index = gpu_index;
    size_params params;

    if (batch > 0) params.train = 0;    // allocates memory for Inference only
    else params.train = 1;              // allocates memory for Inference & Training

    section *s = (section *)n->val;
    list *options = s->options;
    if(!is_network(s)) error("First section must be [net] or [network]");
    parse_net_options(options, &net);

    params.h = net.h;
    params.w = net.w;
    params.c = net.c;
    params.inputs = net.inputs;
    if (batch > 0) net.batch = batch;
    if (time_steps > 0) net.time_steps = time_steps;
    if (net.batch < 1) net.batch = 1;
    if (net.time_steps < 1) net.time_steps = 1;
    if (net.batch < net.time_steps) net.batch = net.time_steps;
    params.batch = net.batch;
    params.time_steps = net.time_steps;
    params.net = &net;
    printf("mini_batch = %d, batch = %d, time_steps = %d, train = %d \n", net.batch, net.batch * net.subdivisions, net.time_steps, params.train);

    int last_stop_backward = -1;
    int avg_outputs = 0;
    int avg_counter = 0;
    float bflops = 0;
    size_t workspace_size = 0;
    size_t max_inputs = 0;
    size_t max_outputs = 0;
    int receptive_w = 1, receptive_h = 1;
    int receptive_w_scale = 1, receptive_h_scale = 1;
    const int show_receptive_field = option_find_float_quiet(options, "show_receptive_field", 0);

    n = n->next;
    int count = 0;
    free_section(s);

    // find l.stopbackward = option_find_int_quiet(options, "stopbackward", 0);
    node *n_tmp = n;
    int count_tmp = 0;
    if (params.train == 1) {
        while (n_tmp) {
            s = (section *)n_tmp->val;
            options = s->options;
            int stopbackward = option_find_int_quiet(options, "stopbackward", 0);
            if (stopbackward == 1) {
                last_stop_backward = count_tmp;
                printf("last_stop_backward = %d \n", last_stop_backward);
            }
            n_tmp = n_tmp->next;
            ++count_tmp;
        }
    }

    int old_params_train = params.train;

    fprintf(stderr, "   layer   filters  size/strd(dil)      input                output\n");
    while(n){

        params.train = old_params_train;
        if (count < last_stop_backward) params.train = 0;

        params.index = count;
        fprintf(stderr, "%4d ", count);
        s = (section *)n->val;
        options = s->options;
        layer l = { (LAYER_TYPE)0 };
        LAYER_TYPE lt = string_to_layer_type(s->type);
        if(lt == CONVOLUTIONAL){
            l = parse_convolutional(options, params);
        }else if(lt == LOCAL){
            l = parse_local(options, params);
        }else if(lt == ACTIVE){
            l = parse_activation(options, params);
        }else if(lt == COST){
            l = parse_cost(options, params);
            l.keep_delta_gpu = 1;
        }else if (lt == YOLO) {
            l = parse_yolo(options, params);
            l.keep_delta_gpu = 1;
        }else if(lt == DETECTION){
            l = parse_detection(options, params);
        }else if(lt == NORMALIZATION){
            l = parse_normalization(options, params);
        }else if(lt == BATCHNORM){
            l = parse_batchnorm(options, params);
        }else if(lt == MAXPOOL){
            l = parse_maxpool(options, params);
        }else if(lt == ROUTE){
            l = parse_route(options, params, netp);
            int k;
            for (k = 0; k < l.n; ++k) {
                net.layers[l.input_layers[k]].use_bin_output = 0;
                if (count >= last_stop_backward)
                    net.layers[l.input_layers[k]].keep_delta_gpu = 1;
            }
        }else if (lt == UPSAMPLE) {
            l = parse_upsample(options, params, netp);
        }else if(lt == SHORTCUT){
            l = parse_shortcut(options, params, netp);
            net.layers[count - 1].use_bin_output = 0;
            net.layers[l.index].use_bin_output = 0;
            if (count >= last_stop_backward)
                net.layers[l.index].keep_delta_gpu = 1;
        }
        else if (lt == EMPTY) {
            layer empty_layer = {(LAYER_TYPE)0};
            l = empty_layer;
            l.type = EMPTY;
            l.w = l.out_w = params.w;
            l.h = l.out_h = params.h;
            l.c = l.out_c = params.c;
            l.batch = params.batch;
            l.inputs = l.outputs = params.inputs;
            l.output = net.layers[count - 1].output;
            l.delta = net.layers[count - 1].delta;
            l.forward = 0;
            l.backward = 0;
#ifdef GPU
            l.output_gpu = net.layers[count - 1].output_gpu;
            l.delta_gpu = net.layers[count - 1].delta_gpu;
            l.keep_delta_gpu = 1;
            l.forward_gpu = 0;
            l.backward_gpu = 0;
#endif
            fprintf(stderr, "empty \n");
        }else{
            fprintf(stderr, "Type not recognized: %s\n", s->type);
        }

        // calculate receptive field
        if(show_receptive_field)
        {
            int dilation = max_val_cmp(1, l.dilation);
            int stride = max_val_cmp(1, l.stride);
            int size = max_val_cmp(1, l.size);

            if (l.type == UPSAMPLE)
            {

                l.receptive_w = receptive_w;
                l.receptive_h = receptive_h;
                l.receptive_w_scale = receptive_w_scale = receptive_w_scale / stride;
                l.receptive_h_scale = receptive_h_scale = receptive_h_scale / stride;

            }
            else {
                if (l.type == ROUTE) {
                    receptive_w = receptive_h = receptive_w_scale = receptive_h_scale = 0;
                    int k;
                    for (k = 0; k < l.n; ++k) {
                        layer route_l = net.layers[l.input_layers[k]];
                        receptive_w = max_val_cmp(receptive_w, route_l.receptive_w);
                        receptive_h = max_val_cmp(receptive_h, route_l.receptive_h);
                        receptive_w_scale = max_val_cmp(receptive_w_scale, route_l.receptive_w_scale);
                        receptive_h_scale = max_val_cmp(receptive_h_scale, route_l.receptive_h_scale);
                    }
                }
                else
                {
                    int increase_receptive = size + (dilation - 1) * 2 - 1;// stride;
                    increase_receptive = max_val_cmp(0, increase_receptive);

                    receptive_w += increase_receptive * receptive_w_scale;
                    receptive_h += increase_receptive * receptive_h_scale;
                    receptive_w_scale *= stride;
                    receptive_h_scale *= stride;
                }

                l.receptive_w = receptive_w;
                l.receptive_h = receptive_h;
                l.receptive_w_scale = receptive_w_scale;
                l.receptive_h_scale = receptive_h_scale;
            }
            //printf(" size = %d, dilation = %d, stride = %d, receptive_w = %d, receptive_w_scale = %d - ", size, dilation, stride, receptive_w, receptive_w_scale);

            int cur_receptive_w = receptive_w;
            int cur_receptive_h = receptive_h;

            fprintf(stderr, "%4d - receptive field: %d x %d \n", count, cur_receptive_w, cur_receptive_h);
        }

#ifdef GPU
        l.optimized_memory = net.optimized_memory;
        if (net.optimized_memory == 1 && params.train) {
            if (l.delta_gpu.ptr) {
                opencl_free(l.delta_gpu);
            }
        } else if (net.optimized_memory >= 2 && params.train)
        {
            if (l.output_gpu.ptr) {
                opencl_free(l.output_gpu);
            }
            if (l.activation_input_gpu.ptr) {
                opencl_free(l.activation_input_gpu);
            }

            if (l.x_gpu.ptr) {
                opencl_free(l.x_gpu);
            }

            if (net.optimized_memory >= 3) {
                if (l.delta_gpu.ptr) {
                    opencl_free(l.delta_gpu);
                }
            }
     }
#endif // GPU

        l.clip = option_find_float_quiet(options, "clip", 0);
        l.dynamic_minibatch = net.dynamic_minibatch;
        l.onlyforward = option_find_int_quiet(options, "onlyforward", 0);
        l.dont_update = option_find_int_quiet(options, "dont_update", 0);
        l.burnin_update = option_find_int_quiet(options, "burnin_update", 0);
        l.stopbackward = option_find_int_quiet(options, "stopbackward", 0);
        l.train_only_bn = option_find_int_quiet(options, "train_only_bn", 0);
        l.dontload = option_find_int_quiet(options, "dontload", 0);
        l.dontloadscales = option_find_int_quiet(options, "dontloadscales", 0);
        l.learning_rate_scale = option_find_float_quiet(options, "learning_rate", 1);
        option_unused(options);

        if (l.stopbackward == 1) printf(" ------- previous layers are frozen ------- \n");

        net.layers[count] = l;
        if (l.workspace_size > workspace_size) workspace_size = l.workspace_size;
        if (l.inputs > max_inputs) max_inputs = l.inputs;
        if (l.outputs > max_outputs) max_outputs = l.outputs;
        free_section(s);
        n = n->next;
        ++count;
        if(n){
            if (l.antialiasing) {
                params.h = l.input_layer->out_h;
                params.w = l.input_layer->out_w;
                params.c = l.input_layer->out_c;
                params.inputs = l.input_layer->outputs;
            }
            else {
                params.h = l.out_h;
                params.w = l.out_w;
                params.c = l.out_c;
                params.inputs = l.outputs;
            }
        }
        if (l.bflops > 0) bflops += l.bflops;

        if (l.w > 1 && l.h > 1) {
            avg_outputs += l.outputs;
            avg_counter++;
        }
    }

    if (last_stop_backward > -1) {
        int k;
        for (k = 0; k < last_stop_backward; ++k) {
            layer l = net.layers[k];
            if (l.keep_delta_gpu) {
                if (!l.delta) {
                    net.layers[k].delta = (float*)calloc(l.outputs*l.batch, sizeof(float));
                }
#ifdef GPU
                if (!l.delta_gpu.ptr) {
                    net.layers[k].delta_gpu = opencl_make_array(l.delta, l.outputs*l.batch);
                }
#endif
            }

            net.layers[k].onlyforward = 1;
            net.layers[k].train = 0;
        }
    }

    free_list(sections);

    set_train_only_bn(net); // set l.train_only_bn for all required layers

    net.outputs = get_network_output_size(net);
    net.output = get_network_output_y4(net);
    avg_outputs = avg_outputs / avg_counter;
    fprintf(stderr, "Total BFLOPS %5.3f \n", bflops);
    fprintf(stderr, "avg_outputs = %d \n", avg_outputs);
#ifdef GPU
    if (gpu_index >= 0)
    {
        int size = get_network_input_size(net) * net.batch;
        net.input_state_gpu = opencl_make_array(0, size);

        // pre-allocate memory for inference on Tensor Cores (fp16)
        *net.max_input16_size = 0;
        *net.max_output16_size = 0;
        if (net.cudnn_half) {
            *net.max_input16_size = max_inputs;
            *net.max_output16_size = max_outputs;
        }
        if (workspace_size) {
            fprintf(stderr, " Allocate additional workspace_size = %1.2f MB \n", (float)workspace_size/1000000);
            net.workspace_gpu = opencl_make_array(0, workspace_size / sizeof(float) + 1);
        }
        else {
            net.workspace = (float*)calloc(1, workspace_size);
        }
    }
#else
    if (workspace_size) {
            net.workspace = (float*)calloc(1, workspace_size);
    }
#endif

    LAYER_TYPE lt = net.layers[net.n - 1].type;
    if ((net.w % 32 != 0 || net.h % 32 != 0) && (lt == YOLO4 || lt == YOLO || lt == DETECTION)) {
        printf("\n Warning: width=%d and height=%d in cfg-file must be divisible by 32 for default networks Yolo v1/v2/v3!!! \n\n",
               net.w, net.h);
    }
    return net;
}

list *read_cfg(char *filename)
{
    char *line=NULL;
    int nu = 0;
    list *options = make_list();
    section *current = 0;

#ifdef SECURITY
    TEE_Ctx_Util *ctx_util=prepare_tee_session();
    TEEC_Operation op;
    uint32_t origin;
    int max_length=1024;
    TEEC_Result res;
    char *buffer=NULL;
    char *origin_name=get_origin_filename(filename);
    do{
        max_length=max_length+1024;
        buffer=realloc(buffer,max_length);
        memset(&op,0,sizeof(op));
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                                            TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
        op.params[0].tmpref.buffer=origin_name;
        op.params[0].tmpref.size=strlen(origin_name);
        op.params[1].tmpref.buffer=buffer;
        op.params[1].tmpref.size=max_length;
        res=TEEC_InvokeCommand(&ctx_util[0].sess,TA_READ_TEE_FILE,&op,&origin);
        
    }while(res==TEEC_ERROR_SHORT_BUFFER);
    
    if(res!=TEEC_SUCCESS){
        free(buffer);
        free(origin_name);
        close_TEE_ctx_util(ctx_util);
        printf("Read file %s filed, res=0x%08x\n",filename,res);
        return options;
    }
    buffer[op.params[1].tmpref.size]='\0';
    close_TEE_ctx_util(ctx_util);
    list *buffer_list=split_str(buffer,'\n');
    char** lines=(char**)list_to_array(buffer_list);
    int line_num=buffer_list->size;
    free_list(buffer_list);
    //free(buffer);
    free(origin_name);
    if(line_num>0){
        line=(char*)malloc(strlen(lines[nu])+1);
        strcpy(line,lines[nu]);
    }
#else
    FILE *file = fopen(filename, "r");
    if(file == 0) file_error(filename);
    line=fgetl(file);
#endif

    while(line){
        ++ nu;
        strip(line);
        switch(line[0]){
            case '[':
                current = malloc(sizeof(section));
                list_insert(options, current);
                current->options = make_list();
                current->type = line;
                break;
            case '\0':
            case '#':
            case ';':
                free(line);
                break;
            default:
                if(!read_option(line, current->options)){
                    fprintf(stderr, "Config file error line %d, could parse: %s\n", nu, line);
                    free(line);
                }
                break;
        }
    #ifdef SECURITY
        if(nu<line_num){
            line=malloc(strlen(lines[nu])+1);
            strcpy(line,lines[nu]);
        }
        else line=NULL;
    #else
        line=fgetl(file);
    #endif
    }
#ifdef SECURITY
    free(buffer);
#else
    fclose(file);
#endif
    return options;
}

void save_convolutional_weights(layer l, FILE *fp)
{

#ifdef GPU
    if(gpu_index >= 0){
        pull_convolutional_layer(l);
    }
#endif
    int num = l.nweights;
    fwrite(l.biases, sizeof(float), l.n, fp);
    if (l.batch_normalize){
        fwrite(l.scales, sizeof(float), l.n, fp);
        fwrite(l.rolling_mean, sizeof(float), l.n, fp);
        fwrite(l.rolling_variance, sizeof(float), l.n, fp);
    }
    fwrite(l.weights, sizeof(float), num, fp);
}

void save_batchnorm_weights(layer l, FILE *fp)
{
#ifdef GPU
    if(gpu_index >= 0){
        pull_batchnorm_layer(l);
    }
#endif
    fwrite(l.scales, sizeof(float), l.c, fp);
    fwrite(l.rolling_mean, sizeof(float), l.c, fp);
    fwrite(l.rolling_variance, sizeof(float), l.c, fp);
}


void save_weights_upto(network *net, char *filename, int cutoff)
{
#ifdef GPU
    if(gpu_index >= 0){
        opencl_set_device(net->gpu_index);
    }
#endif
#if !defined(BENCHMARK) && !defined(LOSS_ONLY)
    fprintf(stderr, "Saving weights to %s\n", filename);
#endif
    FILE *fp = fopen(filename, "wb");
    if(!fp) file_error(filename);

    int major = 0;
    int minor = 2;
    int revision = 0;
    fwrite(&major, sizeof(int), 1, fp);
    fwrite(&minor, sizeof(int), 1, fp);
    fwrite(&revision, sizeof(int), 1, fp);
//#ifdef ARM
//    fwrite(net->seen, sizeof(unsigned long long), 1, fp); // 64-bit on ILP32 and LP64.
//#else
    fwrite(net->seen, sizeof(size_t), 1, fp);
//#endif

    int i;
    for(i = 0; i < net->n && i < cutoff; ++i){
        layer l = net->layers[i];
        if (l.dontsave) continue;
        if(l.type == CONVOLUTIONAL){
            save_convolutional_weights(l, fp);
        } if(l.type == BATCHNORM){
            save_batchnorm_weights(l, fp);
        } if(l.type == LOCAL){
#ifdef GPU
            if(gpu_index >= 0){
                pull_local_layer(l);
            }
#endif
            int locations = l.out_w*l.out_h;
            int size = l.size*l.size*l.c*l.n*locations;
            fwrite(l.biases, sizeof(float), l.outputs, fp);
            fwrite(l.weights, sizeof(float), size, fp);
        }
    }
    fclose(fp);
}
void save_weights(network *net, char *filename)
{
    save_weights_upto(net, filename, net->n);
}

void transpose_matrix(float *a, int rows, int cols)
{
    float *transpose = calloc(rows*cols, sizeof(float));
    int x, y;
    for(x = 0; x < rows; ++x){
        for(y = 0; y < cols; ++y){
            transpose[y*rows + x] = a[x*cols + y];
        }
    }
    memcpy(a, transpose, rows*cols*sizeof(float));
    free(transpose);
}

void load_batchnorm_weights(layer l, FILE *fp)
{
    fread(l.scales, sizeof(float), l.c, fp);
    fread(l.rolling_mean, sizeof(float), l.c, fp);
    fread(l.rolling_variance, sizeof(float), l.c, fp);
#ifdef GPU
    if(gpu_index >= 0){
        push_batchnorm_layer(l);
    }
#endif
}

void load_convolutional_weights(layer l, FILE *fp)
{
    if(l.numload) l.n = l.numload;
    int num = l.c/l.groups*l.n*l.size*l.size;
    fread(l.biases, sizeof(float), l.n, fp);
    if (l.batch_normalize && (!l.dontloadscales)){
        fread(l.scales, sizeof(float), l.n, fp);
        fread(l.rolling_mean, sizeof(float), l.n, fp);
        fread(l.rolling_variance, sizeof(float), l.n, fp);
        if(0){
            int i;
            for(i = 0; i < l.n; ++i){
                printf("%g, ", l.rolling_mean[i]);
            }
            printf("\n");
            for(i = 0; i < l.n; ++i){
                printf("%g, ", l.rolling_variance[i]);
            }
            printf("\n");
        }
        if(0){
            fill_cpu(l.n, 0, l.rolling_mean, 1);
            fill_cpu(l.n, 0, l.rolling_variance, 1);
        }
        if(0){
            int i;
            for(i = 0; i < l.n; ++i){
                printf("%g, ", l.rolling_mean[i]);
            }
            printf("\n");
            for(i = 0; i < l.n; ++i){
                printf("%g, ", l.rolling_variance[i]);
            }
            printf("\n");
        }
    }
    fread(l.weights, sizeof(float), num, fp);
    //if(l.c == 3) scal_cpu(num, 1./256, l.weights, 1);
    if (l.flipped) {
        transpose_matrix(l.weights, l.c*l.size*l.size, l.n);
    }

#ifdef GPU
    if(gpu_index >= 0){
        push_convolutional_layer(l);
    }
#endif
}


void load_weights_upto(network *net, char *filename, int start, int cutoff)
{
#ifdef GPU
    if(net->gpu_index >= 0){
        opencl_set_device(net->gpu_index);
    }
#endif
    fprintf(stderr, "Loading weights from %s...\n", filename);
    fflush(stdout);
    FILE *fp = fopen(filename, "rb");
    if(!fp) file_error(filename);

    int major;
    int minor;
    int revision;
    fread(&major, sizeof(int), 1, fp);
    fread(&minor, sizeof(int), 1, fp);
    fread(&revision, sizeof(int), 1, fp);
    if ((major*10 + minor) >= 2 && major < 1000 && minor < 1000){
//#ifdef ARM
//        fread(net->seen, sizeof(unsigned long long), 1, fp); // 64-bit on ILP32 and LP64.
//#else
        fread(net->seen, sizeof(size_t), 1, fp);
//#endif
    } else {
        int iseen = 0;
        fread(&iseen, sizeof(int), 1, fp);
        *net->seen = iseen;
    }
    int transpose = (major > 1000) || (minor > 1000);

    int i;
    for(i = start; i < net->n && i < cutoff; ++i){
        layer l = net->layers[i];
        net->layers[i].which_layer=i;
        if (l.dontload) continue;
        if(l.type == CONVOLUTIONAL){
            load_convolutional_weights(l, fp);
        }
        if(l.type == BATCHNORM){
            load_batchnorm_weights(l, fp);
        }
        if(l.type == LOCAL){
            int locations = l.out_w*l.out_h;
            int size = l.size*l.size*l.c*l.n*locations;
            fread(l.biases, sizeof(float), l.outputs, fp);
            fread(l.weights, sizeof(float), size, fp);
#ifdef GPU
            if(gpu_index >= 0){
                push_local_layer(l);
            }
#endif
        }
    }
    fprintf(stderr, "done!\n");
    fclose(fp);
}

void load_weights(network *net, char *filename)
{
#ifndef SECURITY
    load_weights_upto(net, filename, 0, net->n);
#else
    tee_file_to_weight(net,filename,0,net->n);
#endif    
}
