#include <stdio.h>

#include "759/759.h"
#include "base/main/main.h"
#include "bdd/cudd/cudd.h"

int *variableRank;

int Abc_CommandReorder759(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    DdManager *dd;
    DdNode **outputs;
    int i, num_inputs, num_outputs, original_size, *perm;

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

    printf("DEBUG: ComvariableRankputing global BDD for network\n");

    Abc_Ntk_t * pTemp = Abc_NtkIsStrash(pNtk) ? pNtk : Abc_NtkStrash(pNtk, 0, 0, 0);
    dd = (DdManager *)Abc_NtkBuildGlobalBdds(pTemp, 10000000, 1, 0, 0, 0);

    if(dd == NULL ){
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


    variableRank = ABC_ALLOC(int, num_inputs);

    perm = ABC_ALLOC(int, num_inputs);
    outputs = ABC_ALLOC(DdNode *, num_outputs);


    // this **SHOULD** fix the variable ordering to the same ordering as in the BLIF file
    for (i = 0; i < num_outputs; i++) {
        outputs[i] = Cudd_bddIthVar(dd, i); 
        //printf("outputs[%d] = %p\n", i, (void *)outputs[i]);
        //need to add logic of each node through th Cudd_bdd(Gates)?
        
        //gives the variable an index basically, so that it's all within the output array
    }
    original_size = Cudd_ReadNodeCount(dd);
    printf("Original BDD size: %d\n", original_size);
    printf("before dscf\n");
    rankVarDSCF(dd, outputs, num_outputs, num_inputs);
    DSCFPermutation(perm, num_inputs);



    Cudd_ShuffleHeap(dd, perm);

    printf("DEBUG: Counted %d active nodes in the BDD\n", Cudd_ReadKeys(dd) - Cudd_ReadDead(dd));
    printf("BDD size after dscf: %d\n", Cudd_ReadNodeCount(dd));
    //Abc_NtkShowBdd(pTemp, 0, 0);

    if (pTemp != pNtk) {
        Abc_NtkDelete(pTemp);
    }

    ABC_FREE(perm);
    ABC_FREE(variableRank);
    ABC_FREE(outputs);

    return 0;
}

//rank based on the lengths of cubes
void rankVarDSCF(DdManager *dd, DdNode **bdd, int numOutputs, int numVars){
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE value;
    int i, cubeLength, o;

    // initialize ranks
    for (i = 0; i < numVars; i++) {
        variableRank[i] = 0;
    }

    printf("before cuddfirst\n");
    // Iterate through all the cubes 
    
    for (o = 0; o < numOutputs; o++){
        if(bdd[o] == NULL){
            printf("null output\n");
            continue;
        }

        printf("before cuddforeach\n");
    //going through lal the cubes (not sure if these are correct parameters)
     Cudd_ForeachCube(dd, bdd[o], gen, cube, value) {
        cubeLength = 0;
        printf("inside foreach\n");
        for (i = 0; i < numVars; i++) {
              //  printf("Cube[%d] = %d\n", i, cube[i]);

            // Variable appears in cube, == 0 -> negated, == 1 -> literal, == 2 -> dc
            if (cube[i] != 2) { 
                cubeLength++; //num of variables in cube
                variableRank[i]++;
            }
        }

        printf("At iteration [%d] the size is %d\n.", i, cubeLength);
        // rank by cube length
        for (i = 0; i < numVars; i++) {
            if (cube[i] != 2) {
                variableRank[i] += (numVars - cubeLength); 
                // shorter cubes get more weight
            }
        }
    }
    printf("finished foreachcube\n"); 


    }
 

}

int compareRanks(const void *a, const void *b) {
    int id = *(int *)a;
    int id2 = *(int *)b;
    return variableRank[id2] - variableRank[id]; 
}

void DSCFPermutation(int *perm, int numVars) {
    int i;
   
    for (i = 0; i < numVars; ++i) {
        perm[i] = i;
    }

    // Sort variables by rank (descending)
    qsort(perm, numVars, sizeof(int), compareRanks);
}