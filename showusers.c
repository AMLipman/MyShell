/* file: showusers.c
   Ben Miller, for cisc361

   This is just a tiny main() to show how to get utmpx entries and printout
   info about logged in users with entries.  Read some man pages to find out
   how these functions work more...
*/
#include <utmpx.h>
#include <stdio.h>

int main()
{
  struct utmpx *up;

  setutxent();			/* start at beginning */
  while (up = getutxent() )	/* get an entry */
  {
    if ( up->ut_type == USER_PROCESS )	/* only care about users */
    {
      printf("%s has logged on %s from %s\n", up->ut_user, up->ut_line, up ->ut_host);
    }
  }
  return 0;
} /* main() */

