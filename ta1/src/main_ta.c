#include "darknet_ta1.h"
#include "util_ta.h"
#include "verification_ta.h"
#include "store_ta_weight_ta.h"
#include "sha256_ta.h"
TEE_Result TA_InvokeCommandEntryPoint(void __unused *Session,
                                      uint32_t command,
                                      uint32_t param_types,
                                      TEE_Param params[4])
{
    switch (command)
    {
    case TA_COPY_BUFFER:
        return TA_copy_buffer(params);
    case TA_PRE_CALCULATION:
        return TA_pre_calculation(param_types,params);
    case TA_VERIFY_BY_IFA:
        return TA_verify_by_IFA(param_types,params);
    case TA_VERIFY_DIFFRIENCE:
        return TA_vector_difference(param_types,params);
    case TA_WRITE_TEE_FILE:
        return write_weight_Object(param_types,params);
        break;
    case TA_DELETE_TEE_FILE:
        return delete_weight_object(param_types,params);
        break;
    case TA_READ_TEE_FILE:
        return read_weight_object(param_types,params);
        break;
    case TA_INIT_HASH_TABLE:
        return TA_init_one_hash_table(param_types,params);
    case TA_GENERATE_HA1:
        return TA_set_weight_sha256_hash(param_types,params);
    default:
        EMSG("Command ID %x is not define", command);
        return TEE_ERROR_NOT_SUPPORTED;
        break;
    }
}

TEE_Result TA_CreateEntryPoint(void)
{
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
}
TEE_Result TA_OpenSessionEntryPoint(uint32_t __unused param_types,
                                    TEE_Param __unused params[4],
                                    void __unused **session)
{
    TEE_UUID uuid=TA_DRAKNET_TA_1_UUID;
    IMSG("Opening session 0x%08x",uuid.timeLow);
    return TA_connect_to_ta2();
}

void TA_CloseSessionEntryPoint(void __unused *session)
{
    TEE_UUID uuid=TA_DRAKNET_TA_1_UUID;
    IMSG("Closing session 0x%08x",uuid.timeLow);
    TA_close_session();
}
