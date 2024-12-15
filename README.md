# BDDs Project
## Goal
The goal is to build a tool that will iterate through permutations of re-ordering variables in BDD to find the best minimized BDD.

## Input
A BLIF file. See `blif-benchmarks/`.

## What is This?
This is an extension to the popular synthesis/verification tool `ABC`. You will find that this is essentially a fork of `ABC` with an additional module under `src/759/`. This module provides a set of `ABC` commands that leverage existing `ABC` and `CUDD` code to enable our custom reordering logic. The overwhelming majority of our original code is in `src/759/759.c` and `src/759/759.h`. We also modified `src/base/abci/abc.c` to register our new commands.

The Makefile and dependencies for this project have been updated accordingly.

## Compilation 
Just run `make` from the top-level directory!

You will need any ABC compilation dependencies, but I'm fairly sure that it only needs GCC.

Prof. Yu: surely you will know better if you are missing something.

# Usage
We provide four new commands:
- `reorder759_start` initializes our reordering manager and builds a global BDD given that some BLIF file has been read into ABC.
- `reorder759_stop` discards existing reordering objects and frees memory.
- `reorder759` performs one iteration of our heuristic. It finds a new ordering given the current ordering, rearranges the current BDD according to the ordering, and records the best ordering found so far.
- `reorder759_compare` compares the performance of our heuristic to CUDD sifting, symmetrical sifting, and simulated annealing given the default variable ordering (from a BLIF file) and given the best variable ordering from our heuristic.

Here is an example of basic usage, given you have entered ABC using `./abc`.

```
read blif-benchmarks/adder8.blif
reorder759_start
reorder759
reorder759
reorder759_compare
```

If you don't wish to use `ABC`'s interactive mode, you may modify `test.input` and batch commands using `make test`. You will need to modify the first line of `test.input` for each new benchmark.

For certain BLIF inputs, it may be necessary to modify the constants `REORDER_LIMIT_FACTOR ` and `REORDER_SHORT_DIVISOR` within `src/759/759.h`. The former controls the upper limit for the ranking of any variable, allowing the heuristic to quit early. It scales the *size of the BDD given the default ordering*. The latter controls the definition of a "short" cube. It divides the number of input variables. We set both to 2 by default. If you have trouble with some particular input, change these appropriately by modifying `759.h` and running `make` again.