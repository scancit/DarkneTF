#ifndef __VERIFICATION_TA_1_H__
#define __VERIFICATION_TA_1_H__
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
TEE_Result TA_copy_buffer(TEE_Param params[4]);
TEE_Result TA_pre_calculation(uint32_t types,TEE_Param params[4]);
TEE_Result TA_vector_difference(uint32_t types,TEE_Param params[4]);
TEE_Result TA_verify_by_IFA(uint32_t types,TEE_Param params[4]);
TEE_Result TA_connect_to_ta2(void);
void TA_close_session(void);
#endif