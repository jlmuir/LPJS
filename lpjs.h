#ifndef _LPJS_H_
#define _LPJS_H_

#ifndef _NODE_LIST_H_
#include "node-list.h"
#endif

#ifndef _JOB_LIST_H_
#include "job-list.h"
#endif

enum
{
    LPJS_SUCCESS = 0,
    LPJS_READ_FAILED,
    LPJS_WRITE_FAILED
};

#define LPJS_FIELD_MAX          1024
#define LPJS_CMD_MAX            4096
// Not too large: Auto vars may exceed stack size
// FIXME: malloc arrays if this needs to be bigger
#define LPJS_PAYLOAD_MAX        65536   // FIXME: Does munge have a max?
#define LPJS_NO_SELECT_TIMEOUT  NULL

#define LPJS_LOG_DIR            PREFIX "/var/log/lpjs"
#define LPJS_COMPD_LOG          LPJS_LOG_DIR "/compd"
#define LPJS_DISPATCHD_LOG      LPJS_LOG_DIR "/dispatchd"
#define LPJS_JOB_HISTORY        LPJS_LOG_DIR "/job-history"

#define LPJS_SPOOL_DIR          PREFIX "/var/spool/lpjs"
#define LPJS_PENDING_DIR        LPJS_SPOOL_DIR "/pending"
#define LPJS_RUNNING_DIR        LPJS_SPOOL_DIR "/running"
#define LPJS_SPECS_FILE_NAME    "job.specs"

/*
 *  Job scripts should be quite small, usually no more than a few dozen lines.
 *  Each script should run one computational command.  Multiple commands
 *  in sequence in the same job script usually doesn't make sense, since they
 *  will have different CPU and memory requirements.
 *  FIXME: submit should print a warning if the script seems too complex.
 */

#define LPJS_SCRIPT_SIZE_MAX    LPJS_PAYLOAD_MAX

#define LPJS_JOB_MSG_MAX        JOB_STR_MAX_LEN + LPJS_SCRIPT_SIZE_MAX

#define LPJS_RUN_DIR            PREFIX "/var/run/lpjs"

#define LPJS_MB                 1000000
#define LPJS_MiB                1048576
#define LPJS_GB                 1000000000
#define LPJS_GiB                1073741824

#define LPJS_EOT                '\004'
#define LPJS_MAX_INT_DIGITS     64

#endif  // _LPJS_H_
