#include <stdio.h>

#include "759/759.h"
#include "base/main/main.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"

// STATIC GLOBAL VARIABLES
// manager should go in the ABC frame, but this was quicker
Orderer_t *manager = NULL;
int *variableRank;

static void rankVarDSCF(Orderer_t *manager, DdNode **bdd);
static int compareRanks(const void *a, const void *b);
static int compareIndices(const void *a, const void *b);

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

    return 0;
}

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

int Abc_CommandReorder759(Abc_Frame_t *pAbc, int argc, char **argv) {
    DdNode **outputs;
    long int new_size;

    if (manager == NULL) {
        printf("Manager is NULL. Please run reorder759_start.\n");
        return 1;
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
    printf("Permutation: ");
    for (int i = 0; i < manager->num_vars; ++i) {
        printf("%d ", manager->perm[i]);
    }
    printf("\n");

    printf("Variable ranks: ");
    for (int i = 0; i < manager->num_vars; ++i) {
        printf("%d ", manager->vars[i].rank);
    }
    printf("\n");
    #endif

    // reorder variables in the decision diagram
    Cudd_ShuffleHeap(manager->dd, manager->perm);

    new_size = Cudd_ReadNodeCount(manager->dd);

    if (new_size < manager->best_size) {
        printf("New best size: %ld; (old best, default) = (%ld, %ld)\n", new_size, manager->best_size, manager->default_size);
        manager->best_size = new_size;

        for (int i = 0; i < manager->num_vars; ++i) {
            manager->best_perm[i] = manager->perm[i];
        }
    } else {
        printf("New size is: %ld; (best, default) = (%ld, %ld)\n", new_size, manager->best_size, manager->default_size);
    }

    return 0;
}

// perform built-in reordering on default BDD, compare across various metrics
int Abc_CommandReorder759_Compare(Abc_Frame_t *pAbc, int argc, char **argv) {
    DdManager *compare_dd;
    Abc_Ntk_t *compare_ntk;
    long int anneal_size;

    if (manager == NULL) {
        printf("Manager is NULL. Please run reorder759_start.\n");
        return 1;
    }

    compare_ntk = Abc_NtkDup(manager->ontk);

    compare_ntk = Abc_NtkIsStrash(compare_ntk) ? compare_ntk : Abc_NtkStrash(compare_ntk, 0, 0, 0);

    compare_dd = (DdManager *)Abc_NtkBuildGlobalBdds(compare_ntk, 10000000, 1, 0, 0, 0);

    if(compare_dd == NULL ){
        printf("Error building global BDDs\n");
        if (compare_ntk != manager->ontk) {
            Abc_NtkDelete(compare_ntk);
        }

        return 1;
    }

    Cudd_AutodynDisable(compare_dd);

    Cudd_ReduceHeap(compare_dd, CUDD_REORDER_SIFT, 1);

    anneal_size = Cudd_ReadNodeCount(compare_dd);
    printf("(sift size, 759 best) = (%ld, %ld)\n", anneal_size, manager->best_size);

    Cudd_ReduceHeap(manager->dd, CUDD_REORDER_SIFT, 1);

    printf("759 best after sifting: %ld\n", Cudd_ReadNodeCount(manager->dd));

    Abc_NtkFreeGlobalBdds(compare_ntk, 0);
    Abc_NtkDelete(compare_ntk);

    return 0;
}

// consider the length of cubes over all outputs and rank variables
// according to their frequency in short cubes.
// "short" := half or more of the variables are DC
static void rankVarDSCF(Orderer_t *manager, DdNode **bdd) {
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE value;
    int i, cubeLength, o;
    DdManager *dd = manager->dd;

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
                if (cube[i] != 2 && cubeLength <= manager->num_vars / 2) {
                    manager->vars[i].rank += (manager->num_vars - cubeLength); 
                }
            }
        }
    }
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