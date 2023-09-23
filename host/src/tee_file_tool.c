#include "tee_flie_tool.h"
#include "darknet.h"
#include "utils.h"
#include <string.h>
#include "parser.h"
#include "convolutional_layer.h"
#include "batchnorm_layer.h"
#include "local_layer.h"

#ifdef SECURITY

#include "user_ta_header_defines.h"
const unsigned long PART_ELE_MAXNUM = 1048576 / sizeof(float);
void itoa(int num, char *str, int radix)
{
    char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned unum;
    int i = 0, j, k;
    if (radix == 10 && num < 0)
    {
        unum = (unsigned)-num;
        str[i++] = '-';
    }
    else
        unum = (unsigned)num;
    do
    {
        str[i++] = index[unum % (unsigned)radix];
        unum /= radix;
    } while (unum);
    str[i] = '\0';
    if (str[0] == '-')
        k = 1;
    else
        k = 0;

    char temp;
    for (j = k; j <= (i - 1) / 2; j++)
    {
        temp = str[j];
        str[j] = str[i - 1 + k - j];
        str[i - 1 + k - j] = temp;
    }
}

void splice_float_array(float *dst, float *src, int start, int src_num)
{
    int i, j;
    for (i = start, j = 0; j < src_num; ++i, ++j)
    {
        dst[i] = src[j];
    }
}

char *get_origin_filename(char *filename)
{
    int last_dot = -1;
    int length;
    for (length = 0; filename[length] != '\0'; length++)
    {
        if (filename[length] == '/' || filename[length] == '\\')
        {
            last_dot = length;
        }
    }
    char *result = malloc(100);
    int i = 0;
    int j = last_dot + 1;

    while (j < length)
    {
        result[i] = filename[j];
        i++;
        j++;
    }
    result[i] = '\0';
    return result;
}

#ifdef SECURITY
#include "darknet_ta1.h"
#include "verfication_host.h"
TEEC_Result is_tee_file_exist(TEE_Ctx_Util *ctx_util, char *objid, size_t objid_size)
{
    uint32_t origin;
    TEEC_Operation op;
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                                     TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = objid,
    op.params[0].tmpref.size = objid_size;

    TEEC_Result res = TEEC_InvokeCommand(&ctx_util[0].sess, TA_FILE_IS_EXIST, &op, &origin);
    return res;
}

TEEC_Result write_tee_weight_file(TEE_Ctx_Util *ctx_util, void *data, unsigned long data_size,
                                  char *orign_filename, int which_layer)
{
    // static size_t last_size=0;
    TEEC_Result res;
    uint32_t origin;
    TEEC_Operation op;
    char filename1[80];
    strcpy(filename1, orign_filename);

    printf("layer=%d datasize=%fMB\n", which_layer, data_size / 1024.0 / 1024);

    if (which_layer == -1)
    {
        strcat(filename1, "_header");
    }
    else
    {
        char num[8];
        itoa(which_layer, num, 10);
        strcat(filename1, num);
    }

    const unsigned long part_max_size = PART_ELE_MAXNUM * sizeof(float);
    int part = 1;
    for (unsigned long offset = 0; offset < data_size; offset += part_max_size, part++)
    {
        char filename2[80];
        char tmp[4];
        itoa(part, tmp, 10);
        strcpy(filename2, filename1);
        strcat(filename2, "_p");
        strcat(filename2, tmp);
        memset(&op, 0, sizeof(op));
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                                         TEEC_MEMREF_TEMP_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
        op.params[0].tmpref.buffer = filename2;
        op.params[0].tmpref.size = strlen(filename2);
        op.params[1].tmpref.buffer = data + offset;
        if (offset + part_max_size <= data_size)
            op.params[1].tmpref.size = part_max_size;
        else
            op.params[1].tmpref.size = data_size - offset;

        op.params[2].value.a = which_layer;

        res = TEEC_InvokeCommand(&ctx_util[0].sess, TA_WRITE_TEE_FILE, &op, &origin);
        if (res != TEEC_SUCCESS)
        {
            printf("write tee weight file failed layer%d__offset%ld, res=0x%08x origin=%ld\n",
                   which_layer, offset, res, origin);
            return res;
        }
    }

    return res;
}

TEEC_Result write_sha256_hash_to_tee(TEE_Ctx_Util *ctx_util, void *data, int data_size, char *origin_filename, int which_layer)
{
    const int max_size = TA_SHM_TRANS_SIZE-1024;
    char objid[80];
    char tmp[4];
    strcpy(objid, origin_filename);
    strcat(objid,"_hash");
    itoa(which_layer,tmp,10);
    strcat(objid,tmp);
    TEEC_Operation op;
    uint32_t orign;
    TEEC_Result res;

    itoa(which_layer, tmp, 10);
    for (int offset = 0; offset < data_size; offset += max_size)
    {
        memset(&op, 0, sizeof(op));
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
        // int is_last=0;
        op.params[0].tmpref.buffer = objid;
        op.params[0].tmpref.size = strlen(objid);
        op.params[1].tmpref.buffer = data + offset;
        if (offset + max_size > data_size)
        {
            op.params[1].tmpref.size = data_size - offset;
            op.params[2].value.a=1;
            res = TEEC_InvokeCommand(&ctx_util[0].sess, TA_GENERATE_HA1, &op, &orign);
            if (TEEC_SUCCESS != res)
            {
                printf("Failed to run command TA_GENERATE_HA1 in %dth layer, res=0x%08x\n", which_layer, res);
                return res;
            }
        }
        else
        {
            op.params[1].tmpref.size = max_size;
            op.params[2].value.a=0;
            res = TEEC_InvokeCommand(&ctx_util[0].sess, TA_GENERATE_HA1, &op, &orign);
            if (TEEC_SUCCESS != res)
            {
                printf("Failed to run command TA_GENERATE_HA1 in %dth layer, res=0x%08x\n", which_layer, res);
                return res;
            }
        }
    }
    return TEEC_SUCCESS;
}

TEEC_Result init_sha256_hash_to_sRAM(TEE_Ctx_Util *ctx_util,char* origin_filename,int which_layer){
    char objid[80];
    strcpy(objid,origin_filename);
    strcat(objid,"_hash");
    char tmp[4];
    itoa(which_layer,tmp,10);
    strcat(objid,tmp);
    TEEC_Operation op;
    op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_VALUE_INPUT,TEEC_NONE,TEEC_NONE);
    op.params[0].tmpref.buffer=objid;
    op.params[0].tmpref.size=strlen(objid);
    op.params[1].value.a=which_layer;
    uint32_t orign;
    TEEC_Result res;
    for(int i=0;i<DEFAULT_MAX_THREAD_NUM;i++){
        res=TEEC_InvokeCommand(&ctx_util[0].sess,TA_INIT_HASH_TABLE,&op,&orign);
        if(res!=TEEC_SUCCESS){
            printf("Failed to run command TA_INIT_HASH_TABLE in %dth layer, res=0x%08x\n",which_layer,res);
        }
    }
    
    return res;
}

TEEC_Result write_tee_convolutional_weights(TEE_Ctx_Util *ctx_util, layer *lay, int which_layer,
                                            FILE *fp, char *orign_filename)
{
    char filename[80];
    strcpy(filename, orign_filename);
    TEEC_Result res;
    // read data from weight file
    if (lay->numload)
        lay->n = lay->numload;
    int num = lay->c / lay->groups * lay->n * lay->size * lay->size;
    fread(lay->biases, sizeof(float), lay->n, fp);
    int head_num = lay->n;
    if (lay->batch_normalize && (!lay->dontloadscales))
    {
        fread(lay->scales, sizeof(float), lay->n, fp);
        fread(lay->rolling_mean, sizeof(float), lay->n, fp);
        fread(lay->rolling_variance, sizeof(float), lay->n, fp);
        head_num += lay->n * 3;
    }
    fread(lay->weights, sizeof(float), num, fp);

    // load all data to a float array
    size_t head_size = sizeof(float) * head_num;

    float *data = (float *)malloc(head_size);
    int start = 0;
    splice_float_array(data, lay->biases, start, lay->n);
    start += lay->n;
    if (lay->batch_normalize && (!lay->dontloadscales))
    {
        splice_float_array(data, lay->scales, start, lay->n);
        start += lay->n;
        splice_float_array(data, lay->rolling_mean, start, lay->n);
        start += lay->n;
        splice_float_array(data, lay->rolling_variance, start, lay->n);
        start += lay->n;
    }
    // splice_float_array(data, lay->weights, start, num);
    strcat(filename, "_header");
    res = write_tee_weight_file(ctx_util, data, head_size, filename, which_layer);
    free(data);
    if (res != TEEC_SUCCESS) {
        printf("Failed to writed %s, res=\0x%08x\n",res);
        return res;
    }

    strcpy(filename, orign_filename);
    strcat(filename, "_weights");
    res = write_tee_weight_file(ctx_util, lay->weights, num * sizeof(float), filename, which_layer);
    if (res != TEEC_SUCCESS) {
        printf("Failed to writed %s, res=\0x%08x\n",res);
        return res;
    }
    res=write_sha256_hash_to_tee(ctx_util,lay->weights,num*sizeof(float),orign_filename,which_layer);
    if (res != TEEC_SUCCESS) {
        printf("Failed to writed hash code %s, res=\0x%08x\n",res);
    }
    return res;
}

TEEC_Result write_tee_batchnorm_weight(TEE_Ctx_Util *ctx_util, layer *lay,
                                       int which_layer, FILE *fp, char *orign_filename)
{
    fread(lay->scales, sizeof(float), lay->c, fp);
    fread(lay->rolling_mean, sizeof(float), lay->c, fp);
    fread(lay->rolling_variance, sizeof(float), lay->c, fp);
    float *tmp = (float *)malloc(sizeof(float) * lay->c * 3);
    splice_float_array(tmp, lay->scales, 0, lay->c);
    splice_float_array(tmp, lay->rolling_mean, lay->c, lay->c);
    splice_float_array(tmp, lay->rolling_variance, lay->c * 2, lay->c);
    TEEC_Result res = write_tee_weight_file(ctx_util, tmp, sizeof(float) * lay->c * 3, orign_filename,
                                            which_layer);
    free(tmp);
    return res;
}

TEEC_Result cfg_to_tee_file(char *filename){
    char *orign_name = get_origin_filename(filename);
    fprintf(stderr, "Read network structure from %s...\n", filename);

    FILE *fp=fopen(filename,"r");
    if(!fp)
        file_error(filename);
    
    int max_length=1024;
    int length=0;
    char *buffer=(char*)malloc(1024);
    int ch;
    while((ch=fgetc(fp))!=EOF){
        buffer[length]=(char)ch;
        length++;
        if(length==max_length){
            max_length*=2;
            buffer=realloc(buffer,max_length);
        }
    }
    fclose(fp);
    buffer[length]='\0';
    TEE_Ctx_Util *ctx_util=prepare_tee_session();
    TEEC_Result res;
    uint32_t origin;
    TEEC_Operation op;
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                                         TEEC_MEMREF_TEMP_INPUT, TEEC_VALUE_INPUT, TEEC_NONE);
    op.params[0].tmpref.buffer=orign_name;
    op.params[0].tmpref.size=strlen(orign_name);
    op.params[1].tmpref.buffer=buffer;
    op.params[1].tmpref.size=strlen(buffer);
    op.params[2].value.a=-1;
    res=TEEC_InvokeCommand(&ctx_util[0].sess,TA_WRITE_TEE_FILE,&op,&origin);
    free(buffer);
    free(orign_name);
    close_TEE_ctx_util(ctx_util);
    if(res!=TEEC_SUCCESS){
        printf("Fail to write cfg to tee,res=%08x\n",res);
    }
    return res;

}

void weight_to_tee_flie(network *net, char *filename, int start, int cutoff)
{

    char *orign_name = get_origin_filename(filename);

    fprintf(stderr, "Read weights from %s...\n", filename);
    fflush(stdout);
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        file_error(filename);

    int major;
    int minor;
    int revision;
    fread(&major, sizeof(int), 1, fp);
    fread(&minor, sizeof(int), 1, fp);
    fread(&revision, sizeof(int), 1, fp);
    if ((major * 10 + minor) >= 2 && major < 1000 && minor < 1000)
    {
        fread(net->seen, sizeof(size_t), 1, fp);
    }
    else
    {
        int iseen = 0;
        fread(&iseen, sizeof(int), 1, fp);
        *net->seen = iseen;
    }
    long weight_head[4] = {major, minor, revision, *(net->seen)};
    net->ctx_util = prepare_tee_session();
    TEEC_Result res = write_tee_weight_file(net->ctx_util, weight_head,
                                            sizeof(long) * 4, orign_name, -1);
    if (res != TEEC_SUCCESS)
    {
        printf("Failed to write %s,res=0x%08x\n",orign_name,res);
        free(orign_name);
        fclose(fp);
        return;
    }
    int transpose = (major > 1000) || (minor > 1000);
    for (int i = start; i < net->n && i < cutoff; ++i)
    {
        layer l = net->layers[i];
        if (l.dontload)
            continue;
        if (l.type == CONVOLUTIONAL)
        {
            res = write_tee_convolutional_weights(net->ctx_util, &l, i, fp, orign_name);
            if (res != TEEC_SUCCESS)
            {
                printf("Failed to write %s,res=0x%08x\n",orign_name,res);
                close_TEE_ctx_util(net->ctx_util);
                free(orign_name);
                fclose(fp);
                return;
            }
        }
        else if (l.type == BATCHNORM)
        {
            res = write_tee_batchnorm_weight(net->ctx_util, &l, i, fp, orign_name);
            if (res != TEEC_SUCCESS)
            {
                close_TEE_ctx_util(net->ctx_util);
                free(orign_name);
                fclose(fp);
                return;
            }
        }
        else if (l.type == LOCAL)
        {
            int locations = l.out_w * l.out_h;
            int size = l.size * l.size * l.c * l.n * locations;
            fread(l.biases, sizeof(float), l.outputs, fp);
            fread(l.weights, sizeof(float), size, fp);
            int total = sizeof(float) * (l.outputs + size);
            float *tmp = (float *)malloc(total);
            splice_float_array(tmp, l.biases, 0, l.outputs);
            splice_float_array(tmp, l.weights, l.outputs, size);
            res = write_tee_weight_file(net->ctx_util, tmp, total, orign_name, i);
            free(tmp);
            if (res != TEEC_SUCCESS)
            {   
                close_TEE_ctx_util(net->ctx_util);
                free(orign_name);
                fclose(fp);
                return;
            }
        }
        
    }
    close_TEE_ctx_util(net->ctx_util);
    fprintf(stderr, "done!\n");
    free(orign_name);
    fclose(fp);
}

TEEC_Result read_tee_weight_file(TEE_Ctx_Util *ctx_util, void *data, unsigned long data_size, char *filename, int which_layer)
{
    TEEC_Result res;
    TEEC_Operation op;
    uint32_t origin;
    char filename1[80];
    strcpy(filename1, filename);
    if (which_layer == -1)
    {
        strcat(filename1, "_header");
    }
    else
    {
        char tmp[4];
        itoa(which_layer, tmp, 10);
        strcat(filename1, tmp);
    }

    int part = 1;
    const unsigned long part_max_size = PART_ELE_MAXNUM * sizeof(float);
    for (unsigned long offset = 0; offset < data_size; offset += part_max_size, part++)
    {
        char filename2[80];
        char tmp[4];
        void *buffer;
        unsigned long buffer_size = 0;
        if (offset + part_max_size < data_size)
        {
            buffer = malloc(part_max_size);
            buffer_size = part_max_size;
        }
        else
        {
            buffer = malloc(data_size - offset);
            buffer_size = data_size - offset;
        }
        memset(buffer, 0, buffer_size);

        itoa(part, tmp, 10);
        strcpy(filename2, filename1);
        strcat(filename2, "_p");
        strcat(filename2, tmp);
        memset(&op, 0, sizeof(op));
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
                                         TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
        op.params[0].tmpref.buffer = filename2;
        op.params[0].tmpref.size = strlen(filename2);
        op.params[1].tmpref.buffer = buffer;
        op.params[1].tmpref.size = buffer_size;

        res = TEEC_InvokeCommand(&ctx_util[0].sess, TA_READ_TEE_FILE, &op, &origin);
        if (res != TEEC_SUCCESS)
        {
            free(buffer);
            printf("Read file %s filed, res=0x%08x origin=%d\n", filename2, res, origin);
            return res;
        }
        splice_float_array(data, buffer, offset / sizeof(float), buffer_size / sizeof(float));
        free(buffer);
    }
    return res;
}

TEEC_Result load_convolutional_weight_from_tee(TEE_Ctx_Util *ctx_util, layer *lay, char *filename, int which_layer)
{
    char filename1[80];
    strcpy(filename1, filename);
    strcat(filename1, "_header");
    if (lay->numload)
        lay->n = lay->numload;
    int num = lay->c / lay->groups * lay->n * lay->size * lay->size;
    unsigned long datasz = lay->n * sizeof(float);
    if (lay->batch_normalize && !(lay->dontloadscales))
    {
        datasz += lay->n * 3 * sizeof(float);
    }
    float *data = malloc(datasz);
    TEEC_Result res = read_tee_weight_file(ctx_util, data, datasz, filename1, which_layer);
    if (res != TEEC_SUCCESS)
    {
        printf("Read tee file %s in layer%d failen, res=0x%08x\n", filename, which_layer, res);
        free(data);
        return res;
    }

    memcpy(lay->biases, data, sizeof(float) * lay->n);
    int offset = lay->n;
    if (lay->batch_normalize && !(lay->dontloadscales))
    {
        memcpy(lay->scales, data + offset, sizeof(float) * lay->n);
        offset += lay->n;
        memcpy(lay->rolling_mean, data + offset, sizeof(float) * lay->n);
        offset += lay->n;
        memcpy(lay->rolling_variance, data + offset, sizeof(float) * lay->n);
        offset += lay->n;
    }
    free(data);
    strcpy(filename1, filename);
    strcat(filename1, "_weights");
    res = read_tee_weight_file(ctx_util, lay->weights, sizeof(float) * num, filename1, which_layer);
    if(res!=TEEC_SUCCESS){
        free(lay->weights);
        return res;
    }
    res=init_sha256_hash_to_sRAM(ctx_util,filename,which_layer);
    if(res!=TEEC_SUCCESS){
        return res;
    }
    if (lay->flipped)
    {
        transpose_matrix(lay->weights, lay->c * lay->size * lay->size, lay->n);
    }
#ifdef GPU
    if (gpu_index >= 0)
    {
        push_convolutional_layer(*lay);
    }
#endif
    return TEEC_SUCCESS;
}

TEEC_Result load_batchnorm_weights_from_tee(TEE_Ctx_Util *ctx_util, layer *lay, char *filename, int which_layer)
{
    unsigned long datasz = lay->c * 3 * sizeof(float);
    float *data = malloc(datasz);
    TEEC_Result res = read_tee_weight_file(ctx_util, data, datasz, filename, which_layer);
    if (res != TEEC_SUCCESS)
    {
        printf("Read batchnorm layer%d failed, res=0x%08x\n", which_layer, res);
        free(data);
        return res;
    }
    memcpy(lay->scales, data, lay->c);
    memcpy(lay->rolling_mean, data + lay->c, lay->c);
    memcpy(lay->rolling_variance, data + lay->c * 2, lay->c);
#ifdef GPU
    if (gpu_index >= 0)
    {
        push_batchnorm_layer(*lay);
    }
#endif
    return TEEC_SUCCESS;
}

void tee_file_to_weight(network *net, char *filename, int start, int cutoff)
{
#ifdef GPU
    if (net->gpu_index >= 0)
    {
        opencl_set_device(net->gpu_index);
    }
#endif
    srand((unsigned)time(NULL));
    if (0 != create_tpool(&(net->thread_pool), DEFAULT_MAX_THREAD_NUM))
    {
        printf("create_tpool failed!\n");
    }
    TEEC_Result res;
    char *base_name=get_origin_filename(filename);
    strcpy(filename,base_name);
    printf("load weights from tee %s.....\n", filename);
    net->weights_file = malloc(strlen(filename) + 1);
    strcpy(net->weights_file, filename);
    net->ctx_util = prepare_tee_session();
    unsigned long data_size = sizeof(long) * 4;
    long *data = malloc(data_size);
    res = read_tee_weight_file(net->ctx_util, data, data_size, filename, -1);
    if (res != TEEC_SUCCESS)
    {
        free(data);
        close_TEE_ctx_util(net->ctx_util);
        exit(1);
        // return;
    }
    int major = data[0];
    int minor = data[1];
    int revision = data[2];
    *(net->seen) = data[3];
    free(data);
    int transpose = (major > 1000) || (minor > 1000);
    for (int i = start; i < net->n && i < cutoff; ++i)
    {
        net->layers[i].which_layer = i;
        layer l = net->layers[i];
        if (l.dontload)
            continue;
        if (l.type == CONVOLUTIONAL)
        {
            if (TEEC_SUCCESS != load_convolutional_weight_from_tee(
                                    net->ctx_util, &l, filename, i))
            {
                close_TEE_ctx_util(net->ctx_util);
                exit(1);
                // return;
            }
        }
        else if (l.type == BATCHNORM)
        {
            if (TEEC_SUCCESS != load_batchnorm_weights_from_tee(
                                    net->ctx_util, &l, filename, i))
            {
                close_TEE_ctx_util(net->ctx_util);
                exit(1);
                // return;
            }
        }
        else if (l.type == LOCAL)
        {
            int locations = l.out_w * l.out_h;
            int size = l.size * l.size * l.c * l.n * locations;
            data_size = (l.outputs + size) * sizeof(float);
            data = malloc(data_size);
            if (TEEC_SUCCESS != read_tee_weight_file(net->ctx_util, data, data_size, filename, i))
            {
                free(data);
                printf("Read local layer%d failed\n");
                close_TEE_ctx_util(net->ctx_util);
                exit(1);
                // return;
            }
            memcpy(l.biases, data, l.outputs * sizeof(float));
            memcpy(l.weights, data + l.outputs, size * sizeof(float));
#ifdef GPU
            if (gpu_index >= 0)
            {
                push_local_layer(l);
            }
#endif
            free(data);
        }
    }

    printf("done\n");
    // close_TEE_ctx_util(net->ctx_util);
}

#endif

#endif
