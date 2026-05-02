#include "delay.h"
#include <omp.h>
#include <stdio.h>
#include <sys/time.h>

#include "parse_flags.h"

#define NTASKS_TO_THREADS_FACTOR commandline_flags->task_to_thread_ratio
#define DEFAULT_DELAY_TIME commandline_flags->big_task_delay  // Default delaytime in microseconds

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  /* Initialization */
  double delaylength = DEFAULT_DELAY_TIME;
  printf("Delaylength: %f\n", delaylength);
  
  struct timeval t1, t2;
	double etime;

  int nthreads = omp_get_max_threads();
  double factor = NTASKS_TO_THREADS_FACTOR;
  int ntasks = (int)(nthreads * factor);
  factor = ntasks / (double)nthreads;
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks: %i\n  #tasks/#threads: %f -> %f\n  task time: %f\n", nthreads, ntasks, NTASKS_TO_THREADS_FACTOR, factor, delaylength);

  gettimeofday(&t1, NULL);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  #pragma omp parallel
  {
    #pragma omp single
    {
      int i;
      struct timeval task_creation_start, task_creation_end;
      gettimeofday(&task_creation_start, NULL);
      for (i=0; i < ntasks; i++)
      {
        #pragma omp task
        {
          delay_sleep(delaylength);
        }
      }
      gettimeofday(&task_creation_end, NULL);
      etime = (task_creation_end.tv_sec - task_creation_start.tv_sec) * 1000 + (task_creation_end.tv_usec - task_creation_start.tv_usec) / 1000;
	    etime = etime / 1000;
      printf("Task creation took %.9f sec.\n", etime);
    }
  }
  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  gettimeofday(&t2, NULL);
	etime = (t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec) / 1000;
	etime = etime / 1000;

  printf("Parallel region took %f sec.\n", etime);

  // Print expected values
  double lb = factor / ceil(factor);
  double cp = ceil(factor) * (delaylength/1000000);
  printf("Expected:\n  #tasks: %i\n  avg task time: %f s\n  LB: %f\n  SerE: 1\n  TE: ~1\n  TaNCP = ThrCP = %f s\n", ntasks, delaylength/1000000, lb, cp);
  print_output("coarsegrained_single_creator", ntasks, delaylength/1000000, cp, cp, etime, lb, 1.0, 1);
}