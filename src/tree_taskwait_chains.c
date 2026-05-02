#include "delay.h"
#include <omp.h>
#include <stdio.h>
#include <sys/time.h>
#include <scorep/SCOREP_User.h>

#include "parse_flags.h"

#define NBRANCHES_RATIO commandline_flags->branching // Nbranches/nthreads
#define INIT_CHAINLENGTH commandline_flags->depth
#define BRANCH_CHAINLENGTH commandline_flags->bdepth // Number of tasks in a branch
#define DEFAULT_DELAY_TIME commandline_flags->chain_task_delay  // Default delaytime in microseconds

SCOREP_USER_REGION_DEFINE(initchain)
SCOREP_USER_REGION_DEFINE(branch)
SCOREP_USER_REGION_DEFINE(firstbranch)

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  /* Initialization */
  double delaylength = DEFAULT_DELAY_TIME;
  printf("Delaylength: %f\n", delaylength);
  
  struct timeval t1, t2;
	double etime;

  int nthreads = omp_get_max_threads();
  double factor = NBRANCHES_RATIO;
  int nbranches = (int)(nthreads * factor);
  factor = nbranches / (double)nthreads;
  fprintf(stdout, "Input parameters: \n  #threads: %i\n  #tasks in init chain: %i\n  init task time: %f\n  #chains/#threads: %f -> %f\n  #branches: %i\n  #task in branches: %i\n  chain task time: %f\n", nthreads, INIT_CHAINLENGTH, delaylength, NBRANCHES_RATIO, factor, nbranches, BRANCH_CHAINLENGTH, delaylength);

  gettimeofday(&t1, NULL);
  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

  #pragma omp parallel
  {
    #pragma omp single
    {
      int i,j;
      struct timeval task_creation_start, task_creation_end;
      gettimeofday(&task_creation_start, NULL);
      for (i=0; i < INIT_CHAINLENGTH; i++) {
        #pragma omp task
        {
          SCOREP_USER_REGION_BEGIN(initchain, "Init Chain Task", SCOREP_USER_REGION_TYPE_COMMON)
          delay_sleep(delaylength);
          SCOREP_USER_REGION_END(initchain)
        }
        #pragma omp taskwait
      }

      for (i=0; i < nbranches; i++)
      {
        #pragma omp task
        {
          delay_sleep(delaylength);
          for(j=0; j < BRANCH_CHAINLENGTH-1; j++) {
            #pragma omp task
            {
              SCOREP_USER_REGION_BEGIN(branch, "Branch Task", SCOREP_USER_REGION_TYPE_COMMON)
              delay_sleep(delaylength);
              SCOREP_USER_REGION_END(branch)
            }
            #pragma omp taskwait
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
  double cp = (ceil(factor) * BRANCH_CHAINLENGTH + INIT_CHAINLENGTH) * (delaylength/1000000);
  // delay can be ignored since it crosses out
  double avg_init = INIT_CHAINLENGTH / (double)nthreads;
  double avg_branch = factor * BRANCH_CHAINLENGTH;
  double lb = (avg_init + avg_branch) / (ceil(factor) * BRANCH_CHAINLENGTH + INIT_CHAINLENGTH);
  int total_tasks = INIT_CHAINLENGTH + (nbranches * BRANCH_CHAINLENGTH);
  printf("Avg_init: %f, avg_branch: %f, max: %f\n", avg_init, avg_branch, (ceil(factor) * BRANCH_CHAINLENGTH + INIT_CHAINLENGTH));
  printf("Expected:\n  #tasks: %i\n  #init tasks: %i\n  #branches of length %i: %i\n  avg task time: %f s\n  LB*SerE: %f\n  TE: ~1\n  TaNCP = ThrCP = %f s\n", nbranches*BRANCH_CHAINLENGTH+INIT_CHAINLENGTH,INIT_CHAINLENGTH, BRANCH_CHAINLENGTH, nbranches, delaylength/1000000, lb, cp);
  print_output("tree_dep_chains", total_tasks, delaylength/1000000, cp, cp, etime, lb, 1.0, 0);
}