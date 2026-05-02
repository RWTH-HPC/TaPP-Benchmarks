# TaPP-Benchmarks
Tasking Performance Problem Benchmarks - OpenMP tasking codes that exhibit fixed performance problems

# How to Build
By default, the codes are built with icx+icpx. If you are using a different compiler, adapt the compiler and linker variables in the Makefile. Then:

```make```

This will create all executables inside a separate `bin` folder.

# How to Execute
```cd bin```

```OMP_NUM_THREADS=<nthreads> ./<exe> [--<param>=<val>]```

For bad_scheduling_*, please also specify `OMP_MAX_TASK_PRIORITY=2`

The codes used for the ICPP'26 submission are: 
- finegrained_multi_creator 
- finegrained_multi_creator_dep
- coarsegrained_single_creator 
- independent_uniform_dep_chains 
- independent_uniform_create_chains 
- independent_uniform_taskwait_chains
- tree_dep_chains 
- tree_create_chains 
- tree_taskwait_chains
- bad_scheduling_no_depend
- bad_scheduling_dep_chain

The following additional, experimental codes are available:
- finegrained_single_creator
- finegrained_single_creator_dep
- mixedgrained_multi_creator 

To get an overview of the parameters of all applications, you can call `./<exe> --help` for any executable.
For a mapping of parameters to codes, see the next section.

You can specify an output file via `--outpath=<path>`. Accepted special values are also `stdout` and `stderr`.
By default, the output is put into a file named `<app>-1x<nthreads>.txt`.

## Parameter Mapping
#### finegrained_multi_creator + finegrained_multi_creator_dep
- many-tasks: number of generated tasks per thread
- short-task-delay: duration of single task in microseconds
#### coarsegrained_single_creator
- task-to-thread-ratio: number of generated tasks / number of threads
- big-task-delay: duration of a single task
#### independent_uniform_dep_chains + independent_uniform_create_chains + independent_uniform_taskwait_chains
- chain-to-thread-ratio: number of generated chains / number of threads
- chain-ntasks: number of tasks in a chain
- chain-task-delay: duration of a single task in the chain
#### tree_dep_chains + tree_create_chains + tree_taskwait_chains
- branching: number of branches / number of threads
- depth: number of tasks in the tree trunk chain
- bdepth: number of tasks in a branch chain
- chain-task-delay: duration of a single task in the tree chain
#### bad_scheduling_no_depend
- ntasks: number of small (independent) tasks
- task-delay: duration of the single big task
- small-task-ratio: duration of big task / duration of a small task (assumes an integer value)
- task-priority: task priority value for a small task (should be 0, 1 or 2)
- big-task-priority: task priority value for the big task (should be 0, 1 or 2)

#### bad_scheduling_dep_chain
- ntasks: number of small (independent) tasks
- chain-ntasks: number of tasks in a chain
- task-delay: duration of a small task
- chain-task-delay: duration of a task inside the chain
- task-priority: task priority value for a small task (should be 0, 1 or 2)
- chain-task-priority: task priority value for tasks in the chain (should be 0, 1 or 2)

#### finegrained_single_creator + finegrained_single_creator_dep + mixedgrained_multi_creator
- many-tasks: number of generated tasks per thread
- short-task-delay: duration of single task in microseconds