/* file: showload.c
   Ben Miller, for cisc361

   Prints outs 1min, 5min and 15min load averages similar to uptime.
   Requires compiling with -DHAVE_KSTAT and linking with -lkstat on Solaris.
   Not supported on other platforms.

   "uptime" no longer uses kstat,
   and uses getloadavg(3C) instead.  It can be used like this and it agrees
   with uptime:

   #include <sys/loadavg.h>
   printf("%.2lf, %.2lf, %.2lf\n", loads[LOADAVG_1MIN], loads[LOADAVG_5MIN], \
   loads[LOADAVG_15MIN]);

   getloadavg() should also be more portable too.  I see Linux has it anyway
   and probably the BSDs too.
*/

#include <sys/param.h>
#include <stdio.h>
#ifdef HAVE_KSTAT
#include <kstat.h>
#endif

int get_load(double *loads);

int main()
{
  double loads[3];
  if ( !get_load(loads) ) {
    printf("%.2lf, %.2lf, %.2lf\n", loads[0]/100, loads[1]/100, loads[2]/100); 
    printf("run......\n");
  }
  return 0;
} /* main() */

/*

*/
int get_load(double *loads)
{
#ifdef HAVE_KSTAT
  kstat_ctl_t *kc;
  kstat_t *ksp;
  kstat_named_t *kn;

  kc = kstat_open();
  if (kc == 0)
  {
    perror("kstat_open");
    exit(1);
  }

  ksp = kstat_lookup(kc, "unix", 0, "system_misc");
  if (ksp == 0)
  {
    perror("kstat_lookup");
    exit(1);
  }
  if (kstat_read(kc, ksp,0) == -1) 
  {
    perror("kstat_read");
    exit(1);
  }

  kn = kstat_data_lookup(ksp, "avenrun_1min");
  if (kn == 0) 
  {
    fprintf(stderr,"not found\n");
    exit(1);
  }
  loads[0] = kn->value.ul/(FSCALE/100);

  kn = kstat_data_lookup(ksp, "avenrun_5min");
  if (kn == 0)
  {
    fprintf(stderr,"not found\n");
    exit(1);
  }
  loads[1] = kn->value.ul/(FSCALE/100);

  kn = kstat_data_lookup(ksp, "avenrun_15min");
  if (kn == 0) 
  {
    fprintf(stderr,"not found\n");
    exit(1);
  }
  loads[2] = kn->value.ul/(FSCALE/100);

  kstat_close(kc);
  return 0;
#else
  /* yes, this isn't right */
  loads[0] = loads[1] = loads[2] = 0;
  return -1;
#endif
} /* get_load() */