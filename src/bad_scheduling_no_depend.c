#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <float.h>
#include <omp.h>
#include <malloc.h>
#include <assert.h>
#include <sys/time.h>
#include "delay.h"
#include <scorep/SCOREP_User.h>

#include "parse_flags.h"

#define NTASKS commandline_flags->ntasks // should be bigger than #threads
#define DEFAULT_DELAY_TIME commandline_flags->task_delay  // Default delaytime in microseconds
#define SMALL_RATIO commandline_flags->small_task_ratio
#define SMALL_TASK_PRIO commandline_flags->task_priority
#define BIG_TASK_PRIO commandline_flags->big_task_priority


SCOREP_USER_REGION_DEFINE(initialtask)
SCOREP_USER_REGION_DEFINE(bigtask)
SCOREP_USER_REGION_DEFINE(smalltask)

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
  double ratio = SMALL_RATIO;
  double delay_big = DEFAULT_DELAY_TIME;
  double delay_small = DEFAULT_DELAY_TIME / ratio;
    fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks: %i\n  Big task time: %fs\n  Indp. task time: %fs\n  Prio chain vs indp: %i vs %i\n", nthreads, ntasks, delay_big, delay_small, BIG_TASK_PRIO, SMALL_TASK_PRIO);


  printf("Delaylength Big: %f us\n", delay_big);
  printf("Delaylength Small: %f us\n", delay_small);
  
  struct timeval t1, t2;
	double etime;

  gettimeofday(&t1, NULL);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  #pragma omp parallel
  {
    #pragma omp single
    {
          SCOREP_USER_REGION_BEGIN(initialtask, "Initial Task", SCOREP_USER_REGION_TYPE_COMMON)
          int i;
          #pragma omp task priority(BIG_TASK_PRIO)
          {
            SCOREP_USER_REGION_BEGIN(bigtask, "Big Task", SCOREP_USER_REGION_TYPE_COMMON)
            delay_sleep(delay_big);
            SCOREP_USER_REGION_END(bigtask)
          }
          for (i=0; i < ntasks; i++)
          {
            #pragma omp task priority(SMALL_TASK_PRIO)
            {
              SCOREP_USER_REGION_BEGIN(smalltask, "Small Task", SCOREP_USER_REGION_TYPE_COMMON)
              delay_sleep(delay_small);
              SCOREP_USER_REGION_END(smalltask)
            }
          }
          SCOREP_USER_REGION_END(initialtask)
    }
  }
  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  gettimeofday(&t2, NULL);
	etime = (t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_usec - t1.tv_usec) / 1000;
	etime = etime / 1000;

  printf("Parallel region took %f sec.\n", etime);

  // Print expected values - Note: Current calculation assumes an integer ratio!
  double avg_task_time = ((ratio+ntasks) *delay_small) / (ntasks+1);
  double remaining_small_tasks = fmax(0,ntasks-(ratio*(nthreads-1)));
  double taNCP = delay_big + delay_small * ceil(remaining_small_tasks / nthreads);
  double avg_time_thread = (delay_big + delay_small * ntasks) / nthreads;
  double max_bad_case = (floor(ntasks/nthreads) + ratio) * delay_small;
  double lb_good = avg_time_thread / taNCP;
  double lb_bad = avg_time_thread / max_bad_case;
  int total_tasks = ntasks + 1;

  // bad case
  if (SMALL_TASK_PRIO > BIG_TASK_PRIO) {
    printf("Expected:\n  #tasks: %i\n  avg task time: %f s\n  LB: %f\n  SerE: 1\n  TE: ~1\n  TaNCP = %f\n  ThrCP = %f s\n", total_tasks, avg_task_time/1000000, lb_bad, taNCP/1000000, max_bad_case/1000000);
    print_output("bad_scheduling_no_depend", total_tasks, avg_task_time/1000000, taNCP/1000000, max_bad_case/1000000, etime, lb_bad, 1.0, 1);
  }
  // good case
  else {
    printf("Expected:\n  #tasks: %i\n  avg task time: %f s\n  LB: %f\n  SerE: 1\n  TE: ~1\n  TaNCP = ThrCP = %f s\n", total_tasks, avg_task_time/1000000, lb_good, taNCP/1000000);
    print_output("bad_scheduling_no_depend", total_tasks, avg_task_time/1000000, taNCP/1000000, taNCP/1000000, etime, lb_good, 1.0, 1);
  }
}

