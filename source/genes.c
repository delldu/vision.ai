
/************************************************************************************
***
***  Gene Expression Programming
***
***	Copyright 2012 Dell Du(dellrunning@gmail.com), All Rights Reserved.
***
***	File Author: Dell, Wed Sep  5 09:38:35 CST 2012
***
************************************************************************************/

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "genes.h"

// Chromosome
typedef struct {
	int count;
	GENE node[GENES_MAX_NODES];
	char buff[GENES_BUFFER_SIZE];		// Gene name buffer
} CHROMOSOME;
static CHROMOSOME __temp_chromosome__;

typedef struct {
	int count;
	char **chromo;
	double *fitness;
} POPULATION;

// [a, b] 
static int __random_int(int a, int b)
{
	return (rand() % (b - a + 1) + a);
}

// > pb ? 
static int __with_probable(double pb)
{
	double pbval = 10000.0f * pb;
	int val = (int)pbval;
	return __random_int(0, 10000) < val;
}

// Term: constant(#1) + dataset(x1)
static int __random_term(GENES *ges, char *buf, int maxsize)
{
	int r, len;
	char temp[128];

	len = ges->constant_count + ges->dataset->n - 1;
	r = __random_int(0, len - 1);

	// if (ges->constant_count > 0 && r < (len * ges->p_constant)) {
	if (ges->constant_count > 0 && r < ges->constant_count && __with_probable(ges->p_constant)) {
		// r %= ges->constant_count;
		snprintf(temp, sizeof(temp) - 1, "#%d", r);
	}
	else {
		// r -= ges->constant_count;
		r %= ges->dataset->n - 1;
		snprintf(temp, sizeof(temp) - 1, "x%d", r);
	}

	len = strlen(temp);
	if (len > maxsize) {
		syslog_error("Len (%d) > maxsize(%d)", len, maxsize);
		return -1;
	}

	strcpy(buf, temp);

	return len;
}

// Head: function + constant(#1) + dataset(x1)
static int __random_head(GENES *ges, char *buf, int maxsize)
{
	int r, len;

	len = ges->function_count + ges->dataset->n - 1;
	r = __random_int(0, len - 1);
	if (r >= ges->function_count) 
		return __random_term(ges, buf, maxsize);

	// Function random selection
	len = strlen(ges->functions[r].name);
	if (maxsize <= len) {
		syslog_error("Buffer size is too small (%d <= %d)", maxsize, len);
		return -1;
	}
	strcpy(buf, ges->functions[r].name);

	return len;
}

int __random_chromosome(GENES *ges, char *buf, int maxsize)
{
	int i, r, t, len = 0;
	char genbuf[128];

	t = ges->head_size * (ges->max_func_argc - 1) + 1;
	for (i = 0; i < ges->head_size; i++) {
		if ((r = __random_head(ges, genbuf, sizeof(genbuf))) == -1) {
			syslog_error("Generate head genes.");
			return -1;
		}
		if (len + r + 1 >= maxsize) {
			syslog_error("Len + r + 1(%d) >= maxsize(%d)", len + r + 1, maxsize);
			return -1;
		}
		len += sprintf(buf + len, "%s.", genbuf);
	}
	
	for (i = 0; i < t; i++) {
		if ((r = __random_term(ges, genbuf, sizeof(genbuf))) == -1) {
			syslog_error("Generate term genes.");
			return -1;
		}
		if (len + r + 1 >= maxsize) {
			syslog_error("Len + r + 1(%d) >= maxsize(%d)", len + r + 1, maxsize);
			return -1;
		}
		len += sprintf(buf + len, "%s.", genbuf);
	}
	buf[len - 1] = '\0';

	return len;
}

static POPULATION *__population_create(GENES *ges)
{
	int i;
	char buf[256];

	POPULATION *pop = malloc(sizeof(POPULATION));
	if (! pop)
		goto memfail;

	pop->chromo = malloc(ges->pop_size * sizeof(char *));
	if (! pop->chromo)
		goto memfail;

	for(i = 0; i < ges->pop_size; i++) {
		__random_chromosome(ges, buf, sizeof(buf));
		pop->chromo[i] = strdup(buf);
		if (! pop->chromo[i])
			goto memfail;
	}

	pop->fitness = malloc(ges->pop_size * sizeof(double));
	if (! pop->fitness)
		goto memfail;

	pop->count = ges->pop_size;
	return pop;

memfail:
	syslog_error("Allocate memory.");
	return NULL;
}


static void __population_destroy(POPULATION *pop)
{
	int i;
	if (! pop)
		return;
	
	for (i = 0; i < pop->count; i++)
		free(pop->chromo[i]);
	free(pop->chromo);
	free(pop->fitness);
	
	free(pop);
}


static double __system_func_add(double *argv)
{
	return argv[0] + argv[1];
}

static double __system_func_sub(double *argv)
{
	return argv[0] - argv[1];
}

static double __system_func_mul(double *argv)
{
	return argv[0] * argv[1];
}

static double __system_func_div(double *argv)
{
	return fabs(argv[1]) > 0.000001 ? argv[0] / argv[1] : 0.0f;
}

static double __system_func_abs(double *argv)
{
	return fabs(argv[0]);
}

static double __system_func_sin(double *argv)
{
	return sin(argv[0]);
}

static double __system_func_cos(double *argv)
{
	return cos(argv[0]);
}

static double __system_func_log(double *argv)
{
	return log(argv[0]);
}

static double __system_func_exp(double *argv)
{
	return exp(argv[0]);
}

static double __system_func_if(double *argv)
{
	return (fabs(argv[0]) > 0.0001)? argv[1] : argv[2];
}


static GENE __system_function_table[GENES_MAX_FUNCTIONS] = {
	{"+",  __system_func_add, 2 },	/* 1 */
	{"-",  __system_func_sub, 2 },
	{"*",  __system_func_mul, 2 },
	{"/",  __system_func_div, 2 },
	{"abs", __system_func_abs, 1 },
	{"sin", __system_func_sin, 1 },
	{"cos", __system_func_cos, 1 },
	{"log", __system_func_log, 1 },
	{"exp", __system_func_exp, 1 },
	{"if", __system_func_if, 3 },		/* 10 */
};
static int __system_function_count = 10; // Should reference __system_function_table

static int __system_function_cmp(const void *p1, const void *p2)
{
	GENE *f1 = (GENE *)p1;
	GENE *f2 = (GENE *)p2;
	return (f1 && f2)? strcmp(f1->name, f2->name) : -1;
}

static GENE *__system_function_search(char *name)
{
	GENE key;
	static int __system_function_sorted = 0;

	if (! __system_function_sorted) {
		qsort(__system_function_table, __system_function_count, sizeof(GENE), __system_function_cmp);
		__system_function_sorted = 1;
	}
	
	key.name = name;
	return bsearch(&key, __system_function_table, __system_function_count, sizeof(GENE), __system_function_cmp);
}

// Karva Expression
static int __kexpression_build(char *some)
{
	GENE *g;
	char *tv[GENES_MAX_NODES];
	int i, j, n, s, start, level, nset0, nset1;
	CHROMOSOME *cs = &__temp_chromosome__;
	int set0[GENES_MAX_NODES], set1[GENES_MAX_NODES];
	
	if (strlen(some) >= GENES_BUFFER_SIZE) {
		syslog_error("Chromosome length is too long.");
		return -1;
	}

	memset(cs, 0, sizeof(CHROMOSOME));
	strcpy(cs->buff, some);

	n = get_token(cs->buff, '.', GENES_MAX_NODES, tv);
	for (i = 0; i < n; i++) {
		g = __system_function_search(tv[i]);
		if (g) {
			cs->node[i].name = tv[i];
			cs->node[i].argc = g->argc;
			cs->node[i].func = g->func;
		}
		else {	// #? or x?
			if (*tv[i] != '#' && *tv[i] != 'x')
				return -1;
			cs->node[i].name = tv[i];
		}
	}

	// Calculate Level
	start = 0;
	cs->node[0].level = level = 1;  // level start from 1  !
	while (1) {
		s = 0;
		for (i = start; i < n && cs->node[i].level > 0; i++) {
			if (cs->node[i].level == level)
				s += cs->node[i].argc;
		}
		if (s == 0)
			break;
		start = i;
		for (j = 0; j < s; j++)
			cs->node[j + start].level = (level + 1);
		level++;
	}
	cs->count = i;

	/*
		set0 = select_node(level);
		set1 = select_node(level + 1);
		for x in set0:
			select x->argc nodes from set1 as function argument list, skip to next x->argc ...
	*/
	for (i = 1; i < level; i++) {
		nset0 = nset1 = 0;
		for (j = 0; j < cs->count; j++) {
			if (cs->node[j].level == i) {
				set0[nset0] = j; nset0++;
			}
			if (cs->node[j].level == i + 1) {
				set1[nset1] = j; nset1++;
			}
		}
		start = 0;
		for (j = 0; j < nset0; j++) {
			s = cs->node[set0[j]].argc;
			for (n = 0; n < s; n++)
				cs->node[set0[j]].argv[n] = set1[start + n];
			start += s;
		}
	}

	return level;
}

static void __kexpression_output(int detail)
{
	int i, j, k;
	GENE *g;	
	CHROMOSOME *cs = &__temp_chromosome__;

	printf("Formula:\n");
	if (detail) {
		for (i = cs->count - 1; i >= 0; i--) {
			g = &cs->node[i];

			printf("  f%d = %s", i, g->name);
			if (g->func) {
				printf("(");
				for (j = 0; j < g->argc; j++)
					printf("f%d%s",  g->argv[j], j < g->argc - 1? ", " : "");
				printf("), ");
			}
			else 
				printf(", ");
			// printf("  level: %d, ", g->level);
			printf("  value: %f\n", g->value);
		}
	}
	else {
		for (i = cs->count - 1; i >= 0; i--) {
			g = &cs->node[i];
			if (! g->func)
				continue;

			printf("  f%d = %s", i, g->name);
			printf("(");
			for (j = 0; j < g->argc; j++) {
				k = g->argv[j];
				if (cs->node[k].argc == 0)
					printf("%s%s",  cs->node[k].name, j < g->argc - 1? ", " : "");
				else
					printf("f%d%s",  g->argv[j], j < g->argc - 1? ", " : "");
			}
			printf(")\n");
		}
	}
}


// Make sure row is valid in dataset ???
static double __kexpression_value(GENES *ges, int row)
{
	int i, j, k;
	char *name;
	double argv[GENES_MAX_FUNC_ARGC];
	CHROMOSOME *cs = &__temp_chromosome__;

	if (! ges) {
		syslog_error("Ges == NULL");
		return 0.0f;
	}

	for (i = cs->count - 1; i >= 0; i--) {
		if (cs->node[i].func) {
			for (j = 0; j < cs->node[i].argc; j++) {
				k = cs->node[i].argv[j];
				argv[j] = cs->node[k].value;
			}
			cs->node[i].value = cs->node[i].func(argv);
		}
		else {
			name = cs->node[i].name;
			if (*name != '#' && *name != 'x')
				return 0.0f;
			k = atoi(name + 1);
			if (*name == '#')
				cs->node[i].value = ges->constants[k];
			else {
				if (row < 0 || row >= ges->dataset->m || k < 0 || k >= ges->dataset->n) {
					syslog_error("Row or Col is in valid range.");
					cs->node[i].value = 0.0f;
				}
				else
					cs->node[i].value = ges->dataset->me[row][k];
			}
		}
	}

	return cs->node[0].value;
}

static double __kexpression_error(GENES *ges, char *some)
{
	int i, n;
	double fit;
	
	if (__kexpression_build(some) == -1)
		return 0.0f;

	fit = 0.0f;
	n = ges->dataset->n;
	if (ges->dataset->m > 0) {
		for (i = 0; i < ges->dataset->m; i++)
			fit += fabs(ges->dataset->me[i][n - 1] - __kexpression_value(ges, i));
		fit /= ges->dataset->m;
	}
	
	return fit;
}

static int __genes_check(GENES *ges)
{
	if (! ges) {
		syslog_error("BAD GENES (= NULL)\n");
		return 0;
	}

	if (ges->head_size < 1) {
		syslog_error("Head Size: %d. \n", ges->head_size);
		return 0;
	}

	if (ges->pop_size < 1) {
		syslog_error("Population Size: %d. \n", ges->pop_size);
		return 0;
	}

	if (ges->function_count < 1) {
		syslog_error("Funtion Count: %d. \n", ges->function_count);
		return 0;
	}

	if (! matrix_valid(ges->dataset)) {
		syslog_error("Dataset Invalid.\n");
		return 0;
	}

	return 1;
}

// Keep Best Invidiuals
static int __genes_save_best(GENES *ges, POPULATION *pop, int generation)
{
	int k, best_index;
	double best_fitness;

	// Save Best Chromosome
	best_index = -1;
	best_fitness = ges->best_fitness;
	for (k = 0; k < pop->count; k++) {
		if (pop->fitness[k] > best_fitness) {
			best_index = k;
			best_fitness = pop->fitness[k];
		}
	}
	if (best_index >= 0) {
		ges->best_generation = generation;
		strcpy(ges->best_chromosome, pop->chromo[best_index]);
		ges->best_fitness = pop->fitness[best_index];
	}

	return 0;
}


static void __genes_evaluation(GENES *ges, POPULATION *pop)
{
	int i;
	double e;

	for (i = 0; i < pop->count; i++) {
		e = __kexpression_error(ges, pop->chromo[i]);
		pop->fitness[i] = ges->max_fitness - e;
	}
}


// Selection/Replication
static int __genes_selection(GENES *ges, POPULATION *pop)
{
	int i, j, k, t, select[GENES_MAX_NODES];

	// Tournament selection
	memset(select, 0, GENES_MAX_NODES * sizeof(int));
	for (k = 0; k < pop->count; k++) {
		i = __random_int(0, pop->count - 1);
		j = __random_int(0, pop->count - 1);
		if (pop->fitness[i] < pop->fitness[j]) {
			t = i; i = j; j = t;	// swap i, j
 		}
		
		if (__with_probable(ges->p_selection))
			select[i]++;
		else
			select[j]++;
	}

	// Re-distribution !!!
	for (k = 0; k < pop->count; k++) {
		while (select[k] > 1) {
			for (j = 0; j < pop->count; j++) {
				if (select[j] == 0)
					break;
			}
			if (j < pop->count) {
				free(pop->chromo[j]);
				pop->chromo[j] = strdup(pop->chromo[k]);
				select[k]--;
				select[j]++;
			}
			else {	// Never reach here !!!
				syslog_error("Never !!!");
				return -1;
			}
		}
	}

	return 0;
}

// Crossover/One Point Recombine
static int __genes_crossover(GENES *ges, POPULATION *pop)
{
	int index, n1, n2, j, len, pos, tail;
	char *t1[GENES_MAX_NODES], *t2[GENES_MAX_NODES];
	char buf1[GENES_BUFFER_SIZE], buf2[GENES_BUFFER_SIZE];

	tail = ges->head_size * (ges->max_func_argc - 1) + 1;

	index = 0;
	while (index < pop->count - 2) {
		// Start crossover
		if (__with_probable(ges->p_crossover)) {
			pos = __random_int(1, ges->head_size + tail - 2);

			n1 = get_token(pop->chromo[index], '.', GENES_MAX_NODES, t1);
			n2 = get_token(pop->chromo[index + 1], '.', GENES_MAX_NODES, t2);
			assert(n2 >= 0);

			for (len = 0, j = 0; j < pos; j++)
				len += sprintf(buf1 + len, "%s.", t2[j]);
			for (j = pos; j < n1; j++)
				len += sprintf(buf1 + len, "%s.", t1[j]);
			buf1[len - 1] = '\0';

			for (len = 0, j = 0; j < pos; j++)
				len += sprintf(buf2 + len, "%s.", t1[j]);
			for (j = pos; j < n1; j++)
				len += sprintf(buf2 + len, "%s.", t2[j]);
			buf2[len - 1] = '\0';

			free(pop->chromo[index]);
			pop->chromo[index] = strdup(buf1);
			free(pop->chromo[index + 1]);
			pop->chromo[index + 1] = strdup(buf2);
		}

		// Loop !!!
		index += 2;
	}

	return 0;
}


// Crossover/Two Point Recombine
static int __genes_crossover2(GENES *ges, POPULATION *pop)
{
	int index, n1, n2, j, len, pos1, pos2, tail;
	char *t1[GENES_MAX_NODES], *t2[GENES_MAX_NODES];
	char buf1[GENES_BUFFER_SIZE], buf2[GENES_BUFFER_SIZE];

	tail = ges->head_size * (ges->max_func_argc - 1) + 1;

	index = 0;
	while (index < pop->count - 2) {
		// Start crossover
		if (__with_probable(ges->p_crossover2)) {
			n1 = __random_int(1, ges->head_size + tail - 2);
			n2 = __random_int(1, ges->head_size + tail - 2);
			pos1 = MIN(n1, n2);
			pos2 = MAX(n1, n2);
			if (pos1 == pos2)
				continue;

			n1 = get_token(pop->chromo[index], '.', GENES_MAX_NODES, t1);
			n2 = get_token(pop->chromo[index + 1], '.', GENES_MAX_NODES, t2);

			// Create buffer1
			for (len = 0, j = 0; j < pos1; j++)
				len += sprintf(buf1 + len, "%s.", t1[j]);
			for (j = pos1; j < pos2; j++)
				len += sprintf(buf1 + len, "%s.", t2[j]);
			for (j = pos2; j < n1; j++)
				len += sprintf(buf1 + len, "%s.", t1[j]);
			buf1[len - 1] = '\0';

			// Create buffer2
			for (len = 0, j = 0; j < pos1; j++)
				len += sprintf(buf2 + len, "%s.", t2[j]);
			for (j = pos1; j < pos2; j++)
				len += sprintf(buf2 + len, "%s.", t1[j]);
			for (j = pos2; j < n2; j++)
				len += sprintf(buf2 + len, "%s.", t2[j]);
			buf2[len - 1] = '\0';

			free(pop->chromo[index]);
			pop->chromo[index] = strdup(buf1);
			free(pop->chromo[index + 1]);
			pop->chromo[index + 1] = strdup(buf2);
		}

		// Loop !!!
		index += 2;
	}

	return 0;
}


// Mutation 
static int __genes_mutation(GENES *ges, POPULATION *pop)
{
	int i, j, n, pos, tail;
	char *tv[GENES_MAX_NODES], holder[GENES_MAX_NODES];
	char buf[GENES_BUFFER_SIZE], genbuf[128];

	tail = ges->head_size * (ges->max_func_argc - 1) + 1;
	for (i = 0; i < pop->count; i++) {
		if (! __with_probable(ges->p_mutation))
			continue;
		
		n = get_token(pop->chromo[i], '.',  GENES_MAX_NODES, tv);
		memset(holder, 0, GENES_MAX_NODES);
		for (j = 0; j < ges->len_matution; j++) {
			pos = __random_int(0, ges->head_size + tail - 1);
			holder[pos] =  (pos < ges->head_size)? 1 : 2;
		}
		pos = 0;
		for (j = 0; j < n; j++) {
			if (holder[j] > 0) {
				if (holder[j] == 1)
					__random_head(ges, genbuf, sizeof(genbuf));
				else
					__random_term(ges, genbuf, sizeof(genbuf));
				pos += sprintf(buf + pos, "%s.", genbuf);
 			}
			else {
				pos += sprintf(buf + pos, "%s.", tv[j]);
			}
		}
		buf[pos - 1] = '\0';

		free(pop->chromo[i]);
		pop->chromo[i] = strdup(buf);
	}

	return 0;
}


// Inversion
static int __genes_inversion(GENES *ges, POPULATION *pop)
{
	int i, j, n, pos, tail;
	char *tv[GENES_MAX_NODES], *temp;
	char buf[GENES_BUFFER_SIZE];

	tail = ges->head_size * (ges->max_func_argc - 1) + 1;

	//make sure inversion could work well !
	ges->len_inversion = MIN(ges->len_inversion, ges->head_size);
	ges->len_inversion = MIN(ges->len_inversion, tail);

	for (i = 0; i < pop->count; i++) {
		if (! __with_probable(ges->p_inversion))
			continue;

		pos = __random_int(0, ges->head_size + tail - 1 - ges->len_inversion);
		
		// [ start ... head_size ... end]
		if (pos < ges->head_size && pos + ges->len_inversion >= ges->head_size) {
			if (pos >= ges->head_size - ges->len_inversion/2)
				pos = ges->head_size; // inversion on tail part
			else
				pos = MAX(0, ges->head_size - ges->len_inversion - 1); //  inversion on head part
		}
		
		n = get_token(pop->chromo[i], '.',  GENES_MAX_NODES, tv);
		for (j = 0; j < ges->len_inversion/2; j++) { 	// Swap 
			temp = tv[pos + i];
			tv[pos + i] = tv[pos + ges->len_inversion - i];
			tv[pos + ges->len_inversion - i] = temp;
		}
		pos = 0;
		for (j = 0; j < n; j++)
			pos += sprintf(buf + pos, "%s.", tv[j]);
		buf[pos - 1] = '\0';

		free(pop->chromo[i]);
		pop->chromo[i] = strdup(buf);
	}

	return 0;
}


// Report 
static void __genes_report(GENES *ges)
{
	int i;

	printf("Gene Expression Programming Report\n");
	printf("-----------------------------------------------------\n");
	printf("Head Size: %d\n", ges->head_size);
	printf("Population Size: %d\n", ges->pop_size);
	printf("Maximum Fitness: %f\n", ges->max_fitness);
	printf("Function List: ");
	for (i = 0; i < ges->function_count; i++)
		printf("%s(%d)%s", ges->functions[i].name, ges->functions[i].argc, (i < ges->function_count- 1)? ",": "");
	printf("\n");
	printf("Constant List: ");
	for (i = 0; i < ges->constant_count; i++)
		printf("%lf%s", ges->constants[i], (i < ges->constant_count - 1)? ",": "");
	printf("\n");
	printf("Evolution Probablity & Length:\n");
	printf("  Constant: %f\n", ges->p_constant);
	printf("  Selection: %f\n", ges->p_selection);
	printf("  Crossover: %f\n", ges->p_crossover);
	printf("  Mutation: %f, Length: %d \n", ges->p_mutation, ges->len_matution);
	printf("  Inversion: %f, Length: %d \n", ges->p_inversion, ges->len_inversion);
	printf("Best Chromosome:\n");
	printf("  Generation: %d, Chromosome: %s, Fitness: %f\n", ges->best_generation,  ges->best_chromosome, 	ges->best_fitness);

	__kexpression_build(ges->best_chromosome);
	__kexpression_output(0);
	printf("-----------------------------------------------------\n");
}

GENES *genes_create(int headsize, int popsize)
{
	GENES *ges;

	srand(time(NULL));

	ges = calloc(1, sizeof(GENES));
	if (ges) {
		ges->head_size = headsize;
		ges->pop_size = popsize;
		ges->max_fitness = 1000.0f;

		ges->p_constant = 0.3f;
		ges->p_selection = 0.6f;
		ges->p_crossover = 0.4f;
		ges->p_crossover2 = 0.2f;
		ges->p_mutation = 0.1f;
		ges->p_inversion = 0.2f;		

		ges->len_matution = 2;
		ges->len_inversion = 4;
	}

	return ges;
}


int genes_dataset(GENES *ges, char *filename)
{
	if (! ges || ! filename) {
		syslog_error("Ges == NULL || filename == NULL");
		return -1;
	}
	ges->dataset = matrix_load(filename);
	return (ges->dataset)? 1 : 0;
}

int genes_addfunc(GENES *ges, char *name, int argc, genef_t func)
{
	if (! ges || ges->function_count >= GENES_MAX_FUNCTIONS) {
		syslog_error("Ges == NULL || function count >= %d", GENES_MAX_FUNCTIONS);
		return -1;
	}
	
	ges->functions[ges->function_count].name = name;
	if (func == NULL) {
		GENE *g = __system_function_search(name);
		if (g) {
			ges->functions[ges->function_count].argc = g->argc;
			ges->functions[ges->function_count].func = g->func;
			goto success;		
		}
		syslog_error("%s is not system function.\n", name);
		return -1;	// NO function !!!
	}
	else {
		ges->functions[ges->function_count].argc = argc;
		ges->functions[ges->function_count].func = func;
	}

success:
	if (ges->functions[ges->function_count].argc > ges->max_func_argc)
		ges->max_func_argc = ges->functions[ges->function_count].argc;
	ges->function_count++;

	return 0;
}

int genes_addconst(GENES *ges, double c)
{
	if (! ges || ges->constant_count >= GENES_MAX_CONSTANTS) {
		syslog_error("Ges == NULL || constant count >= %d", GENES_MAX_CONSTANTS);
		return -1;
	}
	
	ges->constants[ges->constant_count] = c;
	ges->constant_count++;
	return 0;
}

int genes_evolution(GENES *ges, int generation)
{
	int  t;
	POPULATION *pop;

	if (! __genes_check(ges))
		return -1;

	pop = __population_create(ges);
	if (! pop) {
		syslog_error("Create population.");
		return -1;
	}

	for(t = 0; t < generation; t++) {
		__genes_evaluation(ges, pop);
		__genes_save_best(ges, pop, t);

		if (fabs(ges->best_fitness - ges->max_fitness) < 0.001)
			break;

		__genes_selection(ges, pop);
		__genes_mutation(ges, pop);
		__genes_inversion(ges, pop);
		__genes_crossover(ges, pop);
		__genes_crossover2(ges, pop);	
	}

	__genes_report(ges);	
	__population_destroy(pop);

	return 0;
}

void genes_destroy(GENES *ges)
{
	if (ges) {
		matrix_destroy(ges->dataset);
		free(ges);
	}
}

static void make_testdb()
{
	int i;
	double x, y;
	MATRIX *mat = matrix_create(20, 2);

	srand(time(NULL));
	
	for (i = 0; i < mat->m; i++) {
		x = rand() % 1000 - 500.0f;
		y = x * x + x + 1.0f;
		mat->me[i][0] = x;
		mat->me[i][1] = y;
	}
	matrix_save(mat, "dataset.txt");
	matrix_destroy(mat);
}

int genes_test()
{
	make_testdb();

	GENES *ge = genes_create(8, 100);
	if (! ge) {
		syslog_error("Create genes.");
		return -1;
	}

	genes_addfunc(ge, "+", 2, NULL);
	genes_addfunc(ge, "-", 2, NULL);
	genes_addfunc(ge, "*", 2, NULL);
	genes_addfunc(ge, "/", 2, NULL);
	// genes_addfunc(ge, "sin", 1, NULL);
	// genes_addfunc(ge, "cos", 1, NULL);

	genes_addconst(ge, 1.0f);
	genes_addconst(ge, 2.0f);
	// genes_addconst(ge, 3.0f);
	// genes_addconst(ge, 4.0f);

	if (! genes_dataset(ge, "dataset.txt")) {
		syslog_error("Loading dataset.");
		return -1;
	}

	genes_evolution(ge, 300);
	
	genes_destroy(ge);

	return 0;
}

