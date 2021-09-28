/***************************************************************************
 *  Description:
 *      This is the main controller daemon that runs on the head node.
 *      It listens for socket connections and takes requests for information
 *      from lpjs-nodes (for node status), lpjs-jobs (for job status),
 *      job submissions from lpjs-submit, and job completion reports from
 *      lpjs-chaperone.
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-23  Jason Bacon Begin
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include "node-list.h"
#include "config.h"
#include "scheduler.h"
#include "network.h"
#include "misc.h"
#include "lpjs.h"

int     Listen_fd;  // Must be global for signal handler

int     main(int argc,char *argv[])

{
    node_list_t node_list;
    
    lpjs_load_config(&node_list, LPJS_CONFIG_ALL);
    return process_events(&node_list);
}


/***************************************************************************
 *  Description:
 *      Gracefully shut down in the event of an interrupt signal
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-28  Jason Bacon Begin
 ***************************************************************************/

void    terminate_daemon(int s2)

{
    puts("Shutting down...");
    close(Listen_fd);
    exit(0);
}


/***************************************************************************
 *  Description:
 *      Listen for messages on LPJS_TCP_PORT.
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-25  Jason Bacon Begin
 ***************************************************************************/

int     process_events(node_list_t *node_list)

{
    int         msg_fd;
    ssize_t     bytes;
    short       tcp_port;   /* Need short for htons() */
    socklen_t   address_len = sizeof (struct sockaddr_in);
    char        incoming_msg[LPJS_IP_MSG_MAX + 1];
    // FIXME: Support IPV6
    struct sockaddr_in server_address = { 0 };

    /*
     *  Set handler so that Listen_fd is properly closed before termination.
     *  Still getting bind(): address alread in use during testing
     *  with its frequent restarts.  Possible clues:
     *  https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
     */
    signal(SIGINT, terminate_daemon);
    
    if ((Listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	lpjs_log("Error opening socket.\n");
	return EX_UNAVAILABLE;
    }
    lpjs_log("Listen_fd = %d\n", Listen_fd);

    // FIXME: Pick a good default and check config file for override
    tcp_port = LPJS_TCP_PORT;
    server_address.sin_port = htons(tcp_port);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind socket to server address */
    if (bind(Listen_fd, (struct sockaddr *) &server_address,
	      sizeof (server_address)) < 0)
    {
	perror("bind() failed");
	return EX_UNAVAILABLE;
    }
    lpjs_log("Bound to port %d...\n", tcp_port);
    
    while ( 1 )
    {
	/* Listen for connection requests */
	if (listen (Listen_fd, LPJS_MSG_QUEUE_MAX) != 0)
	{
	    fputs ("listen() failed.\n", stderr);
	    return EX_UNAVAILABLE;
	}
    
	/* Accept a connection request */
	if ((msg_fd = accept(Listen_fd,
		(struct sockaddr *)&server_address, &address_len)) == -1)
	{
	    fputs ("accept() failed.\n", stderr);
	    return EX_UNAVAILABLE;
	}
	lpjs_log("Accepted connection. fd = %d\n", msg_fd);
    
	/* Read a message through the socket */
	if ( (bytes = read(msg_fd, incoming_msg, 100)) == - 1)
	{
	    perror("read() failed");
	    return EX_IOERR;
	}
	// lpjs_log("%zu %s\n", bytes, incoming_msg);
	
	/* Process request */
	if ( strcmp(incoming_msg, "nodes") == 0 )
	{
	    lpjs_log("Request for node status.\n");
	    node_list_send_specs(msg_fd, node_list);
	}
	else if ( strcmp(incoming_msg, "jobs") == 0 )
	{
	    lpjs_log("Request for job status.\n");
	    dprintf(msg_fd, "Job status not yet implemented\n");
	}
	else if ( memcmp(incoming_msg, "submit", 6) == 0 )
	{
	    lpjs_log("Request for job submission.\n");
	    queue_job(msg_fd, incoming_msg, node_list);
	}
	else if ( memcmp(incoming_msg, "job-complete", 12) == 0 )
	{
	    lpjs_log("Job completion report.\n");
	    log_job(incoming_msg);
	}
	close(msg_fd);
    }
    close (Listen_fd);
    return EX_OK;
}


/***************************************************************************
 *  Description:
 *      Record job info such as command, exit status, run time, etc.
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-28  Jason Bacon Begin
 ***************************************************************************/

void    log_job(const char *incoming_msg)

{
    puts(incoming_msg);
}
