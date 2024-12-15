#include <stdio.h>

#include "759/759.h"
#include "base/main/main.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"

/* STATIC GLOBAL VARIABLES */

// manager should go in the ABC frame, but this was quicker
static Orderer_t *manager = NULL;

/* STATIC FUNCTION PROTOTYPES*/

static void rankVarDSCF(Orderer_t *manager, DdNode **bdd);
static int compareRanks(const void *a, const void *b);
static int compareIndices(const void *a, const void *b);

/* 759 COMMAND FUNCTIONS */

// start the reordering manager (precondition: a network has been read in)
int Abc_CommandReorder759_Start(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *ntk = Abc_FrameReadNtk(pAbc);
    Abc_Obj_t *pObj;
    int i;

    if (ntk == NULL) {
        printf("NULL network. Please read an input.\n");
        return 1;
    }

    if (manager != NULL) {
        printf("A manager exists for some network. Please run reorder759_stop.\n");
        return 1;
    }

    manager = malloc(sizeof(Orderer_t));
    manager->ontk = ntk;
    
    manager->sntk = Abc_NtkDup(ntk);
    manager->sntk = Abc_NtkIsStrash(manager->sntk) ? manager->sntk : Abc_NtkStrash(manager->sntk, 0, 0, 0);

    manager->dd = (DdManager *)Abc_NtkBuildGlobalBdds(manager->sntk, 10000000, 1, 0, 0, 0);

    if(manager->dd == NULL ){
        printf("Error building global BDDs\n");
        if (manager->sntk != manager->ontk) {
            Abc_NtkDelete(manager->sntk);
        }

        free(manager);

        return 1;
    }

    Cudd_AutodynDisable(manager->dd);

    manager->num_vars = Abc_NtkPiNum(manager->sntk);
    printf("DEBUG: Found %d primary inputs\n", manager->num_vars);

    manager->num_funcs = Abc_NtkPoNum(manager->sntk);
    printf("DEBUG: Found %d primary outputs\n", manager->num_funcs);

    manager->vars = malloc(sizeof(Ordered_Var_t) * manager->num_vars);
    manager->perm = malloc(sizeof(int) * manager->num_vars);
    manager->best_perm = malloc(sizeof(int) * manager->num_vars);

    for (int i = 0; i < manager->num_vars; ++i) {
        manager->vars[i].index = i;
        manager->vars[i].rank = 0;
        manager->perm[i] = i;
        manager->best_perm[i] = i;
    }

    manager->global_funcs = Vec_PtrAlloc(Abc_NtkCoNum(manager->sntk));

    // get DdNode * for each output function
    Abc_NtkForEachCo (manager->sntk, pObj, i) {
        Vec_PtrPush(manager->global_funcs, Abc_ObjGlobalBdd(pObj));
    }

    manager->default_size = Cudd_ReadNodeCount(manager->dd);
    manager->best_size = manager->default_size;
    printf("Default BDD size: %ld\n", manager->default_size);

    manager->ran_once = 0;

    return 0;
}

// stop the reordering manager
int Abc_CommandReorder759_Stop(Abc_Frame_t *pAbc, int argc, char **argv) {
    int i;
    DdNode *bFunc;

    if (manager == NULL) {
        printf("Manager is NULL. Please run reorder759_start.\n");
        return 1;
    }

    free(manager->perm);
    free(manager->best_perm);
    free(manager->vars);
    Abc_NtkFreeGlobalBdds(manager->sntk, 0);

    Vec_PtrForEachEntry (DdNode *, manager->global_funcs, bFunc, i) {
        Cudd_RecursiveDeref(manager->dd, bFunc);
    }

    Vec_PtrFree(manager->global_funcs);    

    if (manager->sntk != manager->ontk) {
        Abc_NtkDelete(manager->sntk);
    }

    return 0;
}

// apply one iteration of reordering
int Abc_CommandReorder759(Abc_Frame_t *pAbc, int argc, char **argv) {
    DdNode **outputs;
    long int new_size;

    if (manager == NULL) {
        printf("Manager is NULL. Please run reorder759_start.\n");
        return 1;
    }

    if (manager->ran_once == 0) {
        manager->ran_once = 1;
    }
    outputs = (DdNode **)Vec_PtrArray(manager->global_funcs);

    // rank variables
    rankVarDSCF(manager, outputs);

    // sort variables by rank (descending)
    qsort(manager->vars, manager->num_vars, sizeof(Ordered_Var_t), compareRanks);

    // set permutation according to computed rankings
    for (int i = 0; i < manager->num_vars; ++i) {
        manager->perm[i] = manager->vars[i].index;
    }

    #ifdef REORDER759_DEBUG
    printf("Variable ranks: ");
    for (int i = 0; i < manager->num_vars; ++i) {
        printf("%ld ", manager->vars[i].rank);
    }
    printf("\n");
    #endif

    // reorder variables in the decision diagram
    Cudd_ShuffleHeap(manager->dd, manager->perm);

    new_size = Cudd_ReadNodeCount(manager->dd);

    if (new_size < manager->best_size || (new_size > manager->best_size && manager->ran_once == 1)) {
        printf("New best size: %ld; (old best, default) = (%ld, %ld)\n", new_size, manager->best_size, manager->default_size);
        manager->best_size = new_size;

        for (int i = 0; i < manager->num_vars; ++i) {
            manager->best_perm[i] = manager->perm[i];
        }

        manager->ran_once = 2;
    } else {
        printf("New size is: %ld; (best, default) = (%ld, %ld)\n", new_size, manager->best_size, manager->default_size);
    }

    printf("Permutation: ");
    for (int i = 0; i < manager->num_vars; ++i) {
        printf("%d ", manager->perm[i]);
    }
    printf("\n");


    return 0;
}

// perform built-in reordering on default BDD, compare across various metrics
// very repetitive, i just want to get the results
// compares sifting, symmetric sifting, and annealing across default order and best permutation
int Abc_CommandReorder759_Compare(Abc_Frame_t *pAbc, int argc, char **argv) {
    DdManager *compare_dd_sift, *compare_dd_symm_sift, *compare_dd_anneal;
    DdManager *reo_dd_sift, *reo_dd_symm_sift, *reo_dd_anneal;
    Abc_Ntk_t *compare_ntk_sift, *compare_ntk_symm_sift, *compare_ntk_anneal;
    Abc_Ntk_t *reo_ntk_sift, *reo_ntk_symm_sift, *reo_ntk_anneal;

    long int sift_size, symm_sift_size, anneal_size;

    if (manager == NULL) {
        printf("Manager is NULL. Please run reorder759_start.\n");
        return 1;
    }

    // make duplicate networks from original
    compare_ntk_sift = Abc_NtkDup(manager->ontk);
    compare_ntk_symm_sift = Abc_NtkDup(manager->ontk);
    compare_ntk_anneal = Abc_NtkDup(manager->ontk);
    reo_ntk_sift = Abc_NtkDup(manager->ontk);
    reo_ntk_symm_sift = Abc_NtkDup(manager->ontk);
    reo_ntk_anneal = Abc_NtkDup(manager->ontk);

    // strash the networks
    compare_ntk_sift = Abc_NtkIsStrash(compare_ntk_sift) ? compare_ntk_sift : Abc_NtkStrash(compare_ntk_sift, 0, 0, 0);
    compare_ntk_symm_sift = Abc_NtkIsStrash(compare_ntk_symm_sift) ? compare_ntk_symm_sift : Abc_NtkStrash(compare_ntk_symm_sift, 0, 0, 0);
    compare_ntk_anneal = Abc_NtkIsStrash(compare_ntk_anneal) ? compare_ntk_anneal : Abc_NtkStrash(compare_ntk_anneal, 0, 0, 0);
    reo_ntk_sift = Abc_NtkIsStrash(reo_ntk_sift) ? reo_ntk_sift : Abc_NtkStrash(reo_ntk_sift, 0, 0, 0);
    reo_ntk_symm_sift = Abc_NtkIsStrash(reo_ntk_symm_sift) ? reo_ntk_symm_sift : Abc_NtkStrash(reo_ntk_symm_sift, 0, 0, 0);
    reo_ntk_anneal = Abc_NtkIsStrash(reo_ntk_anneal) ? reo_ntk_anneal : Abc_NtkStrash(reo_ntk_anneal, 0, 0, 0);

    // build global BDDs from networks
    compare_dd_sift = (DdManager *)Abc_NtkBuildGlobalBdds(compare_ntk_sift, 10000000, 1, 0, 0, 0);
    compare_dd_symm_sift = (DdManager *)Abc_NtkBuildGlobalBdds(compare_ntk_symm_sift, 10000000, 1, 0, 0, 0);
    compare_dd_anneal = (DdManager *)Abc_NtkBuildGlobalBdds(compare_ntk_anneal, 10000000, 1, 0, 0, 0);
    reo_dd_sift = (DdManager *)Abc_NtkBuildGlobalBdds(reo_ntk_sift, 10000000, 1, 0, 0, 0);
    reo_dd_symm_sift = (DdManager *)Abc_NtkBuildGlobalBdds(reo_ntk_symm_sift, 10000000, 1, 0, 0, 0); 
    reo_dd_anneal = (DdManager *)Abc_NtkBuildGlobalBdds(reo_ntk_anneal, 10000000, 1, 0, 0, 0);

    // apply best permutation from reordering manager
    Cudd_ShuffleHeap(reo_dd_sift, manager->best_perm);
    Cudd_ShuffleHeap(reo_dd_symm_sift, manager->best_perm);
    Cudd_ShuffleHeap(reo_dd_anneal, manager->best_perm);

    Cudd_ReduceHeap(compare_dd_sift, CUDD_REORDER_SIFT, 1);
    Cudd_ReduceHeap(compare_dd_symm_sift, CUDD_REORDER_SYMM_SIFT, 1);
    Cudd_ReduceHeap(compare_dd_anneal, CUDD_REORDER_ANNEALING, 1);

    sift_size = Cudd_ReadNodeCount(compare_dd_sift);
    symm_sift_size = Cudd_ReadNodeCount(compare_dd_symm_sift);
    anneal_size = Cudd_ReadNodeCount(compare_dd_anneal);
    printf("(sift size, symmetric sift size, annealing size) = (%ld, %ld, %ld)\n", sift_size, symm_sift_size, anneal_size);

    Cudd_ReduceHeap(reo_dd_sift, CUDD_REORDER_SIFT, 1);
    Cudd_ReduceHeap(reo_dd_symm_sift, CUDD_REORDER_SYMM_SIFT, 1);
    Cudd_ReduceHeap(reo_dd_anneal, CUDD_REORDER_ANNEALING, 1);

    sift_size = Cudd_ReadNodeCount(reo_dd_sift);
    symm_sift_size = Cudd_ReadNodeCount(reo_dd_symm_sift);
    anneal_size = Cudd_ReadNodeCount(reo_dd_anneal);
    printf("(759 best, 759 sift, 759 symmetric sift, 759 annealing) = (%ld, %ld, %ld, %ld)\n", manager->best_size, sift_size, symm_sift_size, anneal_size);

    Abc_NtkFreeGlobalBdds(compare_ntk_sift, 0);
    Abc_NtkFreeGlobalBdds(compare_ntk_symm_sift, 0);
    Abc_NtkFreeGlobalBdds(compare_ntk_anneal, 0);
    Abc_NtkFreeGlobalBdds(reo_ntk_sift, 0);
    Abc_NtkFreeGlobalBdds(reo_ntk_symm_sift, 0);
    Abc_NtkFreeGlobalBdds(reo_ntk_anneal, 0);
    
    Abc_NtkDelete(compare_ntk_sift);
    Abc_NtkDelete(compare_ntk_symm_sift);
    Abc_NtkDelete(compare_ntk_anneal);
    Abc_NtkDelete(reo_ntk_sift);
    Abc_NtkDelete(reo_ntk_symm_sift);
    Abc_NtkDelete(reo_ntk_anneal);
    
    return 0;
}

/* STATIC HELPER FUNCTIONS */

// consider the length of cubes over all outputs and rank variables
// according to their frequency in short cubes.
static void rankVarDSCF(Orderer_t *manager, DdNode **bdd) {
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE value;
    int i, cubeLength, o;
    DdManager *dd = manager->dd;
    long int limit = REORDER_LIMIT_FACTOR*manager->default_size;

    // order manager variables by current permutation
    qsort(manager->vars, manager->num_vars, sizeof(Ordered_Var_t), compareIndices);

    // initialize ranks
    for (i = 0; i < manager->num_vars; i++) {
        manager->vars[i].rank = 0;
    }

    // iterate over all combinational outputs
    for (o = 0; o < manager->num_funcs; o++) {
        if (bdd[o] == NULL){
            continue;
        }

        // for each cube of each combinational output
        Cudd_ForeachCube (dd, bdd[o], gen, cube, value) {
            cubeLength = 0;

            for (i = 0; i < manager->num_vars; i++) {
                // Variable appears in cube, == 0 -> negated, == 1 -> literal, == 2 -> dc
                if (cube[i] != 2) { 
                    cubeLength++;
                }
            }

            // rank by cube length: higher number is "better"
            for (i = 0; i < manager->num_vars; i++) {
                if (cube[i] != 2 && cubeLength <= manager->num_vars/REORDER_SHORT_DIVISOR) {
                    manager->vars[i].rank += (manager->num_vars - cubeLength); 
                }

                // get out early there are too many cubes relative to default BDD size
                if (manager->vars[i].rank >= limit) {
                    goto escape;
                }
            }
        }
    }

    escape:
}

static int compareRanks(const void *a, const void *b) {
    Ordered_Var_t id1= *(Ordered_Var_t *)a;
    Ordered_Var_t id2 = *(Ordered_Var_t *)b;
    return id2.rank - id1.rank; 
}

static int compareIndices(const void *a, const void *b) {
    Ordered_Var_t id1= *(Ordered_Var_t *)a;
    Ordered_Var_t id2 = *(Ordered_Var_t *)b;
    return id1.index - id2.index; 
}