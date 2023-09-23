#include "util_ta.h"
#include <stdlib.h>
#include <string.h>
#include <arm_neon.h>
// TODO: Delete exist object before write object;
TEE_Result write_raw_object(char *obj_id, uint32_t obj_id_size, void *obj_data, uint32_t obj_size)
{
    IMSG("creat %s persistenObject......", obj_id);
    TEE_ObjectHandle object;
    TEE_Result res;
    uint32_t obj_data_flag = TEE_DATA_FLAG_ACCESS_READ |       /*Object can be read*/
                             TEE_DATA_FLAG_ACCESS_WRITE_META | /*Object can be deleted and renamed*/
                             TEE_DATA_FLAG_ACCESS_WRITE |
                             TEE_DATA_FLAG_SHARE_WRITE |
                             TEE_DATA_FLAG_OVERWRITE; /*delete existing object of same ID*/

    res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE, obj_id, obj_id_size, obj_data_flag,
                                     TEE_HANDLE_NULL, NULL, 0, &object);
    if (res != TEE_SUCCESS)
    {
        // TEE_CloseAndDeletePersistentObject1(object);
        EMSG("TEE_CreatePersistentObject failed 0x%08x", res);
        return res;
    }

    IMSG("Begin to write object data");
    res = TEE_WriteObjectData(object, obj_data, obj_size);
    IMSG("Complete to write %s",obj_id);
    if (res != TEE_SUCCESS)
    {
        EMSG("Write object data failed, res=0x%08x", res);
        TEE_CloseAndDeletePersistentObject1(object);
    }
    else
    {
        TEE_CloseObject(object);
    }
    return res;
}

TEE_Result read_raw_object(char *obj_id, uint32_t obj_id_size, void *data, uint32_t *data_size)
{
    TEE_ObjectHandle obj_handel;
    TEE_ObjectInfo obj_info;
    uint32_t read_bytes;
    TEE_Result res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE, obj_id, obj_id_size,
                                              TEE_DATA_FLAG_ACCESS_READ | TEE_DATA_FLAG_SHARE_READ, &obj_handel);
    if (res != TEE_SUCCESS)
    {
        EMSG("Failed to open persistent object, res=0x%08x", res);
        return res;
    }
    res = TEE_GetObjectInfo1(obj_handel, &obj_info);
    if (res != TEE_SUCCESS)
    {
        TEE_CloseObject(obj_handel);
        EMSG("Failed to get Object info, res=0x%08x", res);
        return res;
    }
    if (obj_info.dataSize > *data_size)
    {
        TEE_CloseObject(obj_handel);
        EMSG("The buffer is too small to fully read the %s file.",obj_id);
        EMSG("TA requires a buffer of %dB size, but only a buffer of %dB is provided",
                        obj_info.dataSize,*data_size);
        return TEE_ERROR_SHORT_BUFFER;
    }
    res = TEE_ReadObjectData(obj_handel, data, obj_info.dataSize, &read_bytes);
    if (res != TEE_SUCCESS || read_bytes != obj_info.dataSize)
    {
        EMSG("TEE read object data failed 0x%08x, only read %d / %d",
             res, read_bytes, obj_info.dataSize);
    }
    TEE_CloseObject(obj_handel);
    *data_size = read_bytes;
    return res;
}

TEE_Result delete_object(char *obj_id, uint32_t obj_id_size)
{
    TEE_ObjectHandle obj_hadle;
    TEE_Result res;
    res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE, obj_id, obj_id_size,
                                   TEE_DATA_FLAG_ACCESS_READ | TEE_DATA_FLAG_ACCESS_WRITE_META, &obj_hadle);
    if (res != TEE_SUCCESS)
    {
        EMSG("Failed to open persistent object while deleting, res=0x%08x", res);
        return res;
    }
    res = TEE_CloseAndDeletePersistentObject1(obj_hadle);
    if (res != TEE_SUCCESS)
    {
        EMSG("Failed to delete object, res=0x%08x", res);
    }
    else
        IMSG("Delete %s success", obj_id);
    return res;
}

TEE_Result is_exist_Object(char *obj_id, uint32_t id_sz)
{
    TEE_ObjectHandle obj_hadle;
    TEE_Result res;
    res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE, obj_id, id_sz,
                                   TEE_DATA_FLAG_ACCESS_READ | TEE_DATA_FLAG_SHARE_READ, &obj_hadle);
    if (res != TEE_SUCCESS)
    {
        return res;
    }
    TEE_CloseObject(obj_hadle);
    return res;
}

// float invSqrt(float x)
// {
//     float xhalf = 0.5f * x;
//     int i = *(int *)&x;
//     i = 0x5f375a86 - (i >> 1);
//     x = *(float *)&i;
//     x = x * (1.5f - xhalf * x * x);
//     x = x * (1.5f - xhalf * x * x);
//     x = x * (1.5f - xhalf * x * x);
//     return x;
// }
float *get_rand_vector(uint32_t size)
{
    float *rand_vec = TEE_Malloc(sizeof(float) * size, 0);
    if (!rand_vec)
    {
        EMSG("Malloc rand_vec of %fKB size error.", sizeof(float) * size / 1024.0);
        return NULL;
    }
    for (size_t index = 0; index < size; index++)
    {
        //rand_vec[index]=(float)rand() /RAND_MAX/8.0;
        rand_vec[index]=rand() /(float)RAND_MAX;
    }
    return rand_vec;
}

double *randvec_mul_matrixA(float *rand_vec, float *matrixA,uint32_t M, uint32_t K)
{
    uint32_t sizer=sizeof(double)*K;
    double *result=TEE_Malloc(sizer,0);
    if(!result){
        return NULL;
    }

    uint32_t tmp=0;
    for(uint32_t row=0;row<M;row++){
        //tmp=row*K;
        for(uint32_t col=0;col<K;col++){
            result[col]+=rand_vec[row]*(double)matrixA[tmp];
            tmp++;
        }
    }
    
    return result;
}

double *fvec_mul_fmatrix(float *rand_vec, float *a, uint32_t M, uint32_t K)
{
    double *result = TEE_Malloc(sizeof(double) * K, 0);
    if (!result){
        EMSG("Malloc ff_rec of %fKB size error.",sizeof(double)*K/1024.0);
        return NULL;
    }  
    ////TEE_MemFill(result, 0, sizeof(double) * K);

    uint32_t tmp = 0;
    // Caculate res_ra=rand_vec*A
    for (uint32_t row = 0; row < M; row++)
    {
        //tmp = row * K;
        for (uint32_t col = 0; col < K; col++)
        {
            result[col] += rand_vec[row] * (double)a[tmp];
            tmp++;
        }
    }
    return result;
}

double *fvec_mul_fmatrix_neon1(float32_t *vec, float32_t *a, uint32_t M, uint32_t K){
    double *result = TEE_Malloc(sizeof(double) * K, 0);
    //double *result = (double*)calloc(K,sizeof(double));
    if (!result){
        EMSG("Malloc ff_rec of %fKB size error.",sizeof(double)*K/1024.0);
        return NULL;
    } 
    uint32_t Kn=K>>1;
    float32_t *atmp;
    float32x2_t tmp;
    float64x2_t c0,a0;
    uint32_t idy;
    for(uint32_t x=0;x<M;++x){
        float64_t vtmp=vec[x];
        atmp=a+x*K;
        for(uint32_t y=0;y<Kn;++y){
            idy=y<<1;
            tmp=vld1_f32(atmp+idy);
            c0=vld1q_f64(result+idy);
            a0=vcvt_f64_f32(tmp);
            c0=vaddq_f64(c0,vmulq_n_f64(a0,vtmp));
            vst1q_f64(result+idy,c0);
        }
        if(K&1) result[K-1]+=vtmp*atmp[K-1];
    }
    return result;
}

double *dvec_mul_fmatrix(double *vec, float *b, uint32_t K, uint32_t N)
{
    double *r1 = TEE_Malloc(sizeof(double) * N, 0);
    if (!r1)
    {
        EMSG("Malloc df_vec of %fKB size error.", sizeof(double) * N / 1024.0);
        return NULL;
    }

    //TEE_MemFill(r1, 0, sizeof(double) * N);
    uint32_t tmp = 0;
    for (uint32_t row = 0; row < K; row++)
    {
        //tmp = row * N;
        for (uint32_t col = 0; col < N; col++)
        {
            r1[col] += vec[row] * b[tmp];
            tmp++;
        }
    }
    return r1;
}

double *dvec_mul_fmatrix_neon(double *vec, float *b, uint32_t K, uint32_t N){
    double *result = TEE_Malloc(sizeof(double) * N, 0);
    //double *result=(double*)calloc(N,sizeof(double));
    if (!result)
    {
        //EMSG("Malloc df_vec of %fKB size error.", sizeof(double) * N / 1024.0);
        return NULL;
    }
    uint32_t tmp=0;
    uint32_t nn=N-1;
    float64x2_t r0,b0;
    float32x2_t btmp;
    float64_t vtmp;
    for(uint32_t row=0;row<K;++row){
        vtmp=vec[row];
        for(uint32_t col=0; col<nn; col+=2,tmp+=2){
            r0=vld1q_f64(result+col);
            btmp=vld1_f32(b+tmp);
            b0=vcvt_f64_f32(btmp);
            r0=vaddq_f64(r0,vmulq_n_f64(b0,vtmp));
            vst1q_f64(result+col,r0);
        }
        if(N&1){
            result[N-1]+=vtmp*b[tmp];
            ++tmp;
        }
    }

    return result;
}

void vec_mul_sub_fmat_neon(float *vec,float *sub,double *result,uint32_t *brow,
                        uint32_t *bcol,uint32_t result_size,uint32_t sub_size){
    float64x2_t r0,s0;
    float32x2_t ts0;
    uint32_t sn=sub_size-1;
    uint32_t rn=result_size-1;
    uint32_t flag=result_size&1;
    float32_t *end=sub+sn;
    double ve=vec[*brow];
    for(float *p=sub;p<end;p=p+2){
        r0=vld1q_f64(result+*bcol);
        ts0=vld1_f32(p);
        s0=vcvt_f64_f32(ts0);
        r0=vaddq_f64(r0,vmulq_n_f64(s0,ve));
        vst1q_f64(result+*bcol,r0);
        *bcol=*bcol+2;
        if(*bcol>=rn){
            if(flag){
                result[*bcol]+=ve* *(p+2);
                p=p+1;
            }
            *bcol=0;
            ve=vec[++*brow];
        }
    }
}

uint32_t vector_diffirent(double *vec1, double *vec2, uint32_t M,uint32_t K,uint32_t N)
{
    double sub = 0;
    uint32_t flag = 1;
    //int max=0;
    float lamb=0.7;
    float range=M*K/16777216.0*lamb;
    for (uint32_t index = 0; index < N; index++)
    {
        sub = vec1[index]>vec2[index]?vec1[index]-vec2[index]:vec2[index]-vec1[index];
        //int ppp=(int)(sub*1000000);
        ///max=max>=ppp?max:ppp;
        if (sub > range)
        {    
            //IMSG("M=%d,K=%d,N=%d, Difference is %d*10^-6",M,K,N,ppp);
            flag = 0;
            break;
        }
    }
    //IMSG("M=%d K=%d N=%d max_diff=%d*10^-6",M,K,N,max);
    return flag;
}

void itoa(int num, char *str, int radix)
{
    char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned unum;
    int i = 0, j, k;
    if (radix == 10 && num < 0)
    {
        unum = (unsigned)-num;
        str[i++] = '-';
    }
    else
        unum = (unsigned)num;
    do
    {
        str[i++] = index[unum % (unsigned)radix];
        unum /= radix;
    } while (unum);
    str[i] = '\0';
    if (str[0] == '-')
        k = 1;
    else
        k = 0;

    char temp;
    for (j = k; j <= (i - 1) / 2; j++)
    {
        temp = str[j];
        str[j] = str[i - 1 + k - j];
        str[i - 1 + k - j] = temp;
    }
}

void str_splicer(char *dest, uint32_t dest_size, const char *src, uint32_t src_size)
{
    if (src_size == 0)
        return;
    uint32_t j = dest_size;
    for (uint32_t i = 0; i < src_size; i++, j++)
    {
        dest[j] = src[i];
    }
    dest[j] = '\0';
}
void splice_float_array(float *dst, float *src, int start, int src_num)
{
    int i, j;
    for (i = start, j = 0; j < src_num; ++i, ++j)
    {
        dst[i] = src[j];
    }
}

char* get_basename_from_objid(char* objid,uint32_t idSize){
    char* ans=TEE_Malloc(50,0);
    //TEE_MemFill(ans,0,50);
    for(uint32_t i=0;i<idSize;i++){
        if(objid[i]!='.'){
            ans[i]=objid[i];
        }
        else{
            break;
        }
    }
    return ans;
}

