#ifndef PARSE_FLAG
#error "Define PARSE_FLAG prior to including this file!"
#endif

// PARSE_FLAG(Type, Name, DefaultValue, Description)
// Supported types: bool, const char *, int, uptr, double
// Default value must be a compile-time constant.
// Description must be a string literal.

PARSE_FLAG(int, ntasks, 55,
           "Number of independent tasks.")

PARSE_FLAG(int, many_tasks, 100000,
           "Large number of tasks.")

PARSE_FLAG(int, chain_ntasks, 500,
           "Number of tasks chained with dependences.")

PARSE_FLAG(int, big_ntasks, 1,
           "Number of bigger tasks.")

PARSE_FLAG(int, depth, 200,
           "Recursion depth.")

PARSE_FLAG(int, bdepth, 300,
           "Recursion depth after branching.")

PARSE_FLAG(double, branching, 1.5,
           "Branching factor.")

PARSE_FLAG(int, task_delay, 5000000,
           "Execution time for independent/initial tasks.")

PARSE_FLAG(int, chain_task_delay, 10000,
           "Execution time for chained tasks.")

PARSE_FLAG(int, branch_task_delay, 10000,
           "Execution time for branched tasks.")

PARSE_FLAG(double, short_task_delay, 0.1,
           "Short execution time for tasks.")

PARSE_FLAG(int, small_task_ratio, 5,
           "Ratio of small:big tasks.")

PARSE_FLAG(double, task_to_thread_ratio, 1.5,
           "Ratio of #tasks:#threads.")

PARSE_FLAG(double, chain_to_thread_ratio, 1.5,
           "Ratio of #tasks:#threads.")

PARSE_FLAG(int, big_task_delay, 5000000,
           "Execution time for big tasks.")

PARSE_FLAG(int, task_priority, 2,
           "Priority of independent tasks.")

PARSE_FLAG(int, chain_task_priority, 1,
           "Priority of chained tasks.")

PARSE_FLAG(int, big_task_priority, 1,
           "Priority of big tasks.")

PARSE_FLAG(int, abort_on_error, 0, "Abort instead of exit on error.")

PARSE_FLAG(int, help, 0, "Help")

PARSE_FLAG(int, continue_on_error, 1, "Continue after failing an assert.")

PARSE_FLAG(int, exitcode, 23, "Exit code for errors.")

PARSE_FLAG(const char *, outpath, "",
           "Write output to outpath. Default is\"<app>-<#procs>x<#threads>.txt\". "
           "The special values are \"stdout\" and \"stderr\".")