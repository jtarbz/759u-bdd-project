#include <stdio.h>

#include "759/759.h"
#include "base/main/main.h"
#include "bdd/cudd/cudd.h"

int Abc_CommandReorder759(Abc_Frame_t *pAbc, int argc, char **argv) {
    Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
    DdManager *dd;
    int num_inputs, *perm;

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

    //dd = (DdManager *)pNtk->pManFunc;

    // step 2: apply one iteration of reordering
    // --> FIXME: currently just playing with this for small inputs
    // --> THIS IS NOT FINAL OR CURRENT LOGIC
    // --> tbh im just trying to get something to work idk what im doing
    num_inputs = Abc_NtkPiNum(pNtk);
    printf("DEBUG: Found %d primary inputs\n", num_inputs);

    perm = ABC_ALLOC(int, num_inputs);

    // this **SHOULD** fix the variable ordering to the same ordering as in the BLIF file
    for (int i = 0; i < num_inputs; ++i) {
        perm[i] = i;
    }

    Cudd_ShuffleHeap(dd, perm);

    printf("DEBUG: Counted %d active nodes in the BDD\n", Cudd_ReadKeys(dd) - Cudd_ReadDead(dd));

    //Abc_NtkShowBdd(pTemp, 0, 0);

    if (pTemp != pNtk) {
        Abc_NtkDelete(pTemp);
    }

    ABC_FREE(perm);

    return 0;
}