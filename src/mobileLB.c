/************************************************
 * Mobile Load Balancer -- LabReti 2007/2008
 *
 * Alberti Michele - Pomili Luigi
 ***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>

/* inclusione dei nostri moduli di utilita' */
#include "datastruct.h"
#include "our_util.h"
#include "list_util.h"

#define LOCAL_UDP_PORT	7001
#define LOCAL_LISTEN_PORT	6001
#define MONITOR_PORT	8000
#define ID_START	9998

/* variabili locali al modulo */
HIDDEN int8_t data_app_mobile[DATA_BUFF_SIZE];		/* data proveniente dall'applicazione mobile */
HIDDEN buf_packet up_packets[BUFF_PACK_SIZE];		/* appena bufferizzati, provenienti dal monitor */
HIDDEN buf_packet down_packets[BUFF_PACK_SIZE];	/* dall'applicazione */
HIDDEN udp_packet to_app_packets[BUFF_TO_APP];		/* pacchetti pronti verso l'applicazione */
HIDDEN port_monitoring	*port_list;					/* lista delle porte attive */
HIDDEN mon_notify last_m;	/* ultima notifica da parte del monitor */

HIDDEN uint8_t first_notify;
HIDDEN uint8_t prog_id;		/* ID progressivo di aggiunta per le notifiche eccezionali */
HIDDEN uint8_t id_seq;		/* ID progressivo di sequenze di pacchetti notifiche */
HIDDEN uint32_t last_uploaded, copy_lup;	/* ultimo ID di pacchetto spedito alla AppMobile */
HIDDEN uint32_t max_nid_sent;	/* massimo ID di pacchetto notifica spedito */
HIDDEN uint16_t if_notify; /* determina la necessita' di spedizione della notifica al fixed */
HIDDEN struct timeval actual_zero;	/* istante zero dell'altro load balancer */
HIDDEN struct timeval us_zero;	/* fix ritardo temporale applicazione mobile */

/* select fd set */
HIDDEN int32_t maxfds;	/* fd massimo da controllare */
HIDDEN fd_set readfds, s_readfds;
HIDDEN fd_set writefds, s_writefds;

/* file descriptor delle connessioni */
HIDDEN int32_t to_app;
HIDDEN int32_t to_monitor;
HIDDEN int32_t udp_gate;
HIDDEN int32_t new_app_fd;

struct sockaddr_in mon_side;	/* setting per il monitor side */
struct sockaddr_in app_side;	/* setting per l'application side */

int main(void) {

	int32_t	nready, index;
	uint8_t index_to;
	struct timeval timeout;

	/* settaggio della connessione TCP con il monitor */
	to_monitor = setup_conn(SOCK_STREAM, AF_INET, 0, (int8_t*) "INADDR_ANY", 0, 0, 1);
	setting_up_side(&mon_side, AF_INET, MONITOR_PORT, HOST_IP);
	connect_TCP(to_monitor, &mon_side);	/* connessione al monitor */
	/* settaggio della connessione TCP con l'applicazine mobile */
	to_app = setup_conn(SOCK_STREAM, AF_INET, LOCAL_LISTEN_PORT, (int8_t*) "INADDR_ANY", 0, 0, 1);
	/* settaggio della connessione UDP con il monitor */
	udp_gate = setup_conn(SOCK_DGRAM, AF_INET, LOCAL_UDP_PORT, (int8_t*) "INADDR_ANY", 0, 0, 0);
	
	/* inizializzazione strutture */
	make_list(&port_list);
	clear_buffer(down_packets);
	clear_buffer(up_packets);
	udp_clear_buffer(to_app_packets);
	srand(getpid());
	/* inizializzazioni di default */
	first_notify = TRUE;
	last_uploaded = ID_START;
	max_nid_sent = copy_lup = last_uploaded;
	actual_zero.tv_usec = actual_zero.tv_sec = 0;
	us_zero.tv_usec = us_zero.tv_sec = 0;
	index_to = BUFF_PACK_SIZE;	/* no timeout inizialmente */
	if_notify = FALSE;
	prog_id = id_seq = 0;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&s_readfds);
	FD_ZERO(&s_writefds);
	maxfds = to_monitor;

	FD_SET(to_monitor, &readfds);

	for (;;) {

		s_readfds = readfds;
		s_writefds = writefds;
		nready = select(maxfds + 1, &s_readfds, &s_writefds, NULL, (index_to == BUFF_PACK_SIZE ? NULL : (&timeout)));
		if ((nready == ERROR) && (errno != EINTR)) {
			fprintf(stderr, "fatal error performing select. errno %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (FD_ISSET(to_monitor, &s_readfds)) {
			if (manage_notify(&last_m, to_monitor) == TRUE) {
				switch (last_m.type) {
					case 'C': {
						if (last_m.ports[0] != 0) {
							printf("last m ports: 0 - %d, 1 - %d, 2 - %d\n", last_m.ports[0], last_m.ports[1], last_m.ports[2]);
							build_act_ports(&port_list, &last_m);
							list_print(port_list);
							clear_notify_s(&last_m);
							if (first_notify) {
								new_app_fd = listen_accept(to_app, &app_side);
								FD_SET(new_app_fd, &readfds);
								first_notify = FALSE;
								if (new_app_fd > maxfds)	/* nuovo fd nell'insieme read */
									maxfds = new_app_fd;
								FD_SET(udp_gate, &readfds);	/* abilitazione alla lettura dei pacchetti provenienti dal monitor */
							}
							else { /* eccezione alla spedizione delle notifiche al fixed */
								copy_lup = last_uploaded + NOTIFY_ID_OFFSET + ++prog_id;
								if_notify = TRUE;
								id_seq++;	/* nuova sequenza di notifiche */
								reset_all_notify(port_list);
								FD_SET(udp_gate, &writefds);
							}
						}
						break;
					}
					case 'A': {
						uint8_t	index;
						port_monitoring *tmp;
	
						/* se ack del monitor e' per un pacchetto STANDARD spedito */
						index = index_pack((int32_t*) down_packets, STANDARD, last_m.message);
						if (index != BUFF_PACK_SIZE) {
							if ((tmp = find_port(port_list, down_packets[index].id_port)) != NULL)
								tmp->ack_packets++;
						#ifdef O_DEBUG_P
							printf("stadard ack con id: %d\n", down_packets[index].udp_data.id_packet);
						#endif
							clear_pack(down_packets, index);
						}
						/* oppure per un NOTIFY_F spedita */
						else if ((tmp = find_notify(port_list, last_m.message)) != NULL) {
						#ifdef O_DEBUG_N
							printf("ack di notify con id: %d\n", tmp->not_to_fixed.id_notify);
						#endif
							tmp->ack_packets++;
							tmp->state_notify = NOTIFY_SUCC;
						}
						break;
					}
					case 'N': {
						uint8_t	index;
						port_monitoring *tmp;

						/* se nack del monitor e' per un pacchetto STANDARD spedito */
						index = index_pack((int32_t*) down_packets, STANDARD, last_m.message);
						if (index != BUFF_PACK_SIZE) {
						#ifdef O_DEBUG_P
							printf("ricevuto nack con id: %d\n", down_packets[index].udp_data.id_packet);
						#endif
							if ((tmp = find_port(port_list, down_packets[index].id_port)) != NULL)
								tmp->nack_packets++;
							drop_expired(down_packets);	/* se il pacchetto non viene scartato da questa chiamata */
							if (index_pack((int32_t*) down_packets, STANDARD, last_m.message) != BUFF_PACK_SIZE) {
								down_packets[index].flag_to_send = NACK_FLAG;
								FD_SET(udp_gate, &writefds);
							}
						}
						/* oppure per una NOTIFY_F spedita */
						else if ((tmp = find_notify(port_list, last_m.message)) != NULL) {
						#ifdef O_DEBUG_N
							printf("[NOTIFY] ricevuto nack speciale con id: %d\n", tmp->not_to_fixed.id_notify);
						#endif
							tmp->nack_packets++;
							tmp->state_notify = NOTIFY_FAIL; /* rispedizione di questa notifica */
							FD_SET(udp_gate, &writefds);
						}
						/* evitiamo di mandare al monitor se la drop precedente ha tolto i pacchetti
						 * previsti in precedenza */
						if (first_pack_to_send(down_packets) == BUFF_PACK_SIZE)
							if ((first_notify_snd(port_list) == NULL) && (first_notify_rsd(port_list) == NULL))
								FD_CLR(udp_gate, &writefds);
						break;
					}
				}
			}
		}
		if (FD_ISSET(new_app_fd, &s_readfds)) {
			uint8_t oldestpack;

			/* buffering dei pacchetti provenienti dall' applicazione */
			drop_expired(down_packets);
			safe_read(new_app_fd, DATA_BUFF_SIZE, data_app_mobile);
			/* evitiamo il possibile traboccamento di down_packets */
			if ((index = index_free_pack(down_packets)) == BUFF_PACK_SIZE) {
				oldestpack = oldest_pack(down_packets);
				clear_pack(down_packets, oldestpack);
				index = oldestpack;
			}
			memcpy(&(down_packets[index].udp_data.id_packet), data_app_mobile, sizeof(uint32_t));
			memcpy(&(down_packets[index].udp_data.data), &(data_app_mobile[4]), DATA_BUFF_SIZE - sizeof(uint32_t));
			down_packets[index].udp_data.type_packet = STANDARD;
			gettimeofday(&(down_packets[index].arrived), NULL);
			if (s_2_i(&us_zero) == 0)
				us_zero = down_packets[index].arrived;
			down_packets[index].udp_data.the_mighty_fix = s_2_i(&(down_packets[index].arrived)) - (s_2_i(&us_zero) + (INTER_PACKET * down_packets[index].udp_data.id_packet));
			down_packets[index].flag_to_send = NEW_ENTRY_FLAG;
			FD_SET(udp_gate, &writefds);

			if (first_pack_to_send(down_packets) == BUFF_PACK_SIZE)
				FD_CLR(udp_gate, &writefds);
		}
		if (FD_ISSET(udp_gate, &s_writefds)) {
			uint8_t	index;
			port_monitoring *pnot;
			
			/* ri-spedizione della notifica */
			if ((pnot = first_notify_rsd(port_list)) != NULL) {
				pnot->state_notify = NOTIFY_SENT;
				send_the_pack((int32_t*) &(pnot->not_to_fixed), NOTIFY_F, udp_gate, port_list);
			#ifdef O_DEBUG_N
				printf("rispedita notify con id %d\n", pnot->not_to_fixed.id_notify);
				printf("id porta da notificare %d\n", pnot->not_to_fixed.id_port);
			#endif
			}
			/* spedizione della notifica al monitor */
			if (if_notify && ((pnot = first_notify_snd(port_list)) != NULL)) {
				/* id di pacchetti notifica sempre crescenti e differenti */
				if ((++copy_lup + STEP_NOTIFY) <= max_nid_sent)
					copy_lup = max_nid_sent + 1;
				pnot->not_to_fixed.id_notify = copy_lup + STEP_NOTIFY;
				max_nid_sent = pnot->not_to_fixed.id_notify;	/* aggiorno ID massimo di notifica */
				pnot->not_to_fixed.type_packet = NOTIFY_F + list_lenght(port_list);
				pnot->not_to_fixed.id_sequence = id_seq;
				pnot->state_notify = NOTIFY_SENT;
				send_the_pack((int32_t*) &(pnot->not_to_fixed), NOTIFY_F, udp_gate, port_list);
				pnot->not_to_fixed.n_received = 0;	/* resettaggio default */
			#ifdef O_DEBUG_N
				printf("spedita notify con id %d\n", pnot->not_to_fixed.id_notify);
				printf("id porta da notificare %d\n", pnot->not_to_fixed.id_port);
			#endif
				if ((pnot = first_notify_snd(port_list)) == NULL) {
					/* non ci sono piu' notifiche da fare */
					if_notify = FALSE;
					copy_lup = last_uploaded;
				}
			}
			else if ((index = first_pack_to_send(down_packets)) != BUFF_PACK_SIZE) {
				/* spedizione del pacchetto verso il monitor */
				gettimeofday(&(down_packets[index].time_last_send), NULL);
				down_packets[index].udp_data.time_buffered = BUFFERED_TIME(down_packets[index]);
				send_the_pack((int32_t*) &(down_packets[index]), STANDARD, udp_gate, port_list);
			#ifdef O_DEBUG
				printf("spedito il pacchetto con id %d\n", down_packets[index].udp_data.id_packet);
			#endif
			} 
			else {
				FD_CLR(udp_gate, &writefds);
			}
			/* se non ci sono altri pacchetti in attesa */
			if (first_pack_to_send(down_packets) == BUFF_PACK_SIZE)
				if ((first_notify_snd(port_list) == NULL) && (first_notify_rsd(port_list) == NULL))
					FD_CLR(udp_gate, &writefds);
		}
		if (FD_ISSET(udp_gate, &s_readfds)) {
			udp_packet tmp;
			struct sockaddr_in real_side;
			uint8_t i;
			port_monitoring *ptmp;
			
			/* ricezione del pacchetto dal monitor */
			general_recvfrom((int32_t *) &(tmp), sizeof(udp_packet), udp_gate, &real_side);
			/* aggiornamento valori per la futura notifica verso il fixed */
			ptmp = find_port(port_list, ntohs(real_side.sin_port));
			ptmp->not_to_fixed.n_received++;
		#ifdef O_DEBUG
			printf("id pacchetto ricevuto: %d\n", tmp.id_packet);
		#endif
			/* evita duplicati del pacchetto appena arrivato e gia' gestiti */
			for (i = 0; i < BUFF_PACK_SIZE; i++) {
				if (up_packets[i].udp_data.id_packet == tmp.id_packet)
					break;
			}
			if ((i == BUFF_PACK_SIZE) && (tmp.id_packet > last_uploaded)) {	/* non duplicato */
				/* creazione del rispettivo pacchetto di ack */
				up_packets[BUFF_PACK_SIZE - 1].udp_data = tmp;	/* speranza che sia vuoto per la legge di Gigio */
				gettimeofday(&(up_packets[BUFF_PACK_SIZE - 1].delivered), NULL);
				if ((actual_zero.tv_usec == 0) && (actual_zero.tv_sec == 0))	/* nuovo zero dell'opposto balancer */
					i_2_s(s_2_i(&(up_packets[BUFF_PACK_SIZE - 1].delivered)) - (INTER_PACKET * (tmp.id_packet - ID_START + 1)), &actual_zero);
				mighty_f_revenge(up_packets, BUFF_PACK_SIZE - 1, &actual_zero, ID_START + 1);
				if (tmp.id_packet == (last_uploaded + 1)) {	/* e' il successivo pacchetto da mandare all'applicazione */
					clear_pack(up_packets, BUFF_PACK_SIZE - 1);
					to_app_packets[index_udpfree_pack(to_app_packets)] = tmp;
					last_uploaded = tmp.id_packet;	/* aggiorno ultimo upload */
					check_inline_pack(up_packets, to_app_packets, &last_uploaded, tmp.id_packet);
					FD_SET(new_app_fd, &writefds);
				}
				else {
					if (tmp.id_packet > last_uploaded) {
						sort_buffer(up_packets);
					}
					else	/* in ritado abissale, drop */
						clear_pack(up_packets, BUFF_PACK_SIZE - 1);
				}
			}
		}
		if (FD_ISSET(new_app_fd, &s_writefds)) {
			memcpy(data_app_mobile, &(to_app_packets[0].id_packet), sizeof(uint32_t));
			memcpy(&(data_app_mobile[4]), &(to_app_packets[0].data), DATA_BUFF_SIZE - sizeof(uint32_t));
			safe_write(new_app_fd, DATA_BUFF_SIZE, data_app_mobile);
			udp_shift_pack(to_app_packets, 0);	/* clear e shift */
			if (to_app_packets[0].type_packet == FALSE)	/* non ce ne sono piu' */
				FD_CLR(new_app_fd, &writefds);
		}
		if (nready == 0) {	/* timeout espirato, nessun fd pronto */
			if (check_inline_pack(up_packets, to_app_packets, &last_uploaded, up_packets[0].udp_data.id_packet - 1))
				FD_SET(new_app_fd, &writefds);
		}
		index_to = BUFF_PACK_SIZE;
		index_to = manage_timeout(up_packets, &timeout);
		/* notifica al fixed ogni 50 pacchetti */
		if ((PACK_FROM_LNOT(last_uploaded, copy_lup) >= 50) && !if_notify) {
			copy_lup = last_uploaded;
			if_notify = TRUE;
			id_seq++;	/* nuova sequenza di notifiche */
			reset_all_notify(port_list);
			FD_SET(udp_gate, &writefds);
		}
	}

	return 0;
}
