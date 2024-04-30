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

// System headers
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
#include <fcntl.h>      // open()
#include <pwd.h>        // getpwnam()
#include <grp.h>        // getgrnam()

// Addons
#include <munge.h>
#include <xtend/proc.h>
#include <xtend/file.h>     // xt_rmkdir()

// Project headers
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
    node_list_t *node_list = node_list_new();   // Exits if malloc() fails
    uid_t       daemon_uid;
    gid_t       daemon_gid;
    
    // FIXME: Load existing jobs from spool dir on startup
    
    // Must be global for signal handler
    // FIXME: Maybe use ucontext to pass these to handler
    extern FILE         *Log_stream;
    extern node_list_t  *Node_list;

    Node_list = node_list;
    Log_stream = stderr;
    
    // Silence compiler warnings about initialization
    daemon_uid = getuid();
    daemon_gid = getgid();
    
    for (int arg = 1; arg < argc; ++arg)
    {
	if ( strcmp(argv[arg],"--daemonize") == 0 )
	{
	    // Redirect lpjs_log() output from stderr to file
	    if ( (Log_stream = lpjs_log_output(LPJS_DISPATCHD_LOG)) == NULL )
		return EX_CANTCREAT;
    
	    /*
	     *  Code run after this must not attempt to write to stdout or
	     *  stderr since they will be closed.  Use lpjs_log() for all
	     *  informative messages.
	     *  FIXME: Prevent unchecked log growth
	     */
	    
	    xt_daemonize(0, 0);
	}
	else if ( strcmp(argv[arg],"--log-output") == 0 )
	{
	    /*
	     *  Redirect lpjs_log() output without daemonizing via fork().
	     *  Used by some platforms for services.
	     */

	    if ( (Log_stream = lpjs_log_output(LPJS_DISPATCHD_LOG)) == NULL )
		return EX_CANTCREAT;
	}
	else if ( strcmp(argv[arg], "--user") == 0 )
	{
	    /*
	     *  Set uid under which daemon should run, instead of root.
	     *  Just determine user for now.  Create system files before
	     *  giving up root privs.
	     */
	    
	    // pw_ent points to internal static object
	    // OK since dispatchd is not multithreaded
	    struct passwd *pw_ent;
	    char *user_name = argv[++arg];
	    if ( (pw_ent = getpwnam(user_name)) == NULL )
	    {
		lpjs_log("User %s does not exist.\n", user_name);
		return EX_NOUSER;
	    }
	    daemon_uid = pw_ent->pw_uid;
	}
	else if ( strcmp(argv[arg], "--group") == 0 )
	{
	    // gr_ent points to internal static object
	    // OK since dispatchd is not multithreaded
	    struct group *gr_ent;
	    char *group_name = argv[++arg];
	    if ( (gr_ent = getgrnam(group_name)) == NULL )
	    {
		lpjs_log("Group %s does not exist.\n", group_name);
		return EX_NOUSER;
	    }
	    daemon_gid = gr_ent->gr_gid;
	}
	else
	{
	    fprintf (stderr, "Usage: %s [--daemonize|--log-output] [--user username] [--group groupname]\n", argv[0]);
	    return EX_USAGE;
	}
    }
    
    // Make log file writable to daemon owner after root creates it
    chown(LPJS_LOG_DIR, daemon_uid, daemon_gid);
    chown(LPJS_DISPATCHD_LOG, daemon_uid, daemon_gid);
    
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
    
    // Make spool dir writable to daemon owner after root creates it
    chown(LPJS_PENDING_DIR, daemon_uid, daemon_gid);
    chown(LPJS_RUNNING_DIR, daemon_uid, daemon_gid);
    chown(LPJS_SPOOL_DIR "/next-job", daemon_uid, daemon_gid);

/*
 *  systemd needs a pid file for forking daemons.  BSD systems don't
 *  require this for rc scripts, so we don't bother with it.  PIDs
 *  are found dynamically there.
 */

#ifdef __linux__
    int         status;
    extern char Pid_path[PATH_MAX + 1];
    
    if ( xt_rmkdir(LPJS_RUN_DIR, 0755) != 0 )
	return EX_CANTCREAT;
    
    snprintf(Pid_path, PATH_MAX + 1, "%s/lpjs_compd.pid", LPJS_RUN_DIR);
    status = xt_create_pid_file(Pid_path, Log_stream);
    if ( status != EX_OK )
	return status;
    chown(LPJS_RUN_DIR, daemon_uid, daemon_gid);
    chown(LPJS_RUN_DIR "/lpjs_compd.pid", daemon_uid, daemon_gid);
#endif

    // setgid() must be done while still running as root, so do setuid() after
    if ( daemon_gid != 0 )
    {
	lpjs_log("Setting daemon_gid to %u.\n", daemon_gid);
	if ( setgid(daemon_gid) != 0 )
	{
	    lpjs_log("setgid() failed: %s\n", strerror(errno));
	    return EX_NOPERM;
	}
    }
    
    if ( daemon_uid != 0 )
    {
	lpjs_log("Setting daemon_uid to %u.\n", daemon_uid);
	if ( setuid(daemon_uid) != 0 )
	{
	    lpjs_log("setuid() failed: %s\n", strerror(errno));
	    return EX_NOPERM;
	}
    }
    
    // Read etc/lpjs/config, created by lpjs-admin
    lpjs_load_config(node_list, LPJS_CONFIG_ALL, Log_stream);
    
    /*
     *  bind(): address already in use during testing with frequent restarts.
     *  Best approach is to ensure that client completes a close
     *  before the server closes.
     *  https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
     *  Copy saved in ./bind-address-already-in-use.pdf
     *  FIXME: Does this handler actually help?  FDs are closed
     *  upon process termination anyway.
     */
    signal(SIGINT, lpjs_terminate_handler);
    signal(SIGTERM, lpjs_terminate_handler);

    return lpjs_process_events(node_list);
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

int     lpjs_process_events(node_list_t *node_list)

{
    int                 listen_fd;
    struct sockaddr_in  server_address = { 0 };
    // job_list_new() terminates process if malloc fails, no need to check
    job_list_t          *pending_jobs = job_list_new(),
			*running_jobs = job_list_new();

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
	
	// FIXME: Might this erase pending messages?
	// Use poll() instead of select()?
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

	/*
	 *  The nfds (# of file descriptors) argument to select is a
	 *  bit confusing.  It's actually the highest descriptor + 1,
	 *  not the number of open descriptors.  E.g., to check only
	 *  descriptors 3 and 8, nfds must be 9, not 2.  This is
	 *  different from the analogous poll() function, which takes
	 *  an array of open descriptors.
	 */
	nfds = highest_fd + 1;
	
	lpjs_log("Running select...\n");
	if ( select(nfds, &read_fds, NULL, NULL, LPJS_NO_SELECT_TIMEOUT) > 0 )
	{
	    //lpjs_log("Checking comp fds...\n");
	    lpjs_check_comp_fds(&read_fds, node_list, running_jobs);
	    
	    //lpjs_log("Checking listen fd...\n");
	    // Check FD_ISSET before calling function to avoid overhead
	    if ( FD_ISSET(listen_fd, &read_fds) )
		lpjs_check_listen_fd(listen_fd, &read_fds, &server_address,
				     node_list, pending_jobs, running_jobs);
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
			    job_list_t *running_jobs)

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
			     node_list_t *node_list,
			     job_list_t *pending_jobs, job_list_t *running_jobs)

{
    int             msg_fd;
    ssize_t         bytes;
    char            *munge_payload,
		    *p,
		    *hostname;
    socklen_t       address_len = sizeof (struct sockaddr_in);
    uid_t           munge_uid;
    gid_t           munge_gid;
    unsigned long   job_id;
    unsigned        procs_per_job;
    size_t          mem_per_proc;
    node_t          *node;
    int             items;
    job_t           *job;
    
    lpjs_log("In %s():\n", __FUNCTION__);
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
	    lpjs_log("Accepted connection. fd = %d\n", msg_fd);

	    /* Read a message through the socket */
	    if ( (bytes = lpjs_recv_munge(msg_fd,
			 &munge_payload, 0, 0, &munge_uid, &munge_gid)) < 1 )
	    {
		lpjs_log("%s(): lpjs_recv_munge() failed (%zd bytes): %s\n",
			__FUNCTION__, bytes, strerror(errno));
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
		case    LPJS_DISPATCHD_REQUEST_COMPD_CHECKIN:
		    lpjs_log("LPJS_DISPATCHD_REQUEST_COMPD_CHECKIN\n");
		    lpjs_process_compute_node_checkin(msg_fd, munge_payload,
						      node_list, munge_uid, munge_gid);
		    lpjs_dispatch_jobs(node_list, pending_jobs, running_jobs);
		    break;
		
		case    LPJS_DISPATCHD_REQUEST_NODE_STATUS:
		    lpjs_log("LPJS_DISPATCHD_REQUEST_NODE_STATUS\n");
		    node_list_send_status(msg_fd, node_list);
		    // lpjs_server_safe_close(msg_fd);
		    // node_list_send_status() sends EOT,
		    // so don't use safe_close here.
		    close(msg_fd);
		    break;
		
		case    LPJS_DISPATCHD_REQUEST_JOB_STATUS:
		    lpjs_log("LPJS_DISPATCHD_REQUEST_JOB_STATUS\n");
		    lpjs_send_munge(msg_fd, "Pending\n\n");
		    job_list_send_params(msg_fd, pending_jobs);
		    lpjs_send_munge(msg_fd, "\nRunning\n\n");
		    job_list_send_params(msg_fd, running_jobs);
		    lpjs_server_safe_close(msg_fd);
		    break;
		
		case    LPJS_DISPATCHD_REQUEST_SUBMIT:
		    lpjs_log("LPJS_DISPATCHD_REQUEST_SUBMIT\n");
		    lpjs_submit(msg_fd, munge_payload, node_list,
				pending_jobs, running_jobs,
				munge_uid, munge_gid);
		    lpjs_dispatch_jobs(node_list, pending_jobs, running_jobs);
		    lpjs_log("Back from lpjs_dispatch_jobs().\n");
		    break;

		case    LPJS_DISPATCHD_REQUEST_JOB_COMPLETE:
		    lpjs_log("Job completion report:\n%s\n",
			    munge_payload + 1);
		    
		    p = munge_payload + 1;
		    hostname = strsep(&p, " ");
		    lpjs_log("hostname = %s\n", hostname);
		    node = node_list_find_hostname(node_list, hostname);
		    if ( node == NULL )
		    {
			lpjs_log("%s(): Invalid hostname in job completion report.\n",
				__FUNCTION__);
			break;
		    }
		    if ( (items = sscanf(p, "%lu %u %zu",
			    &job_id, &procs_per_job, &mem_per_proc)) != 3 )
		    {
			lpjs_log("%s(): Got %d items reading job_id, procs and mem.\n",
				items);
			break;
		    }
		    
		    node_set_procs_used(node,
			node_get_procs_used(node) - procs_per_job);
		    node_set_phys_MiB_used(node,
			node_get_phys_MiB_used(node)
			    - mem_per_proc * procs_per_job);
		
		    /*
		     *  FIXME:
		     *      Write a completed job record to accounting log
		     *      Note the job completion in the main log
		     */
		    
		    // FIXME: Report error if NULL
		    if ( (job = lpjs_remove_job(running_jobs, job_id)) != NULL )
			job_free(&job);
		    
		    lpjs_log("Dispatching more jobs...\n");
		    lpjs_dispatch_jobs(node_list, pending_jobs, running_jobs);
		    break;
		    
		default:
		    lpjs_log("Invalid request on listen_fd: %d\n",
			    munge_payload[0]);
		    
	    }   // switch
	    free(munge_payload);
	}
    }
    else
	lpjs_log("%s(): listen_fd is not ready.\n", __FUNCTION__);
    
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
					  uid_t munge_uid, gid_t munge_gid)

{
    node_t      *new_node = node_new();
    extern FILE *Log_stream;
    
    // FIXME: Check for duplicate checkins.  We should not get
    // a checkin request while one is already open
    // lpjs_log("Munge credential message length = %zd\n", bytes);
    // lpjs_log("munge msg: %s\n", incoming_msg);
    
    lpjs_log("Checkin from munge_uid %d, munge_gid %d\n", munge_uid, munge_gid);
    
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
		    node_list_t *node_list,
		    job_list_t *pending_jobs, job_list_t *running_jobs,
		    uid_t munge_uid, gid_t munge_gid)

{
    char        script_path[PATH_MAX + 1],
		*end,
		*script_text;
    job_t       *submission = job_new(), // exits if malloc() fails, no need to check
		*job;
    int         c, job_array_index;
    
    // FIXME:
    // if ( job_list_get_count(running_jobs) + submit_count > LPJS_MAX_JOBS )
    
    // FIXME: Don't accept job submissions from root until
    // security issues have been considered
    
    lpjs_log("Request for job submission.\n");
    
    // Payload in message from lpjs submit is a job description
    // in JOB_SPEC_FORMAT
    job_read_from_string(submission, incoming_msg + 1, &end);
    // Should only be a newline between job specs and script
    script_text = end + 1;
    
    snprintf(script_path, PATH_MAX + 1, "%s/%s",
	     job_get_submit_directory(submission), job_get_script_name(submission));
    for (c = 0; c < job_get_job_count(submission); ++c)
    {
	lpjs_log("Submit script %s:%s from %d, %d\n",
		job_get_submit_host(submission), script_path, munge_uid, munge_gid);
	job_array_index = c + 1;    // Job arrays are 1-based
	
	// Create a separate job_t object for each member of the job array
	// FIXME: Check for success
	job = job_dup(submission);
	lpjs_queue_job(msg_fd, pending_jobs, job, job_array_index, script_text);
    }
    
    lpjs_server_safe_close(msg_fd);
    job_free(&submission);
    
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

int     lpjs_queue_job(int msg_fd, job_list_t *pending_jobs, job_t *job,
		       unsigned long job_array_index, const char *script_text)

{
    char    pending_dir[PATH_MAX + 1],
	    pending_path[PATH_MAX + 1],
	    specs_path[PATH_MAX + 1],
	    job_id_path[PATH_MAX + 1],
	    job_id_buff[LPJS_MAX_INT_DIGITS + 1],
	    outgoing_msg[LPJS_MSG_LEN_MAX + 1];
    int     fd;
    ssize_t bytes;
    unsigned long   next_job_id;
    FILE    *fp;
    
    lpjs_log("Spooling %s...\n", job_get_script_name(job));
    
    snprintf(job_id_path, PATH_MAX + 1, "%s/next-job", LPJS_SPOOL_DIR);
    if ( (fd = open(job_id_path, O_RDONLY)) == -1 )
	next_job_id = 1;
    else
    {
	bytes = read(fd, job_id_buff, LPJS_MAX_INT_DIGITS + 1);
	if ( bytes == -1 )
	{
	    lpjs_log("%s(): Error reading next-job: %s\n",
		    __FUNCTION__, strerror(errno));
	    exit(EX_DATAERR);
	}
	sscanf(job_id_buff, "%lu", &next_job_id);
	close(fd);
    }
    
    job_set_job_id(job, next_job_id);
    job_set_array_index(job, job_array_index);
    
    snprintf(pending_dir, PATH_MAX + 1, "%s/%lu", LPJS_PENDING_DIR,
	    next_job_id);
    if ( xt_rmkdir(pending_dir, 0755) != 0 )
    {
	fprintf(stderr, "Cannot create %s: %s\n", pending_dir, strerror(errno));
	return -1;  // FIXME: Define error codes
    }

    snprintf(pending_path, PATH_MAX + 1, "%s/%s", pending_dir,
	    xt_basename(job_get_script_name(job)));
    
    // FIXME: Use a symlink instead?  Copy is safer in case user
    // modifies the script while a job is running.
    // lpjs_log("CWD = %s  script = '%s'\n", getcwd(NULL, 0), script_path);
    // lpjs_log("stat(): %d\n", stat(script_path, &st));
    if ( (fd = open(pending_path, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1 )
    // if ( (status = xt_fast_cp(script_path, pending_path)) != 0 )
    {
	lpjs_log("lpjs_queue_job(): Failed to copy %s to %s: %s\n",
		job_get_script_name(job), pending_path, strerror(errno));
	return -1;
    }
    
    // FIXME: Check success
    write(fd, script_text, strlen(script_text));
    close(fd);
    
    /*
     *  Write basic job specs to a file for the dispatcher
     */
    
    snprintf(specs_path, PATH_MAX + 1, "%s/job.specs", pending_dir);
    lpjs_log("Storing specs to %s.\n", specs_path);
    // Bump job num after successful spool
    // FIXME: Switch to low-level I/O
    if ( (fp = fopen(specs_path, "w")) == NULL )
    {
	lpjs_log("Cannot create %s: %s\n", specs_path, strerror(errno));
	return -1;
    }
    
    job_print(job, fp);
    fclose(fp);
    
    // Back to submit command for terminal output
    snprintf(outgoing_msg, LPJS_MSG_LEN_MAX, "Spooled job %lu to %s.\n",
	    next_job_id, pending_dir);
    lpjs_send_munge(msg_fd, outgoing_msg);
    
    // FIXME: Send this to the job log, not the daemon log
    lpjs_log(outgoing_msg);
    
    // Bump job num after successful spool
    if ( (fd = open(job_id_path, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1 )
    {
	lpjs_log("Cannot update %s: %s\n", job_id_path, strerror(errno));
	return -1;
    }
    else
    {
	xt_dprintf(fd, "%lu\n", ++next_job_id);
	close(fd);
    }
    
    job_list_add_job(pending_jobs, job);
    
    return 0;   // FIXME: Define error codes
}
