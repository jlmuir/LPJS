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
#include <errno.h>
#include <munge.h>
#include <stdbool.h>
#include <xtend/proc.h>
#include "lpjs.h"
#include "node-list.h"
#include "job-list.h"
#include "config.h"
#include "scheduler.h"
#include "network.h"
#include "misc.h"

// Must be global for signal handler
int     Listen_fd, Msg_fd;
FILE    *Log_stream;

int     main(int argc,char *argv[])

{
    node_list_t node_list;
    job_list_t  job_list;
    
    if ( argc > 2 )
    {
	fprintf (stderr, "Usage: %s [--daemonize]\n", argv[0]);
	return EX_USAGE;
    }
    else if ( (argc == 2) && (strcmp(argv[1],"--daemonize") == 0 ) )
    {
	/*
	 *  Code run after this must not attempt to write to stdout or stderr
	 *  since they will be closed.  Use lpjs_log() for all informative
	 *  messages.
	 */
	Log_stream = fopen("/var/log/lpjs_dispatchd", "a");
	if ( Log_stream == NULL )
	{
	    perror("Cannot open /var/log/lpjs_dispatchd");
	    return EX_CANTCREAT;
	}
	xt_daemonize(0, 0);
    }
    else
	Log_stream = stderr;
    
    lpjs_load_config(&node_list, LPJS_CONFIG_ALL, Log_stream);
    job_list_init(&job_list);

    /*
     *  Set handler so that Listen_fd is properly closed before termination.
     *  Still getting bind(): address already in use during testing
     *  with its frequent restarts.  Possible clues:
     *  https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
     */
    signal(SIGINT, terminate_daemon);
    signal(SIGTERM, terminate_daemon);

    return process_events(&node_list, &job_list);
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
    lpjs_log(Log_stream, "lpjs_dispatch shutting down...\n");
    fclose(Log_stream);
    close(Listen_fd);
    close(Msg_fd);
    
    exit(EX_OK);
}


/***************************************************************************
 *  Description:
 *      Listen for messages on LPJS_TCP_PORT and respond with either info
 *      (lpjs-nodes, lpjs-jobs, etc.) or actions (lpjs-submit).
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-25  Jason Bacon Begin
 ***************************************************************************/

int     process_events(node_list_t *node_list, job_list_t *job_list)

{
    ssize_t     bytes;
    socklen_t   address_len = sizeof (struct sockaddr_in);
    char        incoming_msg[LPJS_IP_MSG_MAX + 1];
    struct sockaddr_in server_address = { 0 };  // FIXME: Support IPV6
    uid_t       uid;
    gid_t       gid;
    munge_err_t munge_status;
    node_t      new_node;

    /*
     *  Create a socket endpoint to pair with the endpoint on the client.
     *  AF_INET and PF_INET have the same value, but PF_INET is more
     *  correct according to BSD and Linux man pages, which indicate
     *  that a protocol family should be specified.  In theory, a
     *  protocol family can support more than one address family.
     *  SOCK_STREAM indicates a reliable stream oriented protocol,
     *  such as TCP, vs. unreliable unordered datagram protocols like UDP.
     */
    if ((Listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
	lpjs_log(Log_stream, "Error opening socket.\n");
	return EX_UNAVAILABLE;
    }
    lpjs_log(Log_stream, "Listen_fd = %d\n", Listen_fd);

    // Convert 16-bit port number to network byte order
    server_address.sin_port = htons(LPJS_TCP_PORT);
    
    // inet4
    server_address.sin_family = AF_INET;
    
    // Convert 32-bit host address to network byte order
    // INADDR_ANY is probably 0, but we may bind to a specific
    // IP address in the future
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind socket fd to server address structure */
    if ( bind(Listen_fd, (struct sockaddr *) &server_address,
	      sizeof (server_address)) < 0 )
    {
	lpjs_log(Log_stream, "bind() failed: %s", strerror(errno));
	return EX_UNAVAILABLE;
    }
    lpjs_log(Log_stream, "Bound to port %d...\n", LPJS_TCP_PORT);
    
    // FIXME: This is just a skeletal loop for testing basic
    // connections between dispatchd and compd
    
	/* Listen for connection requests */
	if (listen(Listen_fd, LPJS_MSG_QUEUE_MAX) != 0)
	{
	    lpjs_log(Log_stream, "listen() failed.\n", stderr);
	    return EX_UNAVAILABLE;
	}

    while ( true )
    {
	/*
	 *  Check last ping time from every node
	 */
	
	// ping_all_nodes();
	
	// FIXME: Only accept requests from cluster nodes
	/* Accept a connection request */
	if ((Msg_fd = accept(Listen_fd,
		(struct sockaddr *)&server_address, &address_len)) == -1)
	{
	    lpjs_log(Log_stream, "accept() failed.\n", stderr);
	    return EX_UNAVAILABLE;
	}
	lpjs_log(Log_stream, "Accepted connection. fd = %d\n", Msg_fd);
    
	/* Read a message through the socket */
	if ( (bytes = read(Msg_fd, incoming_msg, 100)) == -1 )
	{
	    lpjs_log(Log_stream, "read() failed: %s", strerror(errno));
	    return EX_IOERR;
	}

	/* Process request */
	// FIXME: Check for duplicate checkins.  We should not get
	// a checkin request while one is already open
	if ( memcmp(incoming_msg, "compd-checkin", 13) == 0 )
	{
	    // Debug
	    // lpjs_log(Log_stream, "compd checkin.\n");
	    
	    /* Get munge credential */
	    // FIXME: What is the maximum cred length?
	    if ( (bytes = read(Msg_fd, incoming_msg, 4096)) == - 1)
	    {
		lpjs_log(Log_stream, "read() failed: %s", strerror(errno));
		return EX_IOERR;
	    }
	    munge_status = munge_decode(incoming_msg, NULL, NULL, 0, &uid, &gid);
	    if ( munge_status != EMUNGE_SUCCESS )
		lpjs_log(Log_stream, "munge_decode() failed.  Error = %s\n",
			 munge_strerror(munge_status));
	    lpjs_log(Log_stream, "Checkin from %d, %d\n", uid, gid);
	    
	    // FIXME: Only accept compd checkins from root

	    /*
	     *  Get specs from node and add Msg_fd
	     */
	    
	    // Debug
	    send_msg(Msg_fd, "Ident verified.\n");
	    
	    node_receive_specs(&new_node, Msg_fd);
	    node_print_status(&new_node);
	    node_list_update_compute(node_list, &new_node, Log_stream);
	    
	    // node_set_socket_fd(node, Msg_fd);
	    // Acknoledge checkin
	}
	else if ( strcmp(incoming_msg, "nodes") == 0 )
	{
	    lpjs_log(Log_stream, "Request for node status.\n");
	    node_list_send_status(Msg_fd, node_list);
	}
	else if ( strcmp(incoming_msg, "jobs") == 0 )
	{
	    lpjs_log(Log_stream, "Request for job status.\n");
	    job_list_send_params(Msg_fd, job_list);
	}
	else if ( memcmp(incoming_msg, "submit", 6) == 0 )
	{
	    // FIXME: Don't accept job submissions from root until
	    // security issues have been considered
	    
	    lpjs_log(Log_stream, "Request for job submission.\n");
	    
	    /* Get munge credential */
	    // FIXME: What is the maximum cred length?
	    if ( (bytes = read(Msg_fd, incoming_msg, 4096)) == - 1)
	    {
		lpjs_log(Log_stream, "read() failed: %s", strerror(errno));
		return EX_IOERR;
	    }
	    munge_status = munge_decode(incoming_msg, NULL, NULL, 0, &uid, &gid);
	    if ( munge_status != EMUNGE_SUCCESS )
		lpjs_log(Log_stream, "munge_decode() failed.  Error = %s\n",
			 munge_strerror(munge_status));
	    lpjs_log(Log_stream, "Submit from %d, %d\n", uid, gid);
	    queue_job(Msg_fd, incoming_msg, node_list);
	}
	else if ( memcmp(incoming_msg, "job-complete", 12) == 0 )
	{
	    lpjs_log(Log_stream, "Job completion report.\n");
	    log_job(incoming_msg);
	}
	close(Msg_fd);
    }
    close(Listen_fd);
    return EX_OK;
}


/***************************************************************************
 *  Description:
 *      Record job info such as command, exit status, run time, etc.
 *      Incoming message is sent by lpjs-chaperone when its child
 *      (a dispatched computational process) terminates.
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-28  Jason Bacon Begin
 ***************************************************************************/

void    log_job(const char *incoming_msg)

{
    lpjs_log(Log_stream, incoming_msg);
}
