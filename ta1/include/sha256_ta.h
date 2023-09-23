#ifndef __SHA256_TA_H__
#define __SHA256_TA_H__
#include <tee_api_defines.h>
#include <tee_api_defines_extensions.h>
TEE_Result TA_set_weight_sha256_hash(uint32_t params_type,TEE_Param params[4]);
uint32_t* get_hash_from_table(uint32_t which_layer);
TEE_Result TA_init_one_hash_table(uint32_t params_type,TEE_Param params[4]);
uint32_t is_hash_same(uint32_t *hash1,uint32_t *hash2);
uint32_t* get_HB1(void);
TEE_Result TA_HB1_generator(uint32_t params_type,TEE_Param params[4]);
#endif