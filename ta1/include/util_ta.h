#ifndef __UTIL_TA_1_H__
#define __UTIL_TA_1_H__
#include <tee_internal_api.h>
//#include <tee_internal_api_extensions.h>
#include "tee_internal_api_extensions.h"
#include <arm_neon.h>
#define PART_ELE_MAXNUM 262134;
TEE_Result write_raw_object(char *obj_id,uint32_t obj_id_size,void *obj_data,uint32_t obj_size);
TEE_Result read_raw_object(char *obj_id,uint32_t obj_id_size,void* data,uint32_t *data_size);
TEE_Result delete_object(char *obj_id,uint32_t obj_id_size);
TEE_Result is_exist_Object(char *obj_id,uint32_t id_sz);
float invSqrt(float x);
float *get_rand_vector(uint32_t size);
double *fvec_mul_fmatrix(float *rand_vec, float *a, uint32_t M, uint32_t K);
double *fvec_mul_fmatrix_neon1(float32_t *vec, float32_t *a, uint32_t M, uint32_t K);
double *randvec_mul_matrixA(float *rand_vec, float *matrixA,uint32_t M, uint32_t K);
double *dvec_mul_fmatrix(double *vec, float *b, uint32_t K, uint32_t N);
double *dvec_mul_fmatrix_neon(double *vec, float *b, uint32_t K, uint32_t N);
void vec_mul_sub_fmat_neon(float *vec,float *sub,double *result,uint32_t *brow,
                        uint32_t *bcol,uint32_t result_size,uint32_t sub_size);
uint32_t vector_diffirent(double *vec1, double *vec2, uint32_t M,uint32_t K,uint32_t N);
void itoa(int num, char *str, int radix);
void str_splicer(char *dest,uint32_t dest_size,const char *src, uint32_t src_size);
void splice_float_array(float *dst, float *src, int start, int src_num);
char* get_basename_from_objid(char* objid,uint32_t idSize);
#endif