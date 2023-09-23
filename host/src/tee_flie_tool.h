#ifndef __TEE_FILE_TOOL_H__
#define __TEE_FILE_TOOL_H__
#include "darknet.h"
#ifdef SECURITY
#include <tee_client_api.h>
//Call tee to store the weight file
void weight_to_tee_flie(network *net, char *filename, int start, int cutoff);
void tee_file_to_weight(network *net,char *filename,int start,int cutoff);
TEEC_Result cfg_to_tee_file(char *filename);
#endif
//int to string
void itoa(int num, char *str, int radix);
//Merge two float arrays into one
void splice_float_array(float *dst,float *src,int start,int src_num);
char *get_origin_filename(char *filename);

#endif