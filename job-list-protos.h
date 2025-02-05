/* job-list.c */
job_list_t *job_list_new(void);
void job_list_init(job_list_t *job_list);
int job_list_add_job(job_list_t *job_list, job_t *job);
size_t job_list_find_job_id(job_list_t *job_list, unsigned long job_id);
job_t *job_list_remove_job(job_list_t *job_list, unsigned long job_id);
void job_list_send_params(int msg_fd, job_list_t *job_list);
void job_list_sort(job_list_t *job_list);
