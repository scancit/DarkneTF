#include "darknet_ta2.h"
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

float *vec=NULL;
double *r1=NULL;
static void free_global_buffer(void){
    if(r1) {
        TEE_Free(r1);
        r1=NULL;
    }
    if(vec){
        TEE_Free(vec);
        vec=NULL;
    }

}

static TEE_Result TA_set_r1(uint32_t types,TEE_Param params[4]){
    if(r1){
        TEE_Free(r1);
        r1=NULL;
        EMSG("The vector r1 is not consumed");
    } 
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,0,0,0);
    if(exp!=types) return TEE_ERROR_BAD_PARAMETERS;
    r1=TEE_Malloc(params[0].memref.size,0);
    if(!r1){
        EMSG("Failed to malloc buffer of %dB",params[0].memref.size);
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_MemMove(r1,params[0].memref.buffer,params[0].memref.size);
    return TEE_SUCCESS;
}

static TEE_Result TA_set_vec(uint32_t types,TEE_Param params[4]){
    if(vec) TEE_Free(vec);
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,0,0,0);
    if(exp!=types) return TEE_ERROR_BAD_PARAMETERS;
    vec=TEE_Malloc(params[0].memref.size,0);
    if(!vec){
        EMSG("Failed to malloc buffer of %dB",params[0].memref.size);
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_MemMove(vec,params[0].memref.buffer,params[0].memref.size);
    return TEE_SUCCESS;
}


static TEE_Result TA_get_vec(uint32_t types,TEE_Param params[4]){
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,0,0,0);
    if(exp!=types) return TEE_ERROR_BAD_PARAMETERS;
    if(!vec) {
        free_global_buffer();
        return TEE_ERROR_BAD_STATE;
        
    }
    TEE_MemMove(params[0].memref.buffer,vec,params[0].memref.size);
    TEE_Free(vec);
    vec=NULL;
    return TEE_SUCCESS;
}
static TEE_Result TA_get_r1(uint32_t types,TEE_Param params[4]){
    uint32_t exp=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,0,0,0);
    if(exp!=types) return TEE_ERROR_BAD_PARAMETERS;
    if(!r1) {
        free_global_buffer();
        return TEE_ERROR_BAD_STATE;
    }
    TEE_MemMove(params[0].memref.buffer,r1,params[0].memref.size);
    TEE_Free(r1);
    r1=NULL;
    return TEE_SUCCESS;
}


TEE_Result TA_InvokeCommandEntryPoint(void __unused *Session,
                                      uint32_t command,
                                      uint32_t param_types,
                                      TEE_Param params[4])
{
    switch (command)
    {
    case TA_SET_VEC:
        return TA_set_vec(param_types,params);
    case TA_SET_R1:
        return TA_set_r1(param_types,params);
    case TA_GET_VEC:
        return TA_get_vec(param_types,params);
    case TA_GET_R1:
        return TA_get_r1(param_types,params);
    default:
        EMSG("Command ID %x is not define", command);
        return TEE_ERROR_NOT_SUPPORTED;
        break;
    }
}

TEE_Result TA_CreateEntryPoint(void)
{
    // IMSG("Has been called");
    /* Nothing to do */
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
    // IMSG("Complete to call");
    /* Nothing to do */
}
TEE_Result TA_OpenSessionEntryPoint(uint32_t __unused param_types,
                                    TEE_Param __unused params[4],
                                    void __unused **session)
{
    // IMSG("Hello darknet!");
    /* Nothing to do */
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __unused *session)
{
    // IMSG("Goodbye darknet!");
    free_global_buffer();
}
