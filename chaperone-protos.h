/* chaperone.c */
int lpjs_chaperone_checkin(int msg_fd, const char *hostname, const char *job_id, pid_t job_pid);
int lpjs_chaperone_checkin_loop(node_list_t *node_list, const char *hostname, const char *job_id, pid_t job_pid);
int lpjs_chaperone_completion(int msg_fd, const char *hostname, const char *job_id, int status);
int lpjs_chaperone_completion_loop(node_list_t *node_list, const char *hostname, const char *job_id, int status);
void chaperone_cancel_handler(int s2);
void whack_family(pid_t pid);
