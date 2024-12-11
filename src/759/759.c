#include <stdio.h>

#include "759/759.h"
#include "base/main/main.h"
#include "bdd/cudd/cudd.h"

int Abc_CommandReorder759(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    DdManager *dd;
    DdNode **outputs;
    int i, num_inputs, num_outputs, *perm;

    if (pNtk == NULL) {
        printf("NULL network. Please read an input.\n");
        return 1;
    }
    
    printf("DEBUG: Network exists\n");

    // Step 1: build BDD from current network
    // if (Abc_NtkIsBddLogic(pNtk)) {
    //     printf("DEBUG: Network is already BDD\n");
    // } else {
    //     printf("DEBUG: Converting network to BDD\n");
        
    //     if (!Abc_NtkToBdd(pNtk)) {
    //         printf("DEBUG: Conversion to BDD has failed. Aborting.\n");
    //         return 1;
    //     }
    // }

    printf("DEBUG: Computing global BDD for network\n");

    Abc_Ntk_t * pTemp = Abc_NtkIsStrash(pNtk) ? pNtk : Abc_NtkStrash(pNtk, 0, 0, 0);
    dd = (DdManager *)Abc_NtkBuildGlobalBdds(pTemp, 10000000, 1, 0, 0, 0);

    if(dd == NULL){
        printf("Error to build global BDDs\n");
        if (pTemp != pNtk) {
            Abc_NtkDelete(pTemp);
        }
        return 0;
    }
    //dd = (DdManager *)pNtk->pManFunc;

    // step 2: apply one iteration of reordering
    // --> FIXME: currently just playing with this for small inputs
    // --> THIS IS NOT FINAL OR CURRENT LOGIC
    // --> tbh im just trying to get something to work idk what im doing
    num_inputs = Abc_NtkPiNum(pNtk);
    printf("DEBUG: Found %d primary inputs\n", num_inputs);

    num_outputs = Abc_NtkPoNum(pNtk);
    printf("DEBUG: Found %d primary outputs\n", num_outputs);


    int *varRank = ABC_ALLOC(int, num_inputs);

    perm = ABC_ALLOC(int, num_inputs);
    outputs = ABC_ALLOC(DdNode *, num_outputs);


    // this **SHOULD** fix the variable ordering to the same ordering as in the BLIF file
    for (i = 0; i < num_outputs; i++) {
        outputs[i] = Cudd_bddIthVar(dd, i); 
        //gives the variable an index basically, so that it's all within the output array
    }

    rankVarDSCF(dd, outputs, num_outputs, varRank, num_inputs);
    DSCFPermutation(varRank, perm, num_inputs);



    Cudd_ShuffleHeap(dd, perm);

    printf("DEBUG: Counted %d active nodes in the BDD\n", Cudd_ReadKeys(dd) - Cudd_ReadDead(dd));

    //Abc_NtkShowBdd(pTemp, 0, 0);

    if (pTemp != pNtk) {
        Abc_NtkDelete(pTemp);
    }

    ABC_FREE(perm);
    ABC_FREE(varRank);
    ABC_FREE(outputs);

    return 0;
}

//rank based on the lengths of cubes
void rankVarDSCF(DdManager *dd, DdNode *bdd, int numOutputs, int *variableRank, int numVars){
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE value;
    int i;

    // initialize ranks
    for (i = 0; i < numVars; i++) {
        variableRank[i] = 0;
    }

    //going through lal the cubes (not sure if these are correct parameters)
    Cudd_ForeachCube(dd, bdd, gen, cube, value) {
        int cubeLength = 0;
        for (i = 0; i < numVars; i++) {
            // Variable appears in cube, == 0 -> negated, == 1 -> literal, == 2 -> dc
            if (cube[i] != 2) { 
                cubeLength++; //num of variables in cube
                variableRank[i]++;
            }
        }
        // rank by cube length
        for (i = 0; i < numVars; i++) {
            if (cube[i] != 2) {
                variableRank[i] += (numVars - cubeLength); 
                // shorter cubes get more weight
            }
        }
    }
}

void DSCFPermutation(int *variableRank, int *perm, int numVars) {
    int id, id2, i;
    const void *a, *b;
   
    for (i = 0; i < numVars; ++i) {
        perm[i] = i;
    }

    // Sort variables by rank (descending)
    qsort(perm, numVars, sizeof(int), compareRanks(variableRank, a, b));
}

int compareRanks(int *variableRank, const void *a, const void *b) {
    int id = *(int *)a;
    int id2 = *(int *)b;
    return variableRank[id2] - variableRank[id]; 
}