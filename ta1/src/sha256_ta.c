#include "util_ta.h"
#include "sha256_ta.h"
static uint32_t HA1_table[100][8];
static uint32_t HB1[8];
TEE_Result TA_set_weight_sha256_hash(uint32_t params_type,TEE_Param params[4])
{
    static TEE_OperationHandle handle;
    static uint32_t is_alloc=0;
    TEE_Result res=TEE_SUCCESS;
    uint32_t exp_type=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,TEE_PARAM_TYPE_MEMREF_INPUT,
                                        TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_NONE);                                    
    if(exp_type!=params_type) return TEE_ERROR_BAD_PARAMETERS;
    uint32_t id_size=params[0].memref.size;
    uint32_t data_size=params[1].memref.size;
    uint32_t is_last=params[2].value.a;
    char objid[id_size];
    TEE_MemMove(objid,params[0].memref.buffer,id_size);
    objid[id_size]='\0';
    void *data=TEE_Malloc(data_size,0);
    if(data==NULL) return TEE_ERROR_OUT_OF_MEMORY;
    TEE_MemMove(data,params[1].memref.buffer,data_size);
    if(!is_alloc)
    {
        TEE_AllocateOperation(&handle,TEE_ALG_SHA256,TEE_MODE_DIGEST,0);
        is_alloc=1;
        if(res!=TEE_SUCCESS)
        {
            EMSG("TEE_AllocateOperation return error, res=0x%08x",res);
            return res;
        }
    }
    if(is_last)
    {
        uint32_t hash[8];
        uint32_t hash_size=32;
        res=TEE_DigestDoFinal(handle,data,data_size,hash,&hash_size);
        if(res!=TEE_SUCCESS)
        {
            EMSG("TEE_DigestDoFinal return error, res=0x%08x",res);
            return res;
        }
        TEE_FreeOperation(handle);
        is_alloc=0;
        TEE_Free(data);
        res=write_raw_object(objid,id_size,hash,hash_size); 
    }
    else
    {
        TEE_DigestUpdate(handle,data,data_size);
        TEE_Free(data);

    }
    return res;

}

TEE_Result TA_HB1_generator(uint32_t params_type,TEE_Param params[4])
{
    uint32_t exp_type=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_NONE,TEE_PARAM_TYPE_NONE);
    if(exp_type!=params_type) return TEE_ERROR_BAD_PARAMETERS;
    TEE_Result res;
    uint32_t buf_size=params[0].memref.size;
    uint32_t is_end=params[1].value.a;
    float *buffer=TEE_Malloc(buf_size,0);
    if(buffer==NULL)
    {
        EMSG("Malloc buffer failed");
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_MemMove(buffer,params[0].memref.buffer,buf_size);
    static TEE_OperationHandle handle;
    static uint32_t is_alloc=0;

    uint32_t hash_size=32;
    if(!is_alloc)
    {
        res=TEE_AllocateOperation(&handle,TEE_ALG_SHA256,TEE_MODE_DIGEST,0);
        is_alloc=1;
        if(res!=TEE_SUCCESS)
        {
            EMSG("TEE_AllocateOperation return error, res=0x%08x",res);
            return res;
        }
    }
    if(is_end)
    {
        res=TEE_DigestDoFinal(handle,buffer,buf_size,HB1,&hash_size);
        TEE_FreeOperation(handle);
        is_alloc=0;
        if(res!=TEE_SUCCESS)
        {
            EMSG("TEE_DigestDoFinal return error, res=0x%08x",res);
            return res;
        }

    }
    else
    {
        TEE_DigestUpdate(handle,buffer,buf_size);
    }
    TEE_Free(buffer);
    return TEE_SUCCESS;
}

uint32_t* get_HB1(void)
{
    //IMSG("Get HB1");
    return HB1;
}

uint32_t* get_hash_from_table(uint32_t which_layer)
{   
    return HA1_table[which_layer];
}
TEE_Result TA_init_one_hash_table(uint32_t params_type,TEE_Param params[4]){
    uint32_t exp_types=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,TEE_PARAM_TYPE_VALUE_INPUT,
                                        TEE_PARAM_TYPE_NONE,TEE_PARAM_TYPE_NONE);
    if(exp_types!=params_type) return TEE_ERROR_BAD_PARAMETERS;
    char objid[80];
    uint32_t id_size=params[0].memref.size;
    TEE_MemMove(objid,params[0].memref.buffer,id_size);
    objid[id_size]='\0';
    uint32_t which_layer=params[1].value.a;
    uint32_t hash_size=32;
    TEE_Result res=read_raw_object(objid,id_size,HA1_table[which_layer],&hash_size);
    return res;
}

uint32_t is_hash_same(uint32_t *hash1,uint32_t *hash2)
{
    for(uint32_t index=0;index<8;index++)
    {
        if(hash1[index]!=hash2[index]){
            return 0;
        }
    }
    return 1;
}


