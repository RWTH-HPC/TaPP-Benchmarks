#include "delay.h"
#include <omp.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

#include "parse_flags.h"

#define NTASKS_PER_THREAD commandline_flags->ntasks_per_thread // number of "columns" of independent tasks / tasks per thread ignoring chain tail
#define CHAINLENGTH commandline_flags->chain_ntasks // Number of tasks in a chain
#define DEFAULT_DELAY_TIME commandline_flags->chain_task_delay  // Default delaytime in microseconds

void recursive_task(int depth, double delaylength) {
  delay_sleep(delaylength);
  if (depth > 0) {
    #pragma omp task
    {
      recursive_task(depth-1, delaylength);
    }
  }
}

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
  int ntasks_per_thread = NTASKS_PER_THREAD;
  int total_indep_tasks = (ntasks_per_thread * nthreads) - ntasks_per_thread;
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks in chain: %i\n  ntasks per thread: %i \n  task time: %f\n", nthreads, chainlength, NTASKS_PER_THREAD, delaylength);

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
      if (chainlength > 0) {
        #pragma omp task
        {
          recursive_task(chainlength-1, delaylength);
        }
      }
      for (i=0; i < total_indep_tasks; i++) {
        #pragma omp task
        {
          recursive_task(0, delaylength);
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
  double avg_exec = (total_indep_tasks + chainlength) / (double)nthreads ;
  double max_exec = chainlength;
  double lbser = avg_exec / max_exec;
  double taCP = (delaylength/1000000) * chainlength;
  double cp = taCP;
  int total_tasks = total_indep_tasks + chainlength;
  printf("Expected:\n  #tasks: %i\n  #chain of length %i\n  filled 'columns': %i\n  avg task time: %f s\n  LB*SerE: %f\n  TE: ~1\n TaCP = %f\n TaNCP = ThrCP = %f s\n", total_tasks, chainlength, ntasks_per_thread, delaylength/1000000, lbser, taCP, cp);
  print_output("uniform_create_chain", total_tasks, delaylength/1000000, taCP, cp, cp, etime, lbser, 1.0, 0);
}