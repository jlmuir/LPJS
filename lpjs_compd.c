/***************************************************************************
 *  Description:
 *      LPJS compute node daemon.  Checks in with lpfs-dispatchd to signal
 *      that node is up and starts computational processes on compute nodes.
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-30  Jason Bacon Begin
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <poll.h>
#include <munge.h>
#include <xtend/string.h>
#include <xtend/file.h>
#include <xtend/proc.h>
#include "lpjs.h"
#include "node-list.h"
#include "config.h"
#include "network.h"
#include "misc.h"
#include "lpjs_compd.h"

int     main (int argc, char *argv[])
{
    node_list_t node_list;
    node_t      node;
    char        incoming_msg[LPJS_MSG_LEN_MAX + 1];
    ssize_t     bytes;
    int         msg_fd,
		status;
    extern FILE *Log_stream;
    struct pollfd   poll_fd;

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
	Log_stream = fopen("/var/log/lpjs_compd", "a");
	if ( Log_stream == NULL )
	{
	    perror("Cannot open /var/log/lpjs_compd");
	    return EX_CANTCREAT;
	}
	xt_daemonize(0, 0);
    }
    else
	Log_stream = stderr;

    /*
     *  Set handler so that Listen_fd is properly closed before termination.
     *  Still getting bind(): address alread in use during testing
     *  with its frequent restarts.  Possible clues:
     *  https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
     */
    signal(SIGINT, lpjs_terminate_handler);
    signal(SIGTERM, lpjs_terminate_handler);
    
    // Get hostname of head node
    lpjs_load_config(&node_list, LPJS_CONFIG_HEAD_ONLY, Log_stream);

    // FIXME: Retry, at least if running in daemon mode
    if ( (msg_fd = lpjs_connect_to_dispatchd(&node_list)) == -1 )
    {
	perror("lpjs-nodes: Failed to connect to dispatchd");
	return EX_IOERR;
    }

    if ( (status = lpjs_compd_checkin(msg_fd, &node)) != EX_OK )
    {
	lpjs_log("compd-checkin failed.\n");
	exit(status);
    }
    
    poll_fd.fd = msg_fd;
    poll_fd.events = POLLIN | LPJS_POLLHUP;    // POLLERR and POLLHUP always set
    
    // Now keep daemon running, awaiting jobs
    // Almost correct: https://unix.stackexchange.com/questions/581426/how-to-get-notified-when-the-other-end-of-a-socketpair-is-closed
    while ( true )
    {
	poll(&poll_fd, 1, 2000);
	printf("Back from poll().  revents = %08x\n", poll_fd.revents);
	
	// FIXME: Send regular pings to lpjs_dispatchd?
	// Or monitor compd daemons with a separate process that
	// sends events to dispatchd?
	
	if (poll_fd.revents & LPJS_POLLHUP)
	{
	    // Close this end, or dispatchd gets "address already in use"
	    // When trying to restart
	    close(msg_fd);
	    
	    lpjs_log("Lost connection to dispatchd\n");
	    while ( (msg_fd = lpjs_connect_to_dispatchd(&node_list)) == -1 )
	    {
		int     retry_time = 10;
		sleep(retry_time);
		lpjs_log("Reconnect failed.  Retry in %d seconds...\n", retry_time);
	    }
	    
	    if ( lpjs_compd_checkin(msg_fd, &node) == EX_OK )
		lpjs_log("Connection reestablished.\n");
	    else
	    {
		lpjs_log("compd-checkin failed.\n");
		exit(status);
	    }
	}
	
	if (poll_fd.revents & POLLERR) {
	    lpjs_log("Error occurred polling dispatchd: %s\n", strerror(errno));
	    break;
	}
	
	if (poll_fd.revents & POLLIN)
	{
	    bytes = lpjs_recv_msg(msg_fd, incoming_msg, LPJS_MSG_LEN_MAX, 0);
	    incoming_msg[bytes] = '\0';
	    printf("Received from dispatchd: %s\n", incoming_msg);
	}
	poll_fd.revents = 0;
    }

    close(msg_fd);
    return EX_IOERR;
}


int     lpjs_compd_checkin(int msg_fd, node_t *node)

{
    char        *cred,
		outgoing_msg[LPJS_MSG_LEN_MAX + 1],
		incoming_msg[LPJS_MSG_LEN_MAX + 1];
    munge_err_t munge_status;
    size_t      bytes;
    
    /* Send a message to the server */
    /* Need to send \0, so xt_dprintf() doesn't work here */
    outgoing_msg[0] = LPJS_REQUEST_COMPD_CHECKIN;
    outgoing_msg[1] = '\0';
    if ( lpjs_send_msg(msg_fd, 0, outgoing_msg) < 0 )
    {
	perror("lpjs-nodes: Failed to send checkin message to dispatchd");
	close(msg_fd);
	return EX_IOERR;
    }

    if ( (munge_status = munge_encode(&cred, NULL, NULL, 0)) != EMUNGE_SUCCESS )
    {
	lpjs_log("lpjs_compd: munge_encode() failed.\n");
	lpjs_log("Return code = %s\n", munge_strerror(munge_status));
	return EX_UNAVAILABLE; // FIXME: Check actual error
    }

    printf("Sending %zd bytes: %s...\n", strlen(cred), cred);
    if ( lpjs_send_msg(msg_fd, 0, cred) < 0 )
    {
	perror("lpjs_compd: Failed to send credential to dispatchd");
	close(msg_fd);
	free(cred);
	return EX_IOERR;
    }
    free(cred);
    
    // This is needed before node_send_specs().
    // Can't write to socket before reading response to cred.
    bytes = lpjs_recv_msg(msg_fd, incoming_msg, LPJS_MSG_LEN_MAX, 0);
    lpjs_log("Response: %zu '%s'\n", bytes, incoming_msg);
    if ( strcmp(incoming_msg, "Ident verified") != 0 )
    {
	lpjs_log("lpjs_compd_checkin(): Expected \"Ident verified\".\n");
	return EX_DATAERR;
    }
    
    node_detect_specs(node);
    node_send_specs(node, msg_fd);
    
    return EX_OK;
}
