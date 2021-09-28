#ifndef _NODE_H_
#define _NODE_H_

typedef struct
{
    char            *hostname;
    unsigned        cores;
    unsigned        cores_used;
    unsigned long   mem;
    unsigned long   mem_used;
    int             zfs;        // 0 or 1
}   node_t;

#define NODE_SPEC_HEADER_FORMAT "%-30s %5s %5s %7s %7s %3s\n"
#define NODE_SPEC_FORMAT        "%-30s %5u %5u %7lu %7lu %3s\n"

/* Return values for mutator functions */
#define NODE_DATA_OK            0
#define NODE_DATA_INVALID       -1      // Catch-all for non-specific error
#define NODE_DATA_OUT_OF_RANGE  -2

/*
 *  Generated by /usr/local/bin/auto-gen-get-set
 *
 *  Accessor macros.  Use these to access structure members from functions
 *  outside the node_t class.
 *
 *  These generated macros are not expected to be perfect.  Check and edit
 *  as needed before adding to your code.
 */

#define NODE_HOSTNAME(ptr)              ((ptr)->hostname)
#define NODE_HOSTNAME_AE(ptr,c)         ((ptr)->hostname[c])
#define NODE_CORES(ptr)                 ((ptr)->cores)
#define NODE_CORES_USED(ptr)            ((ptr)->cores_used)
#define NODE_MEM(ptr)                   ((ptr)->mem)
#define NODE_MEM_USED(ptr)              ((ptr)->mem_used)
#define NODE_ZFS_STR(ptr)               ((ptr)->zfs == 0 ? "no" : "yes")
/*
 *  Generated by /usr/local/bin/auto-gen-get-set
 *
 *  Mutator macros for setting with no sanity checking.  Use these to
 *  set structure members from functions outside the node_t
 *  class.  These macros perform no data validation.  Hence, they achieve
 *  maximum performance where data are guaranteed correct by other means.
 *  Use the mutator functions (same name as the macro, but lower case)
 *  for more robust code with a small performance penalty.
 *
 *  These generated macros are not expected to be perfect.  Check and edit
 *  as needed before adding to your code.
 */

#define NODE_SET_HOSTNAME(ptr,val)              ((ptr)->hostname = (val))
#define NODE_SET_HOSTNAME_CPY(ptr,val,array_size) strlcpy((ptr)->hostname,val,array_size)
#define NODE_SET_HOSTNAME_AE(ptr,c,val)         ((ptr)->hostname[c] = (val))
#define NODE_SET_CORES(ptr,val)                 ((ptr)->cores = (val))
#define NODE_SET_CORES_USED(ptr,val)            ((ptr)->cores_used = (val))
#define NODE_SET_MEM(ptr,val)                   ((ptr)->mem = (val))
#define NODE_SET_MEM_USED(ptr,val)              ((ptr)->mem_used = (val))

/* node.c */
void    node_init(node_t *node);
int     node_get_specs(node_t *node);
void    node_print_specs(node_t *node);
void    node_send_specs(int fd, node_t *node);

/* node-mutators.c */
int node_set_hostname(node_t *node_ptr, char *new_hostname);
int node_set_hostname_ae(node_t *node_ptr, size_t c, char new_hostname_element);
int node_set_hostname_cpy(node_t *node_ptr, char *new_hostname, size_t array_size);
int node_set_cores(node_t *node_ptr, unsigned new_cores);
int node_set_cores_used(node_t *node_ptr, unsigned new_cores_used);
int node_set_mem(node_t *node_ptr, unsigned long new_mem);
int node_set_mem_used(node_t *node_ptr, unsigned long new_mem_used);

#endif  // _NODE_H_
