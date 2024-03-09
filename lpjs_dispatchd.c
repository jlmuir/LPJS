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
#include <sys/select.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <munge.h>
#include <xtend/proc.h>
#include <xtend/file.h>     // xt_rmkdir()

#include "lpjs.h"
#include "node-list.h"
#include "job-list.h"
#include "config.h"
#include "scheduler.h"
#include "network.h"
#include "misc.h"
#include "lpjs_dispatchd.h"

int     main(int argc,char *argv[])

{
    node_list_t *node_list = node_list_new();
    job_list_t  job_list;
    
    // Must be global for signal handler
    // FIXME: Maybe use ucontext to pass these to handler
    extern FILE         *Log_stream;
    extern node_list_t  *Node_list;
    
    Node_list = node_list;
    
    if ( argc > 2 )
    {
	fprintf (stderr, "Usage: %s [--daemonize|--log-output]\n", argv[0]);
	return EX_USAGE;
    }
    else if ( (argc == 2) && (strcmp(argv[1],"--daemonize") == 0 ) )
    {
	if ( (Log_stream = lpjs_log_output(LPJS_DISPATCHD_LOG)) == NULL )
	    return EX_CANTCREAT;

	/*
	 *  Code run after this must not attempt to write to stdout or stderr
	 *  since they will be closed.  Use lpjs_log() for all informative
	 *  messages.
	 *  FIXME: Prevent unchecked log growth
	 */
	xt_daemonize(0, 0);
    }
    else if ( (argc == 2) && (strcmp(argv[1],"--log-output") == 0 ) )
    {
	if ( (Log_stream = lpjs_log_output(LPJS_DISPATCHD_LOG)) == NULL )
	    return EX_CANTCREAT;
    }
    else
	Log_stream = stderr;

    // Parent of all new job directories
    if ( xt_rmkdir(LPJS_PENDING_DIR, 0755) != 0 )
    {
	fprintf(stderr, "Cannot create %s: %s\n", LPJS_PENDING_DIR, strerror(errno));
	return -1;  // FIXME: Define error codes
    }

    // Parent of all running job directories
    if ( xt_rmkdir(LPJS_RUNNING_DIR, 0755) != 0 )
    {
	fprintf(stderr, "Cannot create %s: %s\n", LPJS_RUNNING_DIR, strerror(errno));
	return -1;  // FIXME: Define error codes
    }

#ifdef __linux__    // systemd needs a pid file for forking daemons
    int         status;
    extern char Pid_path[PATH_MAX + 1];
    
    if ( xt_rmkdir(LPJS_RUN_DIR, 0755) != 0 )
	return EX_CANTCREAT;
    
    snprintf(Pid_path, PATH_MAX + 1, "%s/%s.pid", LPJS_RUN_DIR, "lpjs_compd");
    status = xt_create_pid_file(Pid_path, Log_stream);
    if ( status != EX_OK )
	return status;
#endif

    lpjs_load_config(node_list, LPJS_CONFIG_ALL, Log_stream);
    job_list_init(&job_list);
    
    /*
     *  bind(): address already in use during testing with frequent restarts.
     *  Best approach is to ensure that client completes a close
     *  before the server closes.
     *  https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
     *  Copy saved in ./bind-address-already-in-use.pdf
     */
    signal(SIGINT, lpjs_terminate_handler);
    signal(SIGTERM, lpjs_terminate_handler);

    return lpjs_process_events(node_list, &job_list);
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

int     lpjs_process_events(node_list_t *node_list, job_list_t *job_list)

{
    int                 listen_fd;
    struct sockaddr_in  server_address = { 0 };

    /*
     *  Step 1: Create a socket for listening for new connections.
     */
    
    listen_fd = lpjs_listen(&server_address);

    /*
     *  Step 2: Accept new connections, and create a separate socket
     *  for communication with each new compute node.
     */
    
    while ( true )
    {
	fd_set  read_fds;
	int     nfds, highest_fd;
	
	FD_ZERO(&read_fds);
	FD_SET(listen_fd, &read_fds);
	highest_fd = listen_fd;
	for (unsigned c = 0; c < node_list_get_compute_node_count(node_list); ++c)
	{
	    node_t *node = node_list_get_compute_nodes_ae(node_list, c);
	    //lpjs_log("Checking node %s, fd = %d...\n",
	    //        node_get_hostname(node), node_get_msg_fd(node));
	    if ( node_get_msg_fd(node) != NODE_MSG_FD_NOT_OPEN )
	    {
		FD_SET(node_get_msg_fd(node), &read_fds);
		if ( node_get_msg_fd(node) > highest_fd )
		    highest_fd = node_get_msg_fd(node);
	    }
	}
	// lpjs_log("highest_fd = %d\n", highest_fd);
	nfds = highest_fd + 1;
	
	if ( select(nfds, &read_fds, NULL, NULL, LPJS_NO_SELECT_TIMEOUT) > 0 )
	{
	    // lpjs_log("Back from select.\n");
	    
	    lpjs_check_comp_fds(&read_fds, node_list, job_list);
	    lpjs_check_listen_fd(listen_fd, &read_fds, &server_address,
				 node_list, job_list);
	}
	else
	    lpjs_log("select() returned 0.\n");
    }
    close(listen_fd);
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

void    lpjs_log_job(const char *incoming_msg)

{
    // FIXME: Add extensive info about the job
    lpjs_log(incoming_msg);
}


/***************************************************************************
 *  Description:
 *      Check all connected sockets for messages
 *
 *  History: 
 *  Date        Name        Modification
 *  2024-01-22  Jason Bacon Factor out from lpjs_process_events()
 ***************************************************************************/

void    lpjs_check_comp_fds(fd_set *read_fds, node_list_t *node_list,
			    job_list_t *job_list)

{
    node_t  *node = node_new();
    int     fd;
    ssize_t bytes;
    char    incoming_msg[LPJS_MSG_LEN_MAX + 1];
    
    // Top priority: Active compute nodes (move existing jobs along)
    // Second priority: New compute node checkins (make resources available)
    // Lowest priority: User commands
    for (unsigned c = 0; c < node_list_get_compute_node_count(node_list); ++c)
    {
	node = node_list_get_compute_nodes_ae(node_list, c);
	fd = node_get_msg_fd(node);
	if ( (fd != NODE_MSG_FD_NOT_OPEN) && FD_ISSET(fd, read_fds) )
	{
	    lpjs_log("Activity on fd %d\n", fd);
	    
	    /*
	     *  select() returns when a peer has closed the connection.
	     *  lpjs_recv() will return 0 in this case.
	     */
	    
	    bytes = lpjs_recv(fd, incoming_msg, LPJS_MSG_LEN_MAX, 0, 0);
	    if ( bytes == 0 )
	    {
		lpjs_log("Lost connection to %s.  Closing...\n",
			node_get_hostname(node));
		close(fd);
		node_set_msg_fd(node, NODE_MSG_FD_NOT_OPEN);
		node_set_state(node, "Down");
	    }
	    else
	    {
		switch(incoming_msg[0])
		{
		    case    LPJS_NOTICE_JOB_COMPLETE:
		    
			/*
			 *  Remove job from running dir and write a
			 *  completed job record
			 *  Note the job completion in the main log
			 */
			
			lpjs_log("Job completion report.\n");
			lpjs_log_job(incoming_msg);
			lpjs_dispatch_jobs(node_list, job_list);
			break;
			
		    default:
			lpjs_log("Invalid notification on fd %d: %d\n",
				fd, incoming_msg[0]);
		}
	    }
	}
    }
}


/***************************************************************************
 *  Description:
 *      Create listener socket
 *
 *  History: 
 *  Date        Name        Modification
 *  2024-01-22  Jason Bacon Factor out from lpjs_process_events()
 ***************************************************************************/

int     lpjs_listen(struct sockaddr_in *server_address)

{
    int     listen_fd;
    
    /*
     *  Create a socket endpoint to pair with the endpoint on the client.
     *  This only creates a file descriptor.  It is not yet bound to
     *  any network interface and port.
     *  AF_INET and PF_INET have the same value, but PF_INET is more
     *  correct according to BSD and Linux man pages, which indicate
     *  that a protocol family should be specified.  In theory, a
     *  protocol family can support more than one address family.
     *  SOCK_STREAM indicates a reliable stream oriented protocol,
     *  such as TCP, vs. unreliable unordered datagram protocols like UDP.
     */
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
	lpjs_log("Error opening listener socket.\n");
	exit(EX_UNAVAILABLE);
    }

    /*
     *  Port on which to listen for new connections from compute nodes.
     *  Convert 16-bit port number from host byte order to network byte order.
     */
    server_address->sin_port = htons(LPJS_IP_TCP_PORT);
    
    // AF_INET = inet4, AF_INET6 = inet6
    server_address->sin_family = AF_INET;
    
    /*
     *  Listen on all local network interfaces for now (INADDR_ANY).
     *  We may allow the user to specify binding to a specific IP address
     *  in the future, for multihomed servers acting as gateways, etc.
     *  Convert 32-bit host address to network byte order.
     */
    server_address->sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket fd and server address
    while ( bind(listen_fd, (struct sockaddr *)server_address,
	      sizeof (*server_address)) < 0 )
    {
	lpjs_log("bind() failed: %s\n", strerror(errno));
	lpjs_log("Retry in 10 seconds...\n");
	sleep(10);
    }
    lpjs_log("Bound to port %d...\n", LPJS_IP_TCP_PORT);
    
    /*
     *  Create queue for incoming connection requests
     */
    if (listen(listen_fd, LPJS_CONNECTION_QUEUE_MAX) != 0)
    {
	lpjs_log("listen() failed.\n");
	exit(EX_UNAVAILABLE);
    }
    return listen_fd;
}



/***************************************************************************
 *  Description
 *      Process events arriving on the listening socket
 *
 *  History: 
 *  Date        Name        Modification
 *  2024-01-22  Jason Bacon Factor out from lpjs_process_events()
 ***************************************************************************/

int     lpjs_check_listen_fd(int listen_fd, fd_set *read_fds,
			     struct sockaddr_in *server_address,
			     node_list_t *node_list, job_list_t *job_list)

{
    int         msg_fd;
    ssize_t     bytes;
    char        *munge_payload;
    socklen_t   address_len = sizeof (struct sockaddr_in);
    uid_t       uid;
    gid_t       gid;
    
    bytes = 0;
    if ( FD_ISSET(listen_fd, read_fds) )
    {
	/* Accept a connection request */
	if ((msg_fd = accept(listen_fd,
		(struct sockaddr *)server_address, &address_len)) == -1)
	{
	    lpjs_log("accept() failed, even though select indicated listen_fd.\n");
	    exit(EX_SOFTWARE);
	}
	else
	{
	    // lpjs_log("Accepted connection. fd = %d\n", msg_fd);

	    /* Read a message through the socket */
	    if ( (bytes = lpjs_recv_munge(msg_fd,
			 &munge_payload, 0, 0, &uid, &gid)) < 1 )
	    {
		lpjs_log("lpjs_check_listen_fd(): lpjs_recv_munge() failed (%zd bytes): %s",
			bytes, strerror(errno));
		close(msg_fd);
		free(munge_payload);
		return bytes;
	    }
	    lpjs_log("%s(): Got %zd byte message.\n", __FUNCTION__, bytes);
	    // bytes must be at least 1, or no mem is allocated
	    munge_payload[bytes] = '\0';
	    lpjs_log("%s(): Request code = %d\n", __FUNCTION__, munge_payload[0]);
	    lpjs_log("%s(): %s\n", __FUNCTION__, munge_payload + 1);
	    /* Process request */
	    switch(munge_payload[0])
	    {
		case    LPJS_REQUEST_COMPD_CHECKIN:
		    lpjs_log("LPJS_REQUEST_COMPD_CHECKIN\n");
		    lpjs_process_compute_node_checkin(msg_fd, munge_payload,
						      node_list, uid, gid);
		    lpjs_dispatch_jobs(node_list, job_list);
		    break;
		
		case    LPJS_REQUEST_NODE_STATUS:
		    lpjs_log("Request for node status.\n");
		    node_list_send_status(msg_fd, node_list);
		    lpjs_server_safe_close(msg_fd);
		    break;
		
		case    LPJS_REQUEST_JOB_STATUS:
		    lpjs_log("Request for job status.\n");
		    job_list_send_params(msg_fd, job_list);
		    lpjs_server_safe_close(msg_fd);
		    break;
		
		case    LPJS_REQUEST_SUBMIT:
		    lpjs_submit(msg_fd, munge_payload, node_list, job_list,
				uid, gid);
		    lpjs_dispatch_jobs(node_list, job_list);
		    break;
		
		default:
		    lpjs_log("Invalid request on listen_fd: %d\n",
			    munge_payload[0]);
		    
	    }   // switch
	    free(munge_payload);
	}
    }
    
    return bytes;
}


/***************************************************************************
 *  Description:
 *      Process a compute node checkin request
 *
 *  History: 
 *  Date        Name        Modification
 *  2024-01-22  Jason Bacon Factor out from lpjs_process_events()
 ***************************************************************************/

void    lpjs_process_compute_node_checkin(int msg_fd, const char *incoming_msg,
					  node_list_t *node_list,
					  uid_t uid, gid_t gid)

{
    node_t      *new_node = node_new();
    extern FILE *Log_stream;
    
    // FIXME: Check for duplicate checkins.  We should not get
    // a checkin request while one is already open
    // lpjs_log("Munge credential message length = %zd\n", bytes);
    // lpjs_log("munge msg: %s\n", incoming_msg);
    
    lpjs_log("Checkin from uid %d, gid %d\n", uid, gid);
    
    // FIXME: Record username of compd checkin.  If not root, then only
    // that user can submit jobs to the node.

    /*
     *  Get specs from node and add msg_fd
     */
    
    // Extract specs from incoming_msg + 1
    // node_recv_specs(new_node, msg_fd);
    
    // +1 to skip command code
    node_str_to_specs(new_node, incoming_msg + 1);
    
    // Keep in sync with node_list_send_status()
    node_print_status_header(Log_stream);
    node_print_status(new_node, Log_stream);
    
    // Make sure node name is valid
    // Note: For real security, only authorized
    // nodes should be allowed to pass through
    // the network firewall.
    bool valid_node = false;
    for (unsigned c = 0; c < node_list_get_compute_node_count(node_list); ++c)
    {
	node_t *node = node_list_get_compute_nodes_ae(node_list, c);
	// If config has short hostnames, just match that
	int valid_hostname_len = strlen(node_get_hostname(node));
	if ( memcmp(node_get_hostname(node), node_get_hostname(new_node), valid_hostname_len) == 0 )
	    valid_node = true;
    }
    if ( ! valid_node )
    {
	lpjs_log("Unauthorized checkin request from %s.\n",
		node_get_hostname(new_node));
	close(msg_fd);
    }
    else
    {
	lpjs_send(msg_fd, 0, "Node authorized");
	node_set_msg_fd(new_node, msg_fd);
	
	// Nodes were added to node_list by lpjs_load_config()
	// Just update the fields here
	node_list_update_compute(node_list, new_node);
    }
}


/***************************************************************************
 *  Description:
 *      Add a new submission to the queue
 *  
 *  History: 
 *  Date        Name        Modification
 *  2024-01-22  Jason Bacon Factor out from lpjs_process_events()
 ***************************************************************************/

int     lpjs_submit(int msg_fd, const char *incoming_msg,
		    node_list_t *node_list, job_list_t *job_list,
		    uid_t uid, gid_t gid)

{
    char        script_path[PATH_MAX + 1],
		*end;
    job_t       *job = job_new(); // exits if malloc() fails, no need to check
    int         c;
    
    // FIXME: Don't accept job submissions from root until
    // security issues have been considered
    
    lpjs_log("Request for job submission.\n");
    
    // Payload in message from lpjs submit is a job description
    // in JOB_SPEC_FORMAT
    job_read_from_string(job, incoming_msg + 1, &end);
    
    // Need the absolute pathname of the script.  Might be the same
    // on compute nodes if NFS or other file server is used
    snprintf(script_path, PATH_MAX + 1, "%s/%s",
	    job_get_working_directory(job), job_get_script_name(job));
    for (c = 0; c < job_get_job_count(job); ++c)
    {
	lpjs_log("Submit script %s from %d, %d\n", script_path, uid, gid);
	lpjs_queue_job(msg_fd, job, script_path);
    }
    
    lpjs_server_safe_close(msg_fd);

    return EX_OK;
}


/***************************************************************************
 *  Description:
 *      Add a job to the queue
 *
 *  History: 
 *  Date        Name        Modification
 *  2021-09-30  Jason Bacon Begin
 ***************************************************************************/

int     lpjs_queue_job(int msg_fd, job_t *job, const char *script_path)

{
    char    pending_dir[PATH_MAX + 1],
	    pending_path[PATH_MAX + 1],
	    specs_path[PATH_MAX + 1],
	    job_id_path[PATH_MAX + 1],
	    outgoing_msg[LPJS_MSG_LEN_MAX + 1];
    FILE    *fp;
    int     status;
    unsigned long   next_job_id;
    
    lpjs_log("Spooling %s...\n", script_path);
    
    snprintf(job_id_path, PATH_MAX + 1, "%s/next-job", LPJS_SPOOL_DIR);
    if ( (fp = fopen(job_id_path, "r")) == NULL )
	next_job_id = 1;
    else
    {
	fscanf(fp, "%lu", &next_job_id);
	fclose(fp);
    }
    job_set_job_id(job, next_job_id);
    
    snprintf(pending_dir, PATH_MAX + 1, "%s/%lu", LPJS_PENDING_DIR,
	    next_job_id);
    if ( xt_rmkdir(pending_dir, 0755) != 0 )
    {
	fprintf(stderr, "Cannot create %s: %s\n", pending_dir, strerror(errno));
	return -1;  // FIXME: Define error codes
    }

    snprintf(pending_path, PATH_MAX + 1, "%s/%s", pending_dir,
	    xt_basename(script_path));
    
    // FIXME: Use a symlink instead?  Copy is safer in case user
    // modifies the script while a job is running.
    // lpjs_log("CWD = %s  script = '%s'\n", getcwd(NULL, 0), script_path);
    // lpjs_log("stat(): %d\n", stat(script_path, &st));
    if ( (status = xt_fast_cp(script_path, pending_path)) != 0 )
    {
	lpjs_log("lpjs_queue_job(): Failed to copy %s to %s: %d %s\n",
		script_path, pending_path, status, strerror(errno));
	return -1;
    }
    
    /*
     *  Write basic job specs to a file for the dispatcher
     */
    
    snprintf(specs_path, PATH_MAX + 1, "%s/job.specs", pending_dir);
    lpjs_log("Storing specs to %s.\n", specs_path);
    // Bump job num after successful spool
    if ( (fp = fopen(specs_path, "w")) == NULL )
    {
	lpjs_log("Cannot create %s: %s\n", specs_path, strerror(errno));
	return -1;
    }
    
    job_print(job, fp);
    fclose(fp);
    
    snprintf(outgoing_msg, LPJS_MSG_LEN_MAX, "Spooled job %lu to %s.\n",
	    next_job_id, pending_dir);
    lpjs_send_munge(msg_fd, outgoing_msg);
    
    // FIXME: Send this to the job log, not the daemon log
    lpjs_log(outgoing_msg);
    
    // Bump job num after successful spool
    if ( (fp = fopen(job_id_path, "w")) == NULL )
    {
	lpjs_log("Cannot update %s: %s\n", job_id_path, strerror(errno));
	return -1;
    }
    else
    {
	fprintf(fp, "%lu\n", ++next_job_id);
	fclose(fp);
    }
    
    return 0;   // FIXME: Define error codes
}

