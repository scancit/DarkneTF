#ifndef __VERFICATION_HOST_H__
#define __VERFICATION_HOST_H__
#ifdef SECURITY
#include <tee_client_api.h>
#include <gemm.h>
#include "darknet_ta1.h"
#include "opencl.h"
#include "threadpool.h"

/* TEE Connection util */
typedef struct TEE_Contxt_Sess{
	TEEC_Context ctx;
	TEEC_Session sess;
}TEE_Ctx_Util;

typedef struct Gemm_vf
{
    int M,N,K;
#ifdef GPU
    cl_command_queue queue;
#endif
	TEE_Ctx_Util *util;
    float *a;
    float *b;
    float *c;
    int which_layer;
    char *weight_file;
    char *image_path;
    int transA;
}gemm_vf;

TEE_Ctx_Util* prepare_tee_session();
void close_TEE_ctx_util(TEE_Ctx_Util *ctxUtil);
void security_fun(TEE_Ctx_Util *ctx_util, tpool_t *pool,float *C, int M,int K, int N, 
                  int which_layer, char *weight_file,char *image_path);
void pre_sec_copy(TEE_Ctx_Util *ctx_util,tpool_t *pool, float *matrixA,float *matrixB,int m,int k,int n,int which_layer);             
#endif
#endif