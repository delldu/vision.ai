
/************************************************************************************
***
***  Gene Expression Programming
***
***	Copyright 2012 Dell Du(18588220928@163.com), All Rights Reserved.
***
***	File Author: Dell, Wed Sep  5 09:38:26 CST 2012
***
************************************************************************************/


#ifndef _GENES_H
#define _GENES_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "matrix.h"

#define GENES_MAX_FUNCTIONS 64
#define GENES_MAX_CONSTANTS 64
#define GENES_MAX_FUNC_ARGC 8
#define GENES_MAX_NODES 256
#define GENES_BUFFER_SIZE 2048

// Gene function point
typedef double (*genef_t) (double *argv);

// Gene node
typedef struct {
	char *name;
	genef_t func;
	int argc, argv[GENES_MAX_FUNC_ARGC], level;		// Level for K-expressions 
	double value;
} GENE;

// Gene System
typedef struct {
	int head_size, pop_size;
	double max_fitness;

	// Function Set
	int function_count;
	GENE functions[GENES_MAX_FUNCTIONS];
	int max_func_argc;

	// Constant Set
	int constant_count;
	double constants[GENES_MAX_CONSTANTS];

	// Data Set
	MATRIX *dataset;
	
	// Evolution Probablity 
	double p_constant;		// 0.3
	double p_selection;	// 0.6 ~ 0.9
	double p_crossover;	// 0.4~0.99
	double p_crossover2;	// 0.2
	double p_mutation;		// 0.001 ~ 0.1
	double p_inversion;	// 0.001 ~ 0.1

	int len_matution;		// 2
	int len_inversion;		// 4

	// Best Chromosome
	int best_generation;
	char best_chromosome[GENES_BUFFER_SIZE];
	double best_fitness;
} GENES;

GENES *genes_create(int headsize, int popsize);
int genes_dataset(GENES *ges, char *filename);
int genes_addfunc(GENES *ges, char *name, int argc, genef_t func);
int genes_addconst(GENES *ges, double c);
int genes_evolution(GENES *ges, int generation);
void genes_destroy(GENES *ges);

#define genes_error(fmt, arg...)  do { \
		fprintf(stderr, "Error: %d(%s): " fmt "\n", __LINE__, __FILE__, ##arg); \
		exit(0); \
	} while (0);

int genes_test();

#if defined(__cplusplus)
}
#endif

#endif	// _GENES_H

