#ifndef SEVENFIVENINE_H
#define SEVENFIVENINE_H

#include "base/abc/abc.h"
#include "base/main/abcapis.h"
#include "bdd/cudd/cudd.h"

typedef struct Ordered_Var {
    int index;
    long int rank;
} Ordered_Var_t;

typedef struct Orderer {
    Ordered_Var_t *vars;        // network inputs and their rankings
    int num_vars;               // the number of network inputs
    Vec_Ptr_t *global_funcs;    // network combinational outputs -- array of DdNode *s
    int num_funcs;              // the number of combinational outputs
    DdManager *dd;              // BDD unique table (CUDD)
    Abc_Ntk_t *sntk;            // ABC strashed network (from read input)
    Abc_Ntk_t *ontk;            // original ABC network

    int *perm;                  // current permuation

    long int default_size;      // the size of the BDD given a DEFAULT ordering
    long int best_size;         // the best size found over all iterations
    int *best_perm;             // the permutation corresponding to the best size
} Orderer_t;

int Abc_CommandReorder759(Abc_Frame_t *pAbc, int argc, char **argv);
int Abc_CommandReorder759_Start(Abc_Frame_t *pAbc, int argc, char **argv);
int Abc_CommandReorder759_Stop(Abc_Frame_t *pAbc, int argc, char **argv);
int Abc_CommandReorder759_Compare(Abc_Frame_t *pAbc, int argc, char **argv);

#endif