/*
 * Fine-grained tasks, multi-creator pattern.
 *
 * Every thread of the team creates ntasks busy-wait tasks of the requested
 * length; the creators finish at the implicit barrier of the parallel
 * region. With P threads the total is P * ntasks tasks, the same workload as
 * the single-creator benchmark for the same parameters.
 *
 * Measurement rule: work inside a task that lies outside its timed interval
 * is seen by a tool but not by this benchmark, so the task body keeps all
 * bookkeeping inside the timed interval (where the busy-wait absorbs it).
 * After the final clock read a regular task does one integer add; each
 * creator's last task does one add and one store.
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

/* Per-thread state, written only by the thread with that index: the task
 * fields by tasks running on that thread, the creator fields by that
 * thread's implicit task. One cache line per thread. */
typedef struct {
  _Alignas(CACHE_LINE) long long task_count;
  long long task_ns;           /* W_t */
  long long creator_ns;        /* C_t */
  long long last_prefix_ns;    /* c_{t,last} */
  long long thread_prefix_ns;  /* p_{t,last} */
  long long creation_wall_ns;  /* wall time of the creation loop */
} thread_stats_t;

/* d_{k,last} is written once by whichever thread runs creator k's last
 * task, so it gets its own line instead of sharing creator k's. */
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
  int ntasks = NTASKS;                /* tasks per creator */
  int nthreads = omp_get_max_threads();

  fprintf(stdout,
          "Input parameters:\n"
          "  #threads: %i\n"
          "  #tasks per creator: %i\n"
          "  requested task time: %f us\n",
          nthreads, ntasks, delay);

  if (nthreads <= 0 || ntasks <= 0) {
    fprintf(stderr, "Expected positive thread and task counts.\n");
    return EXIT_FAILURE;
  }

  const long long delay_ns = llround(delay * 1000.0);

  thread_stats_t *stats =
      aligned_alloc(CACHE_LINE, (size_t)nthreads * sizeof *stats);
  last_task_t *last =
      aligned_alloc(CACHE_LINE, (size_t)nthreads * sizeof *last);

  if (stats == NULL || last == NULL) {
    perror("aligned_alloc");
    free(stats);
    free(last);
    return EXIT_FAILURE;
  }
  memset(stats, 0, (size_t)nthreads * sizeof *stats);
  memset(last, 0, (size_t)nthreads * sizeof *last);

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
    const int k = omp_get_thread_num();
    thread_stats_t *self = &stats[k];
    struct timespec start, end;
    long long creator_ns = 0;

    if (k == 0)
      team_size = omp_get_num_threads();

    clock_gettime(CLOCK_MONOTONIC, &start);
    const struct timespec creation_start = start;

    for (int i = 0; i < ntasks - 1; ++i) {
      clock_gettime(CLOCK_MONOTONIC, &end);
      creator_ns += ts_ns(&end) - ts_ns(&start);

      #pragma omp task
      busy_task(stats, delay_ns);

      /* Must remain outside the generated task. */
      clock_gettime(CLOCK_MONOTONIC, &start);
    }

    /* Last task of this creator. A separate construct, so regular tasks
       carry no is_last test. */
    clock_gettime(CLOCK_MONOTONIC, &end);
    creator_ns += ts_ns(&end) - ts_ns(&start);
    self->last_prefix_ns = creator_ns;
    /* self->task_ns is written only by tasks on this thread, and this
       thread is not running one now. */
    self->thread_prefix_ns = creator_ns + self->task_ns;

    #pragma omp task firstprivate(k)
    last[k].last_task_ns = busy_task(stats, delay_ns);

    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_MONOTONIC, &end);
    creator_ns += ts_ns(&end) - ts_ns(&start);

    self->creator_ns = creator_ns;
    self->creation_wall_ns = ts_ns(&end) - ts_ns(&creation_start);
  }

  clock_gettime(CLOCK_MONOTONIC, &t2);

#if ENABLE_CHECKS
  getrusage(RUSAGE_SELF, &ru_after);
#endif

  double etime = (ts_ns(&t2) - ts_ns(&t1)) * 1e-9;

  res = omp_control_tool(omp_control_tool_end, 0, NULL);
  printf("Control tool result: %d\n", res);
  printf("Parallel region took %.9f sec.\n", etime);

  long long total_tasks = 0;
  long long sum_task_ns = 0;
  long long sum_creator_ns = 0;
  long long max_creator_ns = 0;
  long long max_comp_ns = 0;
  long long tacp_ns = 0;
  long long chain_ns = 0;  /* max_k (p_{k,last} + d_{k,last}) */
  long long max_wall_ns = 0;

  for (int t = 0; t < team_size; ++t) {
    const thread_stats_t *s = &stats[t];
    const long long d_last = last[t].last_task_ns;

    total_tasks += s->task_count;
    sum_task_ns += s->task_ns;
    sum_creator_ns += s->creator_ns;
    max_creator_ns = max_ll(max_creator_ns, s->creator_ns);
    max_comp_ns = max_ll(max_comp_ns, s->task_ns + s->creator_ns);
    tacp_ns = max_ll(tacp_ns,
                     max_ll(s->creator_ns, s->last_prefix_ns + d_last));
    chain_ns = max_ll(chain_ns, s->thread_prefix_ns + d_last);
    max_wall_ns = max_ll(max_wall_ns, s->creation_wall_ns);
  }

  const long long thrcp_ns = max_ll(tacp_ns, max_ll(max_comp_ns, chain_ns));

  double task_time = total_tasks
                   ? ((double)sum_task_ns / total_tasks) * 1e-9
                   : 0.0;
  double maxComp = max_comp_ns * 1e-9;
  double avgComp = (sum_creator_ns + sum_task_ns) * 1e-9 / team_size;
  double taCP = tacp_ns * 1e-9;
  double thrCP = thrcp_ns * 1e-9;

  double lb = avgComp / maxComp;
  double te = thrCP / etime;
  double sere = maxComp / thrCP;

  printf("Task creation took %.9f sec (longest creator).\n",
         max_wall_ns * 1e-9);

#if ENABLE_CHECKS
  const long long expected_tasks = (long long)team_size * ntasks;
  if (team_size != nthreads) {
    fprintf(stderr,
            "Warning: team size %d differs from requested %d.\n",
            team_size, nthreads);
  }
  if (total_tasks != expected_tasks) {
    fprintf(stderr,
            "Warning: counted %lld tasks, expected %lld.\n",
            total_tasks, expected_tasks);
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
            "TaCP and ThrCP use each\n"
            "         creator's last task and may undershoot an exact "
            "critical path.\n",
            nivcsw);
  }

  printf("Per thread (s):\n"
         "  %4s %10s %14s %14s %14s %14s %14s\n",
         "tid", "#tasks", "W_t", "C_t", "c_last", "p_last", "d_last");
  for (int t = 0; t < team_size; ++t) {
    const thread_stats_t *s = &stats[t];
    printf("  %4d %10lld %14.9f %14.9f %14.9f %14.9f %14.9f\n",
           t, s->task_count, s->task_ns * 1e-9, s->creator_ns * 1e-9,
           s->last_prefix_ns * 1e-9, s->thread_prefix_ns * 1e-9,
           last[t].last_task_ns * 1e-9);
  }
#endif

  printf("Expected:\n"
         "  #tasks: %lld\n"
         "  avg task time: %.9f us\n"
         "  creator computation (sum): %.9f s\n"
         "  creator computation (max): %.9f s\n"
         "  max last-task chain p_last + d_last: %.9f s\n"
         "  maxComp: %.9f s\n"
         "  avgComp: %.9f s\n"
         "  LB: %f\n"
         "  SerE: %f\n"
         "  TE: %f\n"
         "  TaCP: %.9f s\n"
         "  ThrCP: %.9f s\n",
         total_tasks, task_time * 1e6,
         sum_creator_ns * 1e-9, max_creator_ns * 1e-9, chain_ns * 1e-9,
         maxComp, avgComp, lb, sere, te, taCP, thrCP);

  print_output("finegrained_multi_creator",
               ntasks * team_size,
               task_time, task_time,
               taCP, thrCP, etime, lb, te, sere);

  free(stats);
  free(last);
  return EXIT_SUCCESS;
}

