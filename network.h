#ifndef _LPJS_NETWORK_H_
#define _LPJS_NETWORK_H_

#define LPJS_IP_MAX     64  // IPv6 max is 39
#define LPJS_TCP_PORT   3000

void resolve_hostname(const char *hostname, char *ip, size_t ip_buff_len);
int connect_to_dispatch(node_list_t *node_list);

#endif
