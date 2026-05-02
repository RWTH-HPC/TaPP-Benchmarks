#include "delay.h"
#include <omp.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "parse_flags.h"

#define NTASKS commandline_flags->many_tasks
#define DEFAULT_DELAY_TIME commandline_flags->short_task_delay  // Default delaytime in microseconds

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  /* Initialization */  
  struct timespec t1, t2;

  double delay = DEFAULT_DELAY_TIME;
  int ntasks = NTASKS;
  double stime = 0.0;

  // Run without OpenMP for TE estimation
  clock_gettime(CLOCK_MONOTONIC, &t1);
  {
    int i;
    double ttime = 0.0;
    struct timespec task_creation_start, task_creation_end;
    clock_gettime(CLOCK_MONOTONIC, &task_creation_start);
    for (i=0; i < ntasks; i++)
    {
      struct timespec task_exec_begin, task_exec_end;
      double elapsed;
      clock_gettime(CLOCK_MONOTONIC, &task_exec_begin);
      clock_gettime(CLOCK_MONOTONIC, &task_exec_end);
      elapsed = (task_exec_end.tv_sec - task_exec_begin.tv_sec) * 1000000 + (task_exec_end.tv_nsec - task_exec_begin.tv_nsec) / 1000.0;
      while (elapsed < delay) {
        clock_gettime(CLOCK_MONOTONIC, &task_exec_end);
        elapsed = (task_exec_end.tv_sec - task_exec_begin.tv_sec) * 1000000 + (task_exec_end.tv_nsec - task_exec_begin.tv_nsec) / 1000.0;
      }
      // printf("Elapsed: %f\n", elapsed);
      // delay_sleep(delaylength);
    }
    clock_gettime(CLOCK_MONOTONIC, &task_creation_end);
    ttime = (task_creation_end.tv_sec - task_creation_start.tv_sec) + (task_creation_end.tv_nsec - task_creation_start.tv_nsec) * 1e-9;
    printf("Task creation took %.9f sec.\n", ttime);
  }
  clock_gettime(CLOCK_MONOTONIC, &t2);
	stime = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) * 1e-9;

  printf("Serial execution took %f sec.\n", stime);


  // Parallel execution
  int nthreads = omp_get_max_threads();
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks: %i\n  task time: %f\n", nthreads, ntasks, delay);
  double etime = 0.0;

  clock_gettime(CLOCK_MONOTONIC, &t1);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  double *dep_arr = malloc(sizeof(double) * nthreads);

  #pragma omp parallel
  {
    #pragma omp single
    {
      int i,j;
      double ttime = 0.0;
      struct timespec task_creation_start, task_creation_end;
      clock_gettime(CLOCK_MONOTONIC, &task_creation_start);
      for (j=0; j< nthreads; j++) {
        for (i=0; i < ntasks; i++)
        {
          #pragma omp task depend(inout: dep_arr[j])
          {
            struct timespec task_exec_begin, task_exec_end;
            double elapsed;
            clock_gettime(CLOCK_MONOTONIC, &task_exec_begin);
            clock_gettime(CLOCK_MONOTONIC, &task_exec_end);
            elapsed = (task_exec_end.tv_sec - task_exec_begin.tv_sec) * 1000000 + (task_exec_end.tv_nsec - task_exec_begin.tv_nsec) / 1000.0;
            while (elapsed < delay) {
              clock_gettime(CLOCK_MONOTONIC, &task_exec_end);
              elapsed = (task_exec_end.tv_sec - task_exec_begin.tv_sec) * 1000000 + (task_exec_end.tv_nsec - task_exec_begin.tv_nsec) / 1000.0;
            }
            // printf("Elapsed: %f\n", elapsed);
            // delay_sleep(delaylength);
          }
        }
      }
      clock_gettime(CLOCK_MONOTONIC, &task_creation_end);
      ttime = (task_creation_end.tv_sec - task_creation_start.tv_sec) + (task_creation_end.tv_nsec - task_creation_start.tv_nsec) * 1e-9;
      printf("Task creation took %.9f sec.\n", ttime);
    }
  }
  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  clock_gettime(CLOCK_MONOTONIC, &t2);
	etime = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) * 1e-9;

  printf("Parallel region took %f sec.\n", etime);

  // Print expected values - Note: Current estimation not verified to work correctly
  double te = stime / etime;
  double cp = (ntasks * delay)/1000000;
  if (NTASKS > 1000000 && DEFAULT_DELAY_TIME < 10) {
    printf("Expected:\n  #tasks: %i\n  avg task time: %f us\n  LB: 1\n  SerE: 1\n  TE: %f\n  TaNCP = ThrCP\n", NTASKS, delay, te);
  }
  print_output("finegrained_single_creator_dep", ntasks*nthreads, delay/1000000, cp, cp, etime, 1.0, te, 1);
}