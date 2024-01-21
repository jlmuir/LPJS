
/*
 *  Generated by /usr/local/bin/auto-gen-get-set-transparent
 *
 *  Mutator functions for setting with no sanity checking.  Use these to
 *  set structure members from functions outside the node_t
 *  class.  These macros perform no data validation.  Hence, they achieve
 *  maximum performance where data are guaranteed correct by other means.
 *  Use the mutator functions (same name as the macro, but lower case)
 *  for more robust code with a small performance penalty.
 *
 *  These generated macros are not expected to be perfect.  Check and edit
 *  as needed before adding to your code.
 */

/* temp-node-mutators.c */
int node_set_hostname(node_t *node_ptr, char *new_hostname);
int node_set_hostname_ae(node_t *node_ptr, size_t c, char new_hostname_element);
int node_set_hostname_cpy(node_t *node_ptr, char *new_hostname, size_t array_size);
int node_set_cores(node_t *node_ptr, unsigned new_cores);
int node_set_cores_used(node_t *node_ptr, unsigned new_cores_used);
int node_set_phys_mem(node_t *node_ptr, unsigned long new_phys_mem);
int node_set_phys_mem_used(node_t *node_ptr, unsigned long new_phys_mem_used);
int node_set_zfs(node_t *node_ptr, int new_zfs);
int node_set_os(node_t *node_ptr, char *new_os);
int node_set_os_ae(node_t *node_ptr, size_t c, char new_os_element);
int node_set_os_cpy(node_t *node_ptr, char *new_os, size_t array_size);
int node_set_arch(node_t *node_ptr, char *new_arch);
int node_set_arch_ae(node_t *node_ptr, size_t c, char new_arch_element);
int node_set_arch_cpy(node_t *node_ptr, char *new_arch, size_t array_size);
int node_set_state(node_t *node_ptr, char *new_state);
int node_set_state_ae(node_t *node_ptr, size_t c, char new_state_element);
int node_set_state_cpy(node_t *node_ptr, char *new_state, size_t array_size);
int node_set_msg_fd(node_t *node_ptr, int new_msg_fd);
int node_set_last_ping(node_t *node_ptr, time_t new_last_ping);
