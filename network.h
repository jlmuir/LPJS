#ifndef _LPJS_NETWORK_H_
#define _LPJS_NETWORK_H_

// IPv6 max address size is 39
#define LPJS_IP_MAX             64
#define LPJS_IP_MSG_MAX         4096
#define LPJS_IP_MSG_QUEUE_MAX   10

/*
 *  Use the same default TCP port as SLURM (6817), since only one
 *  scheduler can be running on a given cluster.
 */
#define LPJS_IP_TCP_PORT        (short)6817 // Need short for htons()

#ifndef _SYS_POLL_H_
#include <sys/poll.h>
#endif

#ifdef __FreeBSD__
#define LPJS_POLLHUP    POLLRDHUP
#else
#define LPJS_POLLHUP    POLLHUP
#endif

#ifndef _NODE_LIST_H_
#include "node-list.h"
#endif

#include "network-protos.h"

#endif
