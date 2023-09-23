#ifndef ATTACK_SIMULATE_H

#define ATTACK_SIMULATE_H
#ifdef ATTACK
#ifdef GPU
#include "opencl.h"
void attack_simulation(cl_command_queue que, cl_mem aMem,cl_mem bMem,cl_mem cMem,int m,int n,int k,int whichLayer);
#endif
void rand_inject_attack(float *c, int m, int n);
void rowhammer_attack(float *c,int m,int n);
void focuse_rand_Attack(float *c,int m,int n);
void focuse_rowhammer_attack(float *c,int m,int n);
#endif
#endif