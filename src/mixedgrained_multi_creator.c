#include "delay.h"
#include <bits/time.h>
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
  double ratio = 2.5;
  double big_task_delay = delay * ntasks * ratio;
  printf("Bog delay: %f\n", big_task_delay);


  // Parallel execution
  int nthreads = omp_get_max_threads();
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks: %i\n  task time: %f\n", nthreads, ntasks, delay);
  double etime = 0.0;

  clock_gettime(CLOCK_MONOTONIC, &t1);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  #pragma omp parallel
  {
      int i;
      double ttime = 0.0;
      struct timespec task_creation_start, task_creation_end;
      clock_gettime(CLOCK_MONOTONIC, &task_creation_start);
      if (omp_get_thread_num() == 0)
      {
        #pragma omp task
        {
          delay_sleep(big_task_delay);
        }
      }
      else {
        for (i=0; i < ntasks; i++)
        {
          #pragma omp task
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
  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  clock_gettime(CLOCK_MONOTONIC, &t2);
	etime = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) * 1e-9;

  printf("Parallel region took %f sec.\n", etime);

  // Print expected values
  double te = 1.0;
  double cp = (big_task_delay)/1000000;
  double avg_runtime = ((delay * ntasks * (nthreads-1) + big_task_delay) / nthreads) /1000000;
  double lb = avg_runtime / cp;
  double avg_delay = (delay * ntasks * (nthreads-1) + big_task_delay) / (ntasks + 1);
  printf("Expected:\n  #tasks: %i\n  avg task time: %f us\n  LB: %f\n  SerE: 1\n  TE: %f\n  TaNCP = ThrCP = %f\n", (ntasks*(nthreads-1))+1, avg_delay, lb, te, cp);
  print_output("mixedgrained_multi_creator", ntasks*(nthreads-1)+1, avg_delay/1000000, cp, cp, etime, lb, te, 1);
}