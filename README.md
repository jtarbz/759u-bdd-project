# BDDs Project
## Goal
The goal is to build a tool that will iterate through permutations of re-ordering variables in BDD to find the best minimized BDD.

## Input
A BLIF file (eg from ABC) or a function in PLA format.

## Output
The size of the input BDD, the size of the output BDD, and graphs for "best size at each iteration" (monotonically decreasing) and "actual size at each iteration."

## What is This?
We are building an extension to the popular synthesis/verification tool `ABC`. You will find that this is essentially a fork of `ABC` with an additional
module under `src/759/`. This module provides a set of `ABC` commands that leverage existing `ABC` and `CUDD` code to enable our custom reordering logic, which
we hope to perform at least on par with existing heuristics.

## Compilation 
Just run `make` from the top-level directory!

You will need any ABC compilation dependencies, but I'm fairly sure that it only needs GCC.

Prof. Yu: surely you will know better if you are missing something.

# Usage
The main entry point for our ABC extension is the `reorder759` command.