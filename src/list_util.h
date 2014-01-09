#ifndef OUR_UTIL_LIST__
#define OUR_UTIL_LIST__

extern void make_list(port_monitoring **list);

extern port_monitoring* insert_port(port_monitoring** list, uint16_t	id_port);

extern void remove_port(port_monitoring** to_remove);

extern int8_t check_port(port_monitoring *list, uint16_t id_port);

extern int8_t list_lenght(port_monitoring *list);

extern void list_print(port_monitoring *list);

extern port_monitoring* find_port(port_monitoring *list, uint16_t id_port);

extern port_monitoring* min_port(port_monitoring* list, uint16_t id_port);

extern uint8_t max_port(port_monitoring* list, uint16_t id_port);

extern port_monitoring* find_notify(port_monitoring *list, uint32_t id_notify);

extern port_monitoring* first_notify_snd(port_monitoring *list);

extern port_monitoring* first_notify_rsd(port_monitoring *list);

extern uint8_t reset_all_notify(port_monitoring *list);

extern uint8_t partial_pp_lost(port_monitoring* list);
#endif
