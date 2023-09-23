#ifdef ATTACK
#include <math.h>
#include <stdlib.h>
#include "attack_simulate.h"
#include <stdio.h>
#include<math.h>

const int ATTACK_LAYER=32;
#ifdef GPU
void attack_simulation(cl_command_queue que, cl_mem aMem,cl_mem bMem,cl_mem cMem,int m,int n,int k,int whichLayer){
    // static int flag=0;
    // if(flag) return;
    // if(whichLayer!=ATTACK_LAYER) return;
    //flag=1;
    int row=m;
    int col=n;
    cl_mem memory=cMem;

    float *a=(float*) malloc(sizeof(float)*row*col);
    cl_int status = clEnqueueReadBuffer(que,memory,CL_TRUE,0,sizeof(float)*row*col,a,0,NULL,NULL);
    if(status!=CL_SUCCESS){
        printf("read clmem of A error occured in attack_simulation function,the status is %d",status);
        free(a);
        return;
    }
    rand_inject_attack(a,row,col);
    status = clEnqueueWriteBuffer(que,memory,CL_TRUE,0,sizeof(float )*row*col,a,0,NULL,NULL);
    free(a);
    if(status!=CL_SUCCESS){
        printf("Write clmem of A error occured in attack_simulation function,the status is %d",status);
        return;
    }
    static int num=0;
    num++;
    printf("Times of attacks times: %d\n",num);
}

#endif
int* get_non_repeat_rand_nums(int range,int count){
    if(range<count) return NULL;
    char* map= calloc(range,1);
    int* ansArray= malloc(sizeof(int)*count);
    for(int i=0;i<count;i++){
        while (1){
            int val=rand()%range;
            if(map[val]!='a'){
                ansArray[i]=val;
                map[val]='a';
                break;
            }
        }
    }
    free(map);
    return ansArray;
}

void rand_inject_attack(float *c, int m, int n)
{
    float probability=0.02; //+0.003 each test *6
    float l2_norm=0.2; //0.2 0.5 1
    l2_norm*=l2_norm;
    int numC = m * n;
    float quantity=numC*probability;
    if(quantity<1) quantity=1.0;
    int l0_norm=(int) quantity;
    //printf("L0_Norm=%d ",l0_norm);
    l2_norm=sqrt(l2_norm/l0_norm);
    
    int* cites= get_non_repeat_rand_nums(numC,l0_norm);
    for(int i=0;i<l0_norm;i++){
        //c[cites[i]]=(float)rand()/RAND_MAX;
        if((rand()&1)==0) c[cites[i]]-=l2_norm;
        else c[cites[i]]+=l2_norm;
    }
    free(cites);
}

void rowhammer_attack(float *c,int m,int n){
    //This simulation only modify [probability] part of data and every data which be attack is only modified one bit.
    int numC= m*n;
    float probability=0.2;
    float quautity=numC*probability;
    if(quautity<1) quautity=1.0;
    int count=(int)quautity;
    int* cites= get_non_repeat_rand_nums(numC,count);
    for(int i=0;i<count;i++){
        //Because float is represented in memory as a 32-bit binary number
            int attackSite=rand()%32;
            int a=1;
            a=(a<<attackSite);
            int value=(*(int *)(c+cites[i]));
            value=(value^attackSite);
            c[cites[i]]=(*(float *)(&value));
    }
    free(cites);
}

void focuse_rand_Attack(float *c,int m,int n){
    float probability=0.2;
    float vp=(float)sqrt(probability);
    int row_range=(int)(m*(1-vp));
    int col_range=(int)(n*(1-vp));
    int brow=rand()%row_range;
    int bcol=rand()%col_range;
    int bindex=brow*n+bcol;
    for(int i=0;i<brow;i++){
        for(int j=0;j<bcol;j++){
            c[bindex+j]=(float)rand()/RAND_MAX;
        }
        bindex+=n;
    }
}

void focuse_rowhammer_attack(float *c,int m,int n){
    float probability=0.2;
    probability=(float)sqrt(probability);
    int row_range=(int)(m*(1-probability));
    int col_range=(int)(n*(1-probability));
    int brow=rand()%row_range;
    int bcol=rand()%col_range;
    int bindex=brow*n+bcol;
    for(int i=0;i<brow;i++){
        for(int j=0;j<bcol;j++){
            int attackSite=rand()%32;
            int a=1;
            a=(a<<attackSite);
            int value=(*(int *)(c+i));
            value=(value^attackSite);
            c[bindex+j]=(*(float *)(&value));
        }
        bindex+=n;
    }
}

#endif