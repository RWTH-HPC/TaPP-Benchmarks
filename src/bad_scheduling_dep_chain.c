#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <float.h>
#include <omp.h>
#include <malloc.h>
#include <assert.h>
#include <sys/time.h>
#include "delay.h"

#include "parse_flags.h"

#define NTASKS commandline_flags->ntasks // should be bigger than #threads, number of tasks outside of chain
#define CHAINLENGTH commandline_flags->chain_ntasks // number of tasks in the long chain
#define DELAY_TASKS commandline_flags->task_delay  // Default delaytime in microseconds
#define DELAY_CHAIN_TASKS commandline_flags->chain_task_delay
#define CHAIN_START_PRIO commandline_flags->chain_task_priority
#define TASK_PRIO commandline_flags->task_priority

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);
  int passed;
  passed = (omp_get_max_task_priority() >= 2);
  printf("Got %d max priority via env\n", omp_get_max_task_priority());
  if(!passed) {
    printf( "failed\n" );
    return 1;
  }

  /* Initialization */
  int nthreads = omp_get_max_threads();
  int ntasks = NTASKS;
  int chainlength = CHAINLENGTH;
  double delay_task = DELAY_TASKS;
  double delay_chain_task = DELAY_CHAIN_TASKS;
  double ratio = (chainlength * delay_chain_task) / delay_task;
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks: %i\n  |chain|: %i with %fs per task\n  Indp. task time: %fs\n  Prio chain vs indp: %i vs %i\n", nthreads, ntasks, chainlength, delay_chain_task, delay_task, CHAIN_START_PRIO, TASK_PRIO);
  
  struct timeval t1, t2;
	double etime;

  gettimeofday(&t1, NULL);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  double x;

  #pragma omp parallel
  {
    #pragma omp single
    {
          int i;
          for (i=0; i < NTASKS; i++)
          {
            #pragma omp task priority(TASK_PRIO)
            {
              delay_sleep(delay_task);
            }
          }
          for (i=0; i < CHAINLENGTH; i++)
          {
            #pragma omp task depend(inout: x) priority(CHAIN_START_PRIO)
            {
              delay_sleep(delay_chain_task);
            }
          }
    }
  }
  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  gettimeofday(&t2, NULL);
	etime = (t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec) / 1000;
	etime = etime / 1000;

  printf("Parallel region took %f sec.\n", etime);

  // Print expected values - Note: Current calculation assumes an integer ratio!
  double delay_big = delay_chain_task * chainlength;
  double avg_task_time = ((ratio+ntasks) *delay_task) / (ntasks+1);
  double remaining_small_tasks = fmax(0,ntasks-(ratio*(nthreads-1)));
  double taNCP = delay_big + delay_task * ceil(remaining_small_tasks / nthreads);
  double avg_time_thread = (delay_big + delay_task * ntasks) / nthreads;
  double max_bad_case = (floor(ntasks/(double)nthreads) + ratio) * delay_task;
  double lb_good = avg_time_thread / taNCP;
  double lb_bad = avg_time_thread / max_bad_case;
  int total_tasks = ntasks + chainlength;

  // bad case
  if (TASK_PRIO > CHAIN_START_PRIO) {
    printf("Expected:\n  #tasks: %i\n  avg task time: %f s\n  LB*Ser: %f\n  TE: ~1\n  TaNCP = %f\n  ThrCP = %f s\n", total_tasks, avg_task_time/1000000, lb_bad, taNCP/1000000, max_bad_case/1000000);
    print_output("bad_scheduling_dep_chain", total_tasks, avg_task_time/1000000, taNCP/1000000, max_bad_case/1000000, etime, lb_bad, 1.0, 0);
  }
  // good case
  else {
    printf("Expected:\n  #tasks: %i\n  avg task time: %f s\n  LB*Ser: %f\n  TE: ~1\n  TaNCP = ThrCP = %f s\n", total_tasks, avg_task_time/1000000, lb_good, taNCP/1000000);
    print_output("bad_scheduling_dep_chain", total_tasks, avg_task_time/1000000, taNCP/1000000, taNCP/1000000, etime, lb_good, 1.0, 0);
  }
}

