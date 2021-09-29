/***************************************************************************
 *  Description:
 *      Gather resource availability for a compute node.  Output is meant
 *      to be parsed by lpjs-dispatch in order to build a node list.
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-23  Jason Bacon Begin
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/utsname.h>

int     main(int argc,char *argv[])

{
    unsigned int    cpus = 0;
    unsigned long   user_mem = 0;
    struct utsname  u_name;
    
    /*
     *  hwloc is extremely complex and we don't need most of its functionality
     *  here, so just gather info the simple way
     */
    
    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    user_mem = sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
    printf("CPUs\t%u\n", cpus);
    printf("Physmem\t%lu\n", user_mem / 1024 / 1024);
    /*
     *  Report 1 if ZFS filesystem found, so that additional memory
     *  can be reserved on compute nodes.
     *  FIXME: There should be a better approach to this.
     */
    printf("ZFS\t%u\n", ! system("mount | fgrep -q zfs"));
    uname(&u_name);
    printf("OS\t%s\nArch\t%s\n", u_name.sysname, u_name.machine);
    return EX_OK;
}
