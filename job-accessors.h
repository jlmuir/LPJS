    
/*
 *  Generated by /usr/local/bin/auto-gen-get-set-opaque
 *
 *  Accessor macros.  Use these to access structure members from functions
 *  outside the job_t class.
 *
 *  These generated macros are not expected to be perfect.  Check and edit
 *  as needed before adding to your code.
 */

/* temp-job-accessors.c */
unsigned long job_get_job_id(job_t *job_ptr);
unsigned long job_get_array_index(job_t *job_ptr);
unsigned job_get_job_count(job_t *job_ptr);
unsigned job_get_cores_per_job(job_t *job_ptr);
unsigned job_get_min_cores_per_node(job_t *job_ptr);
size_t job_get_mem_per_core(job_t *job_ptr);
char *job_get_user_name(job_t *job_ptr);
char job_get_user_name_ae(job_t *job_ptr, size_t c);
char *job_get_primary_group_name(job_t *job_ptr);
char job_get_primary_group_name_ae(job_t *job_ptr, size_t c);
char *job_get_submit_host(job_t *job_ptr);
char job_get_submit_host_ae(job_t *job_ptr, size_t c);
char *job_get_submit_directory(job_t *job_ptr);
char job_get_submit_directory_ae(job_t *job_ptr, size_t c);
char *job_get_script_name(job_t *job_ptr);
char job_get_script_name_ae(job_t *job_ptr, size_t c);
char *job_get_push_command(job_t *job_ptr);
char job_get_push_command_ae(job_t *job_ptr, size_t c);
