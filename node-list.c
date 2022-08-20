#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include <xtend/dsv.h>      // dsv_read_field()
#include <xtend/string.h>   // strtrim()
#include <xtend/file.h>
#include "node-list.h"
#include "network.h"
#include "lpjs.h"

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
			      const char *conf_file, FILE *error_stream)

{
    int     delim;
    char    field[LPJS_FIELD_MAX+1];
    size_t  len;
    
    while ( ((delim = dsv_read_field(input_stream, field, LPJS_FIELD_MAX+1,
				     ",", &len)) != '\n') &&
	    (delim != EOF) )
    {
	strtrim(field, " ");
	node_init(&node_list->compute_nodes[node_list->count]);
	node_set_hostname(&node_list->compute_nodes[node_list->count], strdup(field));
	++node_list->count;
    }
    if ( delim == EOF )
    {
	fprintf(error_stream, "Unexpected EOF reading %s.\n", conf_file);
	exit(EX_DATAERR);
    }
    
    // Add last node read by while condition
    strtrim(field, " ");
    node_init(&node_list->compute_nodes[node_list->count]);
    node_set_hostname(&node_list->compute_nodes[node_list->count], strdup(field));
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

void    node_list_update_compute(node_list_t *node_list, node_t *node,
				 FILE *error_stream)

{
    size_t  c;
    char    short_hostname[64],
	    *first_dot;
    
    strlcpy(short_hostname, NODE_HOSTNAME(node), 64);
    if ( (first_dot = strchr(short_hostname, '.')) != NULL )
	*first_dot = '\0';
    for (c = 0; c < node_list->count; ++c)
    {
	fprintf(error_stream, "%s\n",
		NODE_HOSTNAME(&node_list->compute_nodes[c]));
	if ( strcmp(NODE_HOSTNAME(&node_list->compute_nodes[c]), short_hostname) == 0 )
	{
	    fprintf(error_stream, "Updating compute node %zu %s\n", c, NODE_HOSTNAME(&node_list->compute_nodes[c]));
	    node_set_state(&node_list->compute_nodes[c], "Up");
	    node_set_cores(&node_list->compute_nodes[c], NODE_CORES(node));
	    node_set_phys_mem(&node_list->compute_nodes[c], NODE_PHYS_MEM(node));
	    node_set_zfs(&node_list->compute_nodes[c], NODE_ZFS(node));
	    node_set_os(&node_list->compute_nodes[c], strdup(NODE_OS(node)));
	    node_set_arch(&node_list->compute_nodes[c], strdup(NODE_ARCH(node)));
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
    unsigned    c;
    
    xt_dprintf(msg_fd, NODE_STATUS_HEADER_FORMAT, "Hostname", "State",
	    "Cores", "Used", "Physmem", "Used", "OS", "Arch");
    for (c = 0; c < node_list->count; ++c)
	node_send_status(&node_list->compute_nodes[c], msg_fd);
}
