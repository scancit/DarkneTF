#ifndef __STORE_WEIGHT_TA_H__
#define __STORE_WEIGHT_TA_H__
TEE_Result write_weight_Object(uint32_t param_types,TEE_Param params[4]);
TEE_Result delete_weight_object(uint32_t params_type, TEE_Param params[4]);
TEE_Result read_weight_object(uint32_t params_type,TEE_Param params[4]);
#endif