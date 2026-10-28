/*
 * Fine-grained tasks, single-creator pattern.
 *
 * One thread (inside a single construct) creates nthreads * ntasks busy-wait
 * tasks of the requested length; the region ends at the implicit barrier.
 *
 * Measurement rule: work inside a task that lies outside its timed interval
 * is seen by a tool but not by this benchmark, so the task body keeps all
 * bookkeeping inside the timed interval (where the busy-wait absorbs it).
 * After the final clock read a regular task does one integer add; the last
 * task does one add and one store.
 *
 * Compile with -DENABLE_CHECKS=1 to enable optional checks.
 * Compile with -DCACHE_LINE=128 on hardware with 128-byte lines or
 * adjacent-line prefetching.
 */
#include "delay.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parse_flags.h"

#define NTASKS commandline_flags->many_tasks
#define DEFAULT_DELAY_TIME commandline_flags->short_task_delay

#ifndef ENABLE_CHECKS
#define ENABLE_CHECKS 0
#endif

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

#if ENABLE_CHECKS
#include <sys/resource.h>
#endif

/* Per-thread task state, written only by tasks running on the thread with
 * that index. One cache line per thread. */
typedef struct {
  _Alignas(CACHE_LINE) long long task_count;
  long long task_ns;  /* W_t */
} thread_stats_t;

/* d_last is written once by whichever thread runs the last task, so it
 * gets its own line. */
typedef struct {
  _Alignas(CACHE_LINE) long long last_task_ns;
} last_task_t;

static inline long long ts_ns(const struct timespec *t)
{
  return (long long)t->tv_sec * 1000000000LL + t->tv_nsec;
}

static inline long long max_ll(long long a, long long b)
{
  return a > b ? a : b;
}

/* Task body. Returns the measured duration in ns. */
static inline long long busy_task(thread_stats_t *stats, long long delay_ns)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  const long long begin = ts_ns(&now);

  /* Inside the timed interval; absorbed by the busy-wait. */
  thread_stats_t *self = &stats[omp_get_thread_num()];
  self->task_count++;

  long long elapsed;
  do {
    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = ts_ns(&now) - begin;
  } while (elapsed < delay_ns);

  /* The only untimed work in a regular task. */
  self->task_ns += elapsed;
  return elapsed;
}

int main(int argc, char *argv[])
{
  parseArgs(argc, argv);

  struct timespec t1, t2;
  double delay = DEFAULT_DELAY_TIME;  /* microseconds */
  int ntasks = NTASKS;
  int nthreads = omp_get_max_threads();

  fprintf(stdout,
          "Input parameters:\n"
          "  #threads: %i\n"
          "  #tasks per loop: %i\n"
          "  requested task time: %f us\n",
          nthreads, ntasks, delay);

  if (nthreads <= 0 || ntasks <= 0) {
    fprintf(stderr, "Expected positive thread and task counts.\n");
    return EXIT_FAILURE;
  }

  const long long delay_ns = llround(delay * 1000.0);
  const long long total = (long long)nthreads * ntasks;

  thread_stats_t *stats =
      aligned_alloc(CACHE_LINE, (size_t)nthreads * sizeof *stats);

  if (stats == NULL) {
    perror("aligned_alloc");
    return EXIT_FAILURE;
  }
  memset(stats, 0, (size_t)nthreads * sizeof *stats);

  last_task_t last = { 0 };

  long long creator_ns = 0;             /* C */
  long long last_prefix_ns = 0;         /* c_last */
  long long creator_thread_prefix_ns = 0;  /* p_last */
  long long creation_wall_ns = 0;
  int creator_tid = -1;
  int team_size = 0;

  int res = omp_control_tool(omp_control_tool_start, 0, NULL);
  printf("Control tool result: %d\n", res);

#if ENABLE_CHECKS
  /* Sampled outside the parallel region so the check adds no work to any
     task or implicit task. */
  struct rusage ru_before, ru_after;
  getrusage(RUSAGE_SELF, &ru_before);
#endif

  clock_gettime(CLOCK_MONOTONIC, &t1);

  #pragma omp parallel
  {
    #pragma omp single
    {
      struct timespec start, end;
      long long c = 0;

      creator_tid = omp_get_thread_num();
      team_size = omp_get_num_threads();

      clock_gettime(CLOCK_MONOTONIC, &start);
      const struct timespec creation_start = start;

      for (long long n = 0; n < total - 1; ++n) {
        clock_gettime(CLOCK_MONOTONIC, &end);
        c += ts_ns(&end) - ts_ns(&start);

        #pragma omp task
        busy_task(stats, delay_ns);

        /* Must remain outside the generated task. */
        clock_gettime(CLOCK_MONOTONIC, &start);
      }

      /* Last task. A separate construct, so regular tasks carry no
         is_last test. */
      clock_gettime(CLOCK_MONOTONIC, &end);
      c += ts_ns(&end) - ts_ns(&start);
      last_prefix_ns = c;
      /* stats[creator_tid].task_ns is written only by tasks on this
         thread, and this thread is not running one now. */
      creator_thread_prefix_ns = c + stats[creator_tid].task_ns;

      #pragma omp task
      last.last_task_ns = busy_task(stats, delay_ns);

      clock_gettime(CLOCK_MONOTONIC, &start);
      clock_gettime(CLOCK_MONOTONIC, &end);
      c += ts_ns(&end) - ts_ns(&start);

      creator_ns = c;
      creation_wall_ns = ts_ns(&end) - ts_ns(&creation_start);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &t2);

#if ENABLE_CHECKS
  getrusage(RUSAGE_SELF, &ru_after);
#endif

  double etime = (ts_ns(&t2) - ts_ns(&t1)) * 1e-9;

  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  printf("Task creation took %.9f sec.\n", creation_wall_ns * 1e-9);
  printf("Parallel region took %.9f sec.\n", etime);

  long long total_tasks = 0;
  long long sum_task_ns = 0;
  long long max_comp_ns = 0;

  for (int t = 0; t < team_size; ++t) {
    total_tasks += stats[t].task_count;
    sum_task_ns += stats[t].task_ns;
    max_comp_ns = max_ll(max_comp_ns,
                         stats[t].task_ns
                         + (t == creator_tid ? creator_ns : 0));
  }

  const long long d_last = last.last_task_ns;
  const long long tacp_ns = max_ll(creator_ns, last_prefix_ns + d_last);
  const long long thrcp_ns =
      max_ll(tacp_ns, max_ll(max_comp_ns, creator_thread_prefix_ns + d_last));

  double task_time = total_tasks
                   ? ((double)sum_task_ns / total_tasks) * 1e-9
                   : 0.0;
  double maxComp = max_comp_ns * 1e-9;
  double avgComp = (creator_ns + sum_task_ns) * 1e-9 / team_size;
  double taCP = tacp_ns * 1e-9;
  double thrCP = thrcp_ns * 1e-9;

  double lb = avgComp / maxComp;
  double te = thrCP / etime;
  double sere = maxComp / thrCP;

#if ENABLE_CHECKS
  if (total_tasks != total) {
    fprintf(stderr,
            "Warning: counted %lld tasks, expected %lld.\n",
            total_tasks, total);
  }
  if (te > 1.0) {
    fprintf(stderr,
            "Warning: TE > 1; inspect timing intervals and accounting.\n");
  }
  const long nivcsw = ru_after.ru_nivcsw - ru_before.ru_nivcsw;
  if (nivcsw > 0) {
    fprintf(stderr,
            "Warning: %ld involuntary context switch(es) during the "
            "parallel region.\n"
            "         A task may have run far longer than requested; "
            "TaCP and ThrCP use the\n"
            "         last task and may undershoot an exact critical "
            "path.\n",
            nivcsw);
  }
#endif

  printf("Expected:\n"
         "  #tasks: %lld\n"
         "  avg task time: %.9f us\n"
         "  creator computation: %.9f s\n"
         "  last task prefix: %.9f s\n"
         "  creator-thread prefix: %.9f s\n"
         "  last task time: %.9f s\n"
         "  LB: %f\n"
         "  SerE: %f\n"
         "  TE: %f\n"
         "  TaCP: %.9f s\n"
         "  ThrCP: %.9f s\n",
         total_tasks, task_time * 1e6,
         creator_ns * 1e-9, last_prefix_ns * 1e-9,
         creator_thread_prefix_ns * 1e-9, d_last * 1e-9,
         lb, sere, te, taCP, thrCP);

  print_output("finegrained_single_creator",
               ntasks * nthreads,
               task_time, task_time,
               taCP, thrCP, etime, lb, te, sere);

  free(stats);
  return EXIT_SUCCESS;
}

