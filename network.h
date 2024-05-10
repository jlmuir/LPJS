#ifndef _LPJS_NETWORK_H_
#define _LPJS_NETWORK_H_

/*
 *  First byte of each new conversation is a request code.  All codes
 *  to dispatchd must be unique as must codes to compd.
 *  Don't start codes at 0.  It will be interpreted as a null-terminator.
 */

enum
{
    LPJS_DISPATCHD_REQUEST_COMPD_CHECKIN = 1,
    LPJS_DISPATCHD_REQUEST_NODE_STATUS,
    LPJS_DISPATCHD_REQUEST_JOB_STATUS,
    LPJS_DISPATCHD_REQUEST_SUBMIT,
    LPJS_DISPATCHD_REQUEST_CHAPERONE_CHECKIN,
    LPJS_DISPATCHD_REQUEST_JOB_COMPLETE,
    LPJS_DISPATCHD_REQUEST_CANCEL,
    LPJS_DISPATCHD_REQUEST_PAUSE,
    LPJS_DISPATCHD_REQUEST_RESUME
};

enum
{
    LPJS_COMPD_REQUEST_NEW_JOB = 1,
    LPJS_COMPD_REQUEST_CANCEL
};

enum
{
    LPJS_DISPATCH_OK = 1,
    LPJS_DISPATCH_SCRIPT_FAILED,
    LPJS_DISPATCH_OSERR
};

// IPv6 max address size is 39
#define LPJS_TEXT_IP_ADDRESS_MAX    64
// FIXME: 4096 is just a guestimate
#define LPJS_MSG_LEN_MAX            LPJS_PAYLOAD_MAX + 4096
#define LPJS_CONNECTION_QUEUE_MAX   4096    // Should be more than enough

/*
 *  Use a different TCP port than SLURM (6817), so that the same head
 *  node can be used to manage both LPJS and SLURM compute nodes.
 *  This allows gradual migration of a cluster between schedulers,
 *  or partitioning of compute resources for other reasons.
 *  LPJS and SLURM compute nodes must be mutually exclusive.  A node
 *  cannot be used by both.
 */
#define LPJS_IP_TCP_PORT        (short)6818 // Need short for htons()
#define LPJS_RETRY_TIME         5

#define LPJS_MUNGE_CRED_VERIFIED     "MCD"

#ifndef _SYS_POLL_H_
#include <sys/poll.h>
#endif

#ifndef _NODE_LIST_H_
#include "node-list.h"
#endif

#include "network-protos.h"

#endif
