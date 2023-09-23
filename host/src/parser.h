#ifndef PARSER_H
#define PARSER_H
#include "darknet.h"
#include "network.h"

void save_network(network net, char *filename);
void save_weights_double(network net, char *filename);
void transpose_matrix(float *a, int rows, int cols);
network parse_network_cfg_custom(char *filename, int batch, int time_steps);

#endif
