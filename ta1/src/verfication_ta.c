#include "darknet_ta1.h"
#include "util_ta.h"
#include "verification_ta.h"
#include "string.h"
#include "sha256_ta.h"
#include "darknet_ta2.h"
#include <arm_neon.h>
TEE_TASessionHandle *sess=NULL;
float* matrixA=NULL,*matrixB=NULL,*matrixC=NULL;
double *result2=NULL;

TEE_Result TA_connect_to_ta2(void){
    sess=TEE_Malloc(sizeof(TEE_TASessionHandle),0);
    TEE_UUID uuid=TA_DRAKNET_TA_2_UUID;
    TEE_Param params[4];
    uint32_t origin;
    TEE_Result res=TEE_OpenTASession(&uuid,2000,0,params,sess,&origin);
    if(res!=TEE_SUCCESS){
        EMSG("Failed to open session of %d, res=0x%08x",uuid.timeLow,res);
        TEE_Free(sess);
        sess=NULL;
    }
    IMSG("Open session of 0x%08x sucessfully",uuid.timeLow);
    return res;
}

void TA_close_session(void){
    TEE_UUID uuid=TA_DRAKNET_TA_2_UUID;
    if(!sess) return;
    TEE_CloseTASession(*sess);
    TEE_Free(sess);
    sess=NULL;
    IMSG("Close session of 0X%08x",uuid.timeLow);
    return;
}

static void TA_Free_tmp_array(void)
{
    if(matrixA) TEE_Free(matrixA);
    if(matrixB) TEE_Free(matrixB);
    if(matrixC) TEE_Free(matrixC);
    if(result2) TEE_Free(result2);
    matrixA=NULL;
    matrixB=NULL;
    matrixC=NULL;
}

TEE_Result TA_copy_buffer(TEE_Param params[4]){
    uint32_t mod=params[3].value.b;
    if(mod==3){
        matrixA=TEE_Malloc(params[0].memref.size,0);
        matrixB=TEE_Malloc(params[1].memref.size,0);
        if(!matrixA || !matrixB){
            EMSG("Failed to malloc memory for A(%dMB) and B(%dMB)",
                params[0].memref.size/1024/1024,params[1].memref.size/1024/1024);
            TA_Free_tmp_array();
            return TEE_ERROR_OUT_OF_MEMORY;
        }
        TEE_MemMove(matrixA,params[0].memref.buffer,params[0].memref.size);
        TEE_MemMove(matrixB,params[1].memref.buffer,params[1].memref.size);
    }
    else{
        float **tt;
        uint32_t offset=params[1].value.a;
        if(mod==1) tt=&matrixA;
        else if(mod==2) tt=&matrixB;
        else tt=&matrixC;
        if(offset==0){
            *tt=TEE_Malloc(sizeof(float) * params[2].value.a * params[2].value.b, 0);
            if(!*tt){
                EMSG("Failed to malloc memory with %ldMB",
                    sizeof(float) * params[2].value.a * params[2].value.b/1024/1024);
                TA_Free_tmp_array();
                return TEE_ERROR_OUT_OF_MEMORY;
            }
        }
        TEE_MemMove((*tt)+offset,params[0].memref.buffer,params[0].memref.size);
    }
    return TEE_SUCCESS;
}

static TEE_Result TA_set_vec_to_ta2(float *vec,uint32_t size){
    uint32_t origin;
    uint32_t params_type=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,0,0,0);
    TEE_Param params[4];
    params[0].memref.buffer=vec;
    params[0].memref.size=size;
    TEE_Result res=TEE_InvokeTACommand(*sess,2000,TA_SET_VEC,params_type,params,&origin);
    
    return res;
}
static TEE_Result TA_set_r1_to_ta2(double *r1,uint32_t size){
    TEE_Param params[4];
    params[0].memref.buffer=r1;
    params[0].memref.size=size;
    uint32_t types=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,0,0,0);
    uint32_t origin;
    TEE_Result res=TEE_InvokeTACommand(*sess,2000,TA_SET_R1,types,params,&origin);
    return res;
}
static TEE_Result TA_get_vec(float **vec,uint32_t vsize){
    
    TEE_Param params[4];
    uint32_t types=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,0,0,0);
    uint32_t origin;
    *vec=TEE_Malloc(vsize,0);
    if(*vec==NULL) return TEE_ERROR_OUT_OF_MEMORY;
    params[0].memref.buffer=*vec;
    params[0].memref.size=vsize;
    TEE_Result res=TEE_InvokeTACommand(*sess,2000,TA_GET_VEC,types,params,&origin);
    if(res!=TEE_SUCCESS){
        TEE_Free(*vec);
        *vec=NULL;
        EMSG("Failed to get vec");
    }
    return res;
}

static TEE_Result TA_get_r1(double **r1,uint32_t rsize){
    
    TEE_Param params[4];
    uint32_t types=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,0,0,0);
    uint32_t origin;
    *r1=TEE_Malloc(rsize,0);
    if(*r1==NULL) return TEE_ERROR_OUT_OF_MEMORY;
    params[0].memref.buffer=*r1;
    params[0].memref.size=rsize;
    TEE_Result res=TEE_InvokeTACommand(*sess,2000,TA_GET_R1,types,params,&origin);
    if(res!=TEE_SUCCESS){
        TEE_Free(*r1);
        *r1=NULL;
        EMSG("Failed to get r1");
    }
    return res;
}

TEE_Result TA_pre_calculation(uint32_t types,TEE_Param params[4]){
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_VALUE_OUTPUT,0);
    if(exp!=types) return TEE_ERROR_BAD_PARAMETERS;
    TEE_Result res;
    uint32_t which_layer=params[0].value.a;
    uint32_t m=params[0].value.b, k=params[1].value.a, n=params[1].value.b;
    if(matrixA==NULL || matrixB==NULL){
        EMSG("Matrix A or B is NULL.");
        TA_Free_tmp_array();
        return TEE_ERROR_BAD_STATE;
    }

    float *tmp_rand=get_rand_vector(m);
    if(!tmp_rand){
        EMSG("Failed to malloc vector.");
        TA_Free_tmp_array();
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    res=TA_set_vec_to_ta2(tmp_rand,m*sizeof(float));
    if(TEE_SUCCESS!=res){
        TEE_Free(tmp_rand);
        TA_Free_tmp_array();
        EMSG("Failed to transfer the rand vector");
        return res;
    }
    TEE_OperationHandle handle;
    TEE_AllocateOperation(&handle,TEE_ALG_SHA256,TEE_MODE_DIGEST,0);
    uint32_t hash_code[8];
    uint32_t hash_size=32;
    res=TEE_DigestDoFinal(handle,matrixA,sizeof(float)*m*k,hash_code,&hash_size);
    TEE_FreeOperation(handle);
    if(res!=TEE_SUCCESS){
        EMSG("Failed to function of TEE_DigestDoFinal in TA_pre_calculation, res=0x%08x" ,res);
        TA_Free_tmp_array();
        return res;
    }
    params[2].value.a=0;
    if(!is_hash_same(hash_code,get_hash_from_table(which_layer))){
        IMSG("Matrix A is attacked in layer %d",which_layer);
        TEE_Free(tmp_rand);
        params[2].value.a=1;
        TA_Free_tmp_array();
        return TEE_SUCCESS;
    }

    double *result_ra=fvec_mul_fmatrix_neon1(tmp_rand,matrixA,m,k);
    if(!result_ra){
        EMSG("Failed to malloc result_ra.");
        TEE_Free(tmp_rand);
        TA_Free_tmp_array();
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_Free(matrixA);
    TEE_Free(tmp_rand);
    matrixA=NULL;
    double *result1=dvec_mul_fmatrix_neon(result_ra,matrixB,k,n);
    TEE_Free(result_ra);
    if(!result1){
        EMSG("Failed to malloc result1.");
        TA_Free_tmp_array();
        return TEE_ERROR_OUT_OF_MEMORY;
    }

    res=TA_set_r1_to_ta2(result1,n*sizeof(double));
    if(res!=TEE_SUCCESS)  EMSG("Failed to transfer result1 to ta2");
    
    TEE_Free(result1);
    TEE_Free(matrixB);
    matrixB=NULL;
    return res;
}

TEE_Result TA_verify_by_IFA(uint32_t types,TEE_Param params[4]){
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_VALUE_INPUT,0);
    if(exp!=types) return TEE_ERROR_BAD_PARAMETERS;
    float *vec;
    uint32_t which_layer=params[0].value.a;
    uint32_t M=params[1].value.a;
    uint32_t K=params[1].value.b;
    uint32_t N=params[2].value.a;

    TEE_Result res=TA_get_vec(&vec,M*sizeof(float));
    if(res!=TEE_SUCCESS) {
        EMSG("Failed to get rand vec of layer %d, res=0x%08x",which_layer,res);
        return res;
    }
    if(!matrixC) {
        EMSG("matrixC is NULL");
        return TEE_ERROR_BAD_PARAMETERS;
    }
    result2=fvec_mul_fmatrix_neon1(vec,matrixC,M,N);
    TEE_Free(vec);
    TEE_Free(matrixC);
    matrixC=NULL;
    if(!result2){
        TA_Free_tmp_array();
        return TEE_ERROR_OUT_OF_MEMORY;
    }

    return TEE_SUCCESS;
    
}

TEE_Result TA_vector_difference(uint32_t types,TEE_Param params[4]){
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_VALUE_INPUT,0);
    if(exp!=types)
        return TEE_ERROR_BAD_PARAMETERS;
    uint32_t which_layer=params[0].value.a;
    uint32_t M=params[1].value.a;
    uint32_t K=params[1].value.b;
    uint32_t N=params[2].value.a;
    double *result1=NULL;
    
    TEE_Result res=TA_get_r1(&result1,N*sizeof(double));
    if(res!=TEE_SUCCESS){
        EMSG("Failed to get result1 of layer %d, res=0x%08x",which_layer,res);
        TEE_Free(result2);
        result2=NULL;
        return res;
    }
    params[0].value.b=vector_diffirent(result1,result2,M,K,N);
    TEE_Free(result1);
    TEE_Free(result2);
    result2=NULL;
    return TEE_SUCCESS;
    
}