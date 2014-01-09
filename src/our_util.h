#ifndef OUR_UTIL__
#define OUR_UTIL__

extern int32_t setup_conn(int32_t type, int16_t sin_family, uint16_t local_port, const int8_t *local_ip, uint32_t dimBuftx, uint32_t dimBufrx, uint8_t tcp_no_delay);

extern void connect_TCP(int32_t fd, const struct sockaddr_in *server);

extern int32_t listen_accept(int32_t listen_fd, struct sockaddr_in *new_cli);

extern void setsockoption(int32_t fd, int32_t level, int32_t optname, uint32_t optval);

extern void setting_up_side(struct sockaddr_in *side, int16_t sin_family, uint16_t port, const int8_t *ip);

extern uint8_t manage_notify(mon_notify *new_notify, int32_t fd);

extern void build_act_ports(port_monitoring **plist, mon_notify *config);

extern void clear_buffer(buf_packet *buffer);

extern uint8_t oldest_pack(buf_packet *buffer);

extern uint8_t index_pack(int32_t *buffer, uint8_t type_packet, uint32_t id);

extern void safe_read(int32_t fd, int32_t size, int8_t *buffer);

extern void safe_write(int32_t fd, int32_t size, int8_t *buffer);

extern void clear_notify_s(mon_notify *config);

extern uint8_t index_free_pack(buf_packet *buffer);

extern void drop_expired(buf_packet *buffer);

extern void clear_pack(buf_packet *buffer, uint8_t index);

extern uint8_t first_pack_to_send(buf_packet *buffer);

extern void mighty_f_ports(port_monitoring **list);

extern port_monitoring* choose_port(port_monitoring *list, uint32_t number);

extern struct errno_port send_the_pack(int32_t *data, uint8_t type_packet, int32_t fd, port_monitoring *list);

extern int32_t general_sendto(int32_t *data, uint16_t size_data, int32_t fd, struct sockaddr_in *monitor);

extern void general_recvfrom(int32_t *data, uint16_t size_data, int32_t fd, struct sockaddr_in *monitor);

extern void sort_buffer(buf_packet *buffer);

extern void mighty_f_revenge(buf_packet *buffer, uint8_t index, struct timeval *act_zero, uint32_t first_pack_id);

extern void i_2_s(int64_t i, struct timeval *s);

extern int64_t s_2_i(struct timeval *s);

extern uint8_t check_inline_pack(buf_packet *buffer, udp_packet *buffer_udp, uint32_t *lup, uint32_t id);

extern uint8_t manage_timeout(buf_packet *buffer, struct timeval *timeout);

extern void udp_clear_buffer(udp_packet *buffer);

extern void udp_shift_pack(udp_packet *buffer, uint8_t index);

extern uint8_t index_udpfree_pack(udp_packet *buffer);

extern uint32_t min_to_before_rsd(buf_packet *buffer, uint8_t ppack_lost, uint32_t lag, struct timeval *timeout);
#endif
