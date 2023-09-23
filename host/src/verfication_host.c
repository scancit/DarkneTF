#ifdef SECURITY
#include <stdio.h>
#include <err.h>
#include <string.h>
#include "verfication_host.h"
#include <stdlib.h>
#include "tee_flie_tool.h"
#include "user_ta_header_defines.h"
#include "darknet_ta2.h"
#include <semaphore.h>

sem_t isPR1;
TEE_Ctx_Util *util_ta2=NULL;

TEE_Ctx_Util *prepare_tee_session()
{
    TEEC_UUID uuid1=TA_DRAKNET_TA_1_UUID;
    TEEC_UUID uuid2=TA_DRAKNET_TA_2_UUID;
    uint32_t origin;
    TEEC_Result res;

    util_ta2=(TEE_Ctx_Util *)malloc(sizeof(TEE_Ctx_Util));

    TEEC_InitializeContext(NULL,&util_ta2->ctx);
    TEEC_OpenSession(&util_ta2->ctx,&util_ta2->sess,&uuid2,TEEC_LOGIN_PUBLIC,NULL,NULL,&origin);

    TEE_Ctx_Util *ctxUtil = (TEE_Ctx_Util *)malloc(sizeof(TEE_Ctx_Util)*2);
    for(int i=0;i<2;i++){
        res = TEEC_InitializeContext(NULL, &ctxUtil[i].ctx);
        if (res != TEEC_SUCCESS){
            close_TEE_ctx_util(ctxUtil);
            free(ctxUtil);
            errx(1, "TEEC_InitializeContext failed with code 0x%x", res);
        }
        res=TEEC_OpenSession(&ctxUtil[i].ctx,&ctxUtil[i].sess,&uuid1,TEEC_LOGIN_PUBLIC,NULL,NULL,&origin);
        if(res!=TEEC_SUCCESS){
            close_TEE_ctx_util(ctxUtil);
            free(ctxUtil);
            errx(1,"Run TEEC_OpenSession unsuccessfully,res=0x%08x\n",res);
        }
    }
    sem_init(&isPR1,0,0);

    return ctxUtil;
}

void close_TEE_ctx_util(TEE_Ctx_Util *ctxUtil)
{   
    for(int i=0;i<2;i++){
        TEEC_CloseSession(&ctxUtil[i].sess);
        TEEC_FinalizeContext(&ctxUtil[i].ctx);
    }
    TEEC_CloseSession(&util_ta2->sess);
    TEEC_FinalizeContext(&util_ta2->ctx);
    sem_destroy(&isPR1);
    free(ctxUtil);
}

gemm_vf* init_gemmvf(float *a,float *b,float *c,int m,int k,int n,TEE_Ctx_Util *util,int which_layer,char *weight,char*image){
    gemm_vf* ver=(gemm_vf*)malloc(sizeof(gemm_vf));
    ver->a=a;
    ver->b=b;
    ver->c=c;
    ver->M=m;
    ver->K=k;
    ver->N=n;
    ver->which_layer=which_layer;
    ver->weight_file=weight;
    ver->image_path=image;
    ver->util=util;
    return ver;
}


void* verify_by_IFA(void* arg){
    gemm_vf *ver=(gemm_vf*)arg;
    TEEC_Operation op;
    TEEC_Result res;
    uint32_t origin;
    memset(&op,0,sizeof(op));
    op.paramTypes=TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,0);
    op.params[0].value.a=ver->which_layer;
    op.params[0].value.b=1;
    op.params[1].value.a=ver->M;
    op.params[1].value.b=ver->K;
    op.params[2].value.a=ver->N;
    
    res=TEEC_InvokeCommand(&ver->util[1].sess,TA_VERIFY_BY_IFA,&op,&origin);

    if(res!=TEEC_SUCCESS){
        free(ver);
        close_TEE_ctx_util(ver->util);
        errx(1,"Error: Failed to run TA_VERIFY_BY_IFA, res=0x%08x\n", res);
    }

    op.paramTypes=TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,0);
    sem_wait(&isPR1);
    res=TEEC_InvokeCommand(&ver->util[1].sess,TA_VERIFY_DIFFRIENCE,&op,&origin);
    if(res!=TEEC_SUCCESS){
        free(ver);
        close_TEE_ctx_util(ver->util);
        errx(1,"Error: Failed to run TA_VERIFY_DIFFRIENCE, res=0x%08x\n", res);
    }
    if(op.params[0].value.b==0)
        printf("Image \"%s\" layer %d is attacked\n",ver->image_path ,ver->which_layer);
    free(ver);

    return NULL;
}   


void security_fun(TEE_Ctx_Util *ctx_util, tpool_t *pool,float *C, int M,int K, int N, 
                  int which_layer, char *weight_file,char *image_path){
    if(!ctx_util || !pool)
        errx(1,"Error:threadpool is NULL\n");
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;
    uint32_t sizeC=M*N*sizeof(float);
    memset(&op,0,sizeof(op));
    op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT);
    op.params[2].value.a=M;
    op.params[2].value.b=N;
    op.params[3].value.b=4;

    for(uint32_t offsetC=0;offsetC<sizeC;offsetC+=TA_SHM_TRANS_SIZE){
        op.params[0].tmpref.buffer=C+offsetC/sizeof(float);
        op.params[0].tmpref.size=offsetC+TA_SHM_TRANS_SIZE<=sizeC?TA_SHM_TRANS_SIZE:sizeC-offsetC;
        op.params[1].value.a=offsetC/sizeof(float);
        res=TEEC_InvokeCommand(&ctx_util[1].sess,TA_COPY_BUFFER,&op,&origin);
        if(TEEC_SUCCESS!=res){
            close_TEE_ctx_util(ctx_util);
            errx(1,"Faild to Run TA_COPY_BUFFER, res=0x%08x",res);
        }
    }
    gemm_vf *ver=init_gemmvf(NULL,NULL,NULL,M,K,N,ctx_util,which_layer,NULL,image_path);

    add_task_2_tpool(pool,verify_by_IFA,ver);
    
}

void *pre_sec_cal(void *arg){
    
    gemm_vf *ver=(gemm_vf*)arg;
    
    TEEC_Operation op;
    uint32_t origin;
    memset(&op,0,sizeof(op));
    op.paramTypes=TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_OUTPUT,TEEC_NONE);
    op.params[0].value.a=ver->which_layer;
    op.params[0].value.b=ver->M;
    op.params[1].value.a=ver->K;
    op.params[1].value.b=ver->N;

    TEEC_Result res=TEEC_InvokeCommand(&(ver->util[0].sess),TA_PRE_CALCULATION,&op,&origin);
    sem_post(&isPR1);
    if(res!=TEEC_SUCCESS){
        close_TEE_ctx_util(ver->util);
        free(ver);
        errx(1,"Error: Failed to run TA_PRE_CALCULATION, res=0x%08x",res);
    }
    if(op.params[2].value.a){
        printf("The weight at layer %d is attacked",ver->which_layer);
    }
    free(ver);
    return NULL;
}

void pre_sec_copy(TEE_Ctx_Util *ctx_util,tpool_t *pool, float *matrixA,float *matrixB,int m,int k,int n,int which_layer)
{   
    TEEC_Operation op;
    uint32_t origin;
    TEEC_Result res;
    uint32_t sizeA=m*k;
    uint32_t sizeB=n*k;
    TEEC_Session *sess=&ctx_util[0].sess;
    const uint32_t PART_MAX_SIZE=TA_SHM_TRANS_SIZE/sizeof(float);
    // sem_wait(semAB); //等待子线程计算完毕
    if(sizeA+sizeB>PART_MAX_SIZE){
        memset(&op,0,sizeof(op));
        op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT);
        op.params[2].value.a=m;
        op.params[2].value.b=k;
        op.params[3].value.b=1;
        for(uint32_t offsetA=0;offsetA<sizeA;offsetA+=PART_MAX_SIZE){
            op.params[0].tmpref.buffer=matrixA+offsetA;
            op.params[0].tmpref.size= offsetA+PART_MAX_SIZE<=sizeA?TA_SHM_TRANS_SIZE:(sizeA-offsetA)*sizeof(float);
            op.params[1].value.a=offsetA;
            res=TEEC_InvokeCommand(sess,TA_COPY_BUFFER,&op,&origin);
            if(res!=TEEC_SUCCESS){
                close_TEE_ctx_util(ctx_util);
                errx(1,"Failed to run TA_COPY_BUFFER command, res=0x%08x",res);
            }
        }

        memset(&op,0,sizeof(op));
        op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT);
        op.params[2].value.a=k;
        op.params[2].value.b=n;
        op.params[3].value.a=0;
        op.params[3].value.b=2;
        for(uint32_t offsetB=0;offsetB<sizeB;offsetB+=PART_MAX_SIZE){
            op.params[0].tmpref.buffer=matrixB+offsetB;
            op.params[0].tmpref.size=offsetB+PART_MAX_SIZE<=sizeB?TA_SHM_TRANS_SIZE:(sizeB-offsetB)*sizeof(float);
            op.params[1].value.a=offsetB;
            res=TEEC_InvokeCommand(sess,TA_COPY_BUFFER,&op,&origin);
            if(res!=TEEC_SUCCESS){
                close_TEE_ctx_util(ctx_util);
                errx(1,"Failed to run TA_COPY_BUFFER command, res=0x%08x",res);
            }
        }
    }
    else{
        memset(&op,0,sizeof(op));
        op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_MEMREF_TEMP_INPUT,TEEC_VALUE_INPUT,TEEC_VALUE_INPUT);
        op.params[0].tmpref.buffer=matrixA;
        op.params[0].tmpref.size=sizeA*sizeof(float);
        op.params[1].tmpref.buffer=matrixB;
        op.params[1].tmpref.size=sizeB*sizeof(float);
        op.params[2].value.a=m;
        op.params[2].value.b=k;
        op.params[3].value.a=n;
        op.params[3].value.b=3;
        res=TEEC_InvokeCommand(sess,TA_COPY_BUFFER,&op,&origin);
        if(res!=TEEC_SUCCESS){
            close_TEE_ctx_util(ctx_util);
            errx(1,"Failed to run TA_COPY_BUFFER command, res=0x%08x",res);
        }
    }
    gemm_vf *ver=init_gemmvf(NULL,NULL,NULL,m,k,n,ctx_util,which_layer,NULL,NULL);
    add_task_2_tpool(pool,pre_sec_cal,ver);  
}

#endif