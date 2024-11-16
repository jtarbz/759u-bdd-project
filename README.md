# BDDs Project
## Goal
The goal is to build a tool that will iterate through permutations to find the best BDD.

## Input
A BLIF file (eg from ABC) or a function in PLA format.

## Output
The size of the input BDD, the size of the output BDD, and graphs for "best size at each iteration" (monotonically decreasing) and "actual size at each iteration."

## Libraries
We may make use of the CUDD package to build BDDs and perform transformations, which will help us to avoid spending time on data structures. In particular, Dr. Yu has provided a "blif2bdd" package on top of CUDD which we can use to parse the input. You will need to compile and install it on your own system, so I've included the directory "blif-provided" in the .gitignore file. Just copy the entire "BDDs" folder from the provided ENEE759U repository and name it blif-provided.