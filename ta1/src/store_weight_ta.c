#include "util_ta.h"
#include "store_ta_weight_ta.h"

TEE_Result write_weight_Object(uint32_t param_types,TEE_Param params[4]){
    const uint32_t exp_param_type=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                                                    TEE_PARAM_TYPE_MEMREF_INPUT,
                                                    TEE_PARAM_TYPE_VALUE_INPUT,
                                                    TEE_PARAM_TYPE_NONE);

    if(param_types!=exp_param_type) return TEE_ERROR_BAD_PARAMETERS;
    uint32_t id_sz=params[0].memref.size;
    char *obj_id=TEE_Malloc(id_sz,0);
    if(!obj_id) {
        EMSG("Malloc obj_id failed");
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_MemMove(obj_id,params[0].memref.buffer,id_sz);
    uint32_t data_size=params[1].memref.size;
    uint32_t which_layer=params[2].value.a;

    void *data=TEE_Malloc(data_size,0);
    if(!data) {
        TEE_Free(obj_id);
        EMSG("TEE malloc data failed");
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_MemMove(data,params[1].memref.buffer,data_size);

    TEE_Result res=write_raw_object(obj_id,id_sz,data,data_size);
    
    if(res!=TEE_SUCCESS){
        int layer=(int)which_layer;
        TEE_Free(obj_id);
        TEE_Free(data);
        EMSG("Fail to write %s in layer %d, res=%08x",obj_id,layer,res);
    }
    
    TEE_Free(obj_id);
    TEE_Free(data);
    return res;
}

TEE_Result delete_weight_object(uint32_t params_type, TEE_Param params[4]){
    uint32_t exp_params_type = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,TEE_PARAM_TYPE_NONE);
    if(exp_params_type!=params_type) return TEE_ERROR_BAD_PARAMETERS;
    uint32_t objid_size=params[0].memref.size;
    char *objid=TEE_Malloc(objid_size,0);
    if(!objid){
        EMSG("Malloc obj_id failed");
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    TEE_MemMove(objid,params[0].memref.buffer,objid_size);
    TEE_Result res = delete_object(objid,objid_size);
    TEE_Free(objid);
    return res;
}

TEE_Result read_weight_object(uint32_t params_type,TEE_Param params[4]){
    uint32_t exp_type = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_OUTPUT,TEE_PARAM_TYPE_NONE,TEE_PARAM_TYPE_NONE);
    if(exp_type!=params_type) return TEE_ERROR_BAD_PARAMETERS;
    uint32_t objidsz=params[0].memref.size;
    char *objid=TEE_Malloc(objidsz,0);
    TEE_MemMove(objid,params[0].memref.buffer,objidsz);
    uint32_t datasz=params[1].memref.size;
    void *data=TEE_Malloc(datasz,0);
    TEE_Result res = read_raw_object(objid,objidsz,data,&datasz);
    if(TEE_SUCCESS != res){
        TEE_Free(objid);
        TEE_Free(data);
        return res;
    }
    TEE_MemMove(params[1].memref.buffer,data,datasz);
    params[1].memref.size=datasz;
    TEE_Free(objid);
    TEE_Free(data);
    return res;
}
