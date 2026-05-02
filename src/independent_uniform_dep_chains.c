#include "delay.h"
#include <omp.h>
#include <stdio.h>
#include <sys/time.h>

#include "parse_flags.h"

#define NCHAINS_FACTOR commandline_flags->chain_to_thread_ratio // nchains/nthreads
#define CHAINLENGTH commandline_flags->chain_ntasks // Number of tasks in a chain
#define DEFAULT_DELAY_TIME commandline_flags->chain_task_delay  // Default delaytime in microseconds

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  /* Initialization */
  double delaylength = DEFAULT_DELAY_TIME;
  printf("Delaylength: %f\n", delaylength);
  
  struct timeval t1, t2;
	double etime;

  int nthreads = omp_get_max_threads();
  int chainlength = CHAINLENGTH;
  double factor = NCHAINS_FACTOR;
  int nchains = (int)(nthreads * factor);
  factor = nchains / (double)nthreads;
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks in chain: %i\n  #chains/#threads: %f -> %f\n  #chains: %i\n  task time: %f\n", nthreads, chainlength, NCHAINS_FACTOR, factor, nchains, delaylength);

  gettimeofday(&t1, NULL);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  double *dep_arr = malloc(sizeof(double) * nchains);

  #pragma omp parallel
  {
    #pragma omp single
    {
      int i,j;
      struct timeval task_creation_start, task_creation_end;
      gettimeofday(&task_creation_start, NULL);
      for (i=0; i < nchains; i++)
      {
        for(j=0; j < chainlength; j++) {
          #pragma omp task depend(inout: dep_arr[i])
          {
            delay_sleep(delaylength);
          }
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
  double cp = ceil(factor) * (delaylength/1000000) * chainlength;
  double total_tasks = nchains * chainlength;
  printf("Expected:\n  #tasks: %i\n  #chains of length %i: %i\n  avg task time: %f s\n  LB*SerE: %f\n  TE: ~1\n  TaNCP = ThrCP = %f s\n", nchains*chainlength, chainlength, nchains, delaylength/1000000, lb, cp);
  print_output("independent_uniform_dep_chains", total_tasks, delaylength/1000000, cp, cp, etime, lb, 1.0, 0);
}