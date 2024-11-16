# BDDs Project
## Goal
The goal is to build a tool that will iterate through permutations to find the best BDD.

## Input
A BLIF file (eg from ABC) or a function in PLA format.

## Output
The size of the input BDD, the size of the output BDD, and graphs for "best size at each iteration" (monotonically decreasing) and "actual size at each iteration."

## Libraries
We may make use of the CUDD package to build BDDs and perform transformations, which will help us to avoid spending time on data structures. In particular, Dr. Yu has provided a "blif2bdd" package on top of CUDD which we can use to parse the input. You will need to compile and install it on your own system, so I've included the directory "blif-provided" in the .gitignore file. Just copy the entire "BDDs" folder from the provided ENEE759U repository and name it blif-provided.

## Project Stages (Proposed)
1. Integrate CUDD/blif2bdd to parse generic BLIF input and generate a BDD structure.
2. Develop a program skeleton: basic permuting re-ordering (no heuristic) within an iterating structure. The program structure should be generalizable so that we could apply a different function to the current structure at each iteration but *still remember past information/orderings*.
3. Generate output based on the iteration history.
4. Integrate new/different heuristics based on current research and benchmark them.
5. Develop command-line options and make the tool usable for a final presentation.

Note that the projects document on ELMS has more suggestions about how we should organize our code / submission.