#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>

#include <xtend/dsv.h>      // xt_dsv_read_field()
#include <xtend/string.h>   // xt_strtrim()
#include <xtend/file.h>

#include "node-list.h"
#include "network.h"
#include "lpjs.h"
#include "misc.h"

/***************************************************************************
 *  Description:
 *      Constructor for node_list_t
 *  
 *  History: 
 *  Date        Name        Modification
 *  2021-09-24  Jason Bacon Begin
 ***************************************************************************/

void    node_list_init(node_list_t *node_list)

{
    node_list->count = 0;
}


/***************************************************************************
 *  Description:
 *      Add a compute node to the list of configured nodes 
 *  
 *  History: 
 *  Date        Name        Modification
 *  2021-09-24  Jason Bacon Begin
 ***************************************************************************/

int     node_list_add_compute(node_list_t *node_list, FILE *input_stream,
			      const char *conf_file)

{
    int     delim;
    char    field[LPJS_FIELD_MAX + 1];
    size_t  len;
    
    while ( ((delim = xt_dsv_read_field(input_stream, field, LPJS_FIELD_MAX + 1,
				     ",", &len)) != '\n') &&
	    (delim != EOF) )
    {
	xt_strtrim(field, " ");
	node_list->compute_nodes[node_list->count] = node_new();
	node_set_hostname(node_list->compute_nodes[node_list->count], strdup(field));
	++node_list->count;
    }
    if ( delim == EOF )
    {
	lpjs_log("Unexpected EOF reading %s.\n", conf_file);
	exit(EX_DATAERR);
    }
    
    // Add last node read by while condition
    xt_strtrim(field, " ");
    node_list->compute_nodes[node_list->count] = node_new();
    node_set_hostname(node_list->compute_nodes[node_list->count], strdup(field));
    ++node_list->count;
    return 0;   // NL_OK?
}


/***************************************************************************
 *  Description:
 *      Update state and specs of a node after receiving info, e.g. from
 *      lpjs_compd
 *  
 *  History: 
 *  Date        Name        Modification
 *  2021-10-02  Jason Bacon Begin
 ***************************************************************************/

void    node_list_update_compute(node_list_t *node_list, node_t *node)

{
    size_t  c;
    char    short_hostname[64],
	    *first_dot;
    
    strlcpy(short_hostname, node_get_hostname(node), 64);
    if ( (first_dot = strchr(short_hostname, '.')) != NULL )
	*first_dot = '\0';
    for (c = 0; c < node_list->count; ++c)
    {
	if ( strcmp(node_get_hostname(node_list->compute_nodes[c]), short_hostname) == 0 )
	{
	    // lpjs_log("Updating compute node %zu %s\n", c, NODE_HOSTNAME(node_list->compute_nodes[c]));
	    node_set_state(node_list->compute_nodes[c], "Up");
	    node_set_cores(node_list->compute_nodes[c], node_get_cores(node));
	    node_set_phys_mem(node_list->compute_nodes[c], node_get_phys_mem(node));
	    node_set_zfs(node_list->compute_nodes[c], node_get_zfs(node));
	    node_set_os(node_list->compute_nodes[c], strdup(node_get_os(node)));
	    node_set_arch(node_list->compute_nodes[c], strdup(node_get_arch(node)));
	    node_set_msg_fd(node_list->compute_nodes[c], node_get_msg_fd(node));
	    node_set_last_ping(node_list->compute_nodes[c], node_get_last_ping(node));
	    return;
	}
    }
}


/***************************************************************************
 *  Description:
 *      Send current node list to msg_fd in human-readable format
 *  
 *  History: 
 *  Date        Name        Modification
 *  2021-09-26  Jason Bacon Begin
 ***************************************************************************/

void    node_list_send_status(int msg_fd, node_list_t *node_list)

{
    unsigned        c,
		    cores_up,
		    cores_up_used,
		    cores_down;
    unsigned long   mem_up,
		    mem_up_used,
		    mem_down;
    
    xt_dprintf(msg_fd, NODE_STATUS_HEADER_FORMAT, "Hostname", "State",
	    "Cores", "Used", "Physmem", "Used", "OS", "Arch");
    
    cores_up = cores_up_used = cores_down = 0;
    mem_up = mem_up_used = mem_down = 0;
    
    for (c = 0; c < node_list->count; ++c)
    {
	node_send_status(node_list->compute_nodes[c], msg_fd);
	if ( strcmp(node_get_state(node_list->compute_nodes[c]), "Up") == 0 )
	{
	    cores_up += node_get_cores(node_list->compute_nodes[c]);
	    cores_up_used += node_get_cores_used(node_list->compute_nodes[c]);
	    mem_up += node_get_phys_mem(node_list->compute_nodes[c]);
	    mem_up_used += node_get_phys_mem_used(node_list->compute_nodes[c]);
	}
	else
	{
	    cores_down += node_get_cores(node_list->compute_nodes[c]);
	    mem_down += node_get_phys_mem(node_list->compute_nodes[c]);
	}
    }
    
    xt_dprintf(msg_fd, "\n");
    xt_dprintf(msg_fd, NODE_STATUS_FORMAT, "Total", "Up",
		  cores_up, cores_up_used, mem_up, mem_up_used, "-", "-");
    xt_dprintf(msg_fd, NODE_STATUS_FORMAT, "Total", "Down",
		  cores_down, 0, mem_down, 0, "-", "-");
    
    /*
     *  Closing the listener first results in "address already in use"
     *  errors on restart.  Send an EOT character to signal the end of
     *  transmission, so the client can close first and avoid a wait
     *  state for the socket.
     */
    lpjs_send_eot(msg_fd);
}

