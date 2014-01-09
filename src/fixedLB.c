/************************************************
 * Fixed Load Balancer -- LabReti 2007/2008
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

#define LOCAL_UDP_PORT	10001
#define LOCAL_LISTEN_PORT	11001
#define ID_START	-1
#define ID_FIRST_SENT	9999
#define GOOD_P	75

/* variabili locali al modulo */
HIDDEN int8_t data_app_fixed[DATA_BUFF_SIZE];		/* data proveniente dall'applicazione fixed */
HIDDEN buf_packet up_packets[BUFF_PACK_SIZE];		/* appena bufferizzati, provenienti dal monitor */
HIDDEN buf_packet down_packets[BUFF_PACK_SIZE];		/* dall'applicazione */
HIDDEN udp_packet to_app_packets[BUFF_TO_APP];		/* pacchetti pronti verso l'applicazione */
HIDDEN mon_notify m_id_ports;				/* id delle porte lato mobile */
HIDDEN port_monitoring	*port_list;					/* lista delle porte attive */

HIDDEN uint32_t last_uploaded;	/* ultimo ID di pacchetto spedito alla AppFixed */
HIDDEN uint32_t last_notify;	/* ultimo ID di pacchetto notifica arrivato */
HIDDEN uint8_t last_seq;		/* ultimo ID di sequenza di notifiche arrivato */
HIDDEN struct timeval actual_zero;	/* istante zero dell'altro load balancer */
HIDDEN struct timeval us_zero;	/* fix ritardo temporale applicazione fixed */
HIDDEN uint8_t good_percentual;			/* limitazione alla rispedizione del pacchetto */

/* select fd set */
HIDDEN int32_t maxfds;	/* fd massimo da controllare */
HIDDEN fd_set readfds, s_readfds;
HIDDEN fd_set writefds, s_writefds;

/* file descriptor delle connessioni */
HIDDEN int32_t to_app;
HIDDEN int32_t udp_gate;
HIDDEN int32_t new_app_fd;

struct sockaddr_in app_side;	/* setting per l'application side */

int main(void) {

	int32_t	nready;
	uint8_t index_to;
	struct timeval timeout;

	/* settaggio della connessione TCP con l'applicazine fissa */
	to_app = setup_conn(SOCK_STREAM, AF_INET, LOCAL_LISTEN_PORT, (int8_t*) "INADDR_ANY", 0, 0, 1);
	/* accettazione della connessione proveniente dall'applicazione fissa */
	new_app_fd = listen_accept(to_app, &app_side);
	/* settaggio della connessione UDP con il monitor */
	udp_gate = setup_conn(SOCK_DGRAM, AF_INET, LOCAL_UDP_PORT, (int8_t*) "INADDR_ANY", 0, 0, 0);
	
	/* inizializzazione strutture */
	make_list(&port_list);
	clear_buffer(down_packets);
	clear_buffer(up_packets);
	udp_clear_buffer(to_app_packets);
	srand(getpid());
	/* inizializzazioni di default */
	last_uploaded = ID_START;
	actual_zero.tv_usec = actual_zero.tv_sec = 0;
	us_zero.tv_usec = us_zero.tv_sec = 0;
	index_to = BUFF_PACK_SIZE;	/* no timeout inizialmente */
	m_id_ports.type = 'C';
	m_id_ports.message = 0;
	last_notify = last_seq = 0;
	good_percentual = GOOD_P;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&s_readfds);
	FD_ZERO(&s_writefds);

	FD_SET(new_app_fd, &readfds);	/* tanto prima aspetta un pacchetto da noi */
	FD_SET(udp_gate, &readfds);
	maxfds = (udp_gate > new_app_fd ? udp_gate : new_app_fd);

	for (;;) {

		s_readfds = readfds;
		s_writefds = writefds;
		nready = select(maxfds + 1, &s_readfds, &s_writefds, NULL, (index_to == BUFF_PACK_SIZE ? NULL : (&timeout)));
		if ((nready == ERROR) && (errno != EINTR)) {
			fprintf(stderr, "fatal error performing select. errno %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (FD_ISSET(new_app_fd, &s_readfds)) {
			uint8_t oldestpack, index;

			/* buffering dei pacchetti provenienti dall' applicazione */
			drop_expired(down_packets);
			safe_read(new_app_fd, DATA_BUFF_SIZE, data_app_fixed);
			/* evitiamo il possibile traboccamento di down_packets */
			if ((index = index_free_pack(down_packets)) == BUFF_PACK_SIZE) {
				oldestpack = oldest_pack(down_packets);
				clear_pack(down_packets, oldestpack);
				index = oldestpack;
			}
			memcpy(&(down_packets[index].udp_data.id_packet), data_app_fixed, sizeof(uint32_t));
			memcpy(&(down_packets[index].udp_data.data), &(data_app_fixed[4]), DATA_BUFF_SIZE - sizeof(uint32_t));
			down_packets[index].udp_data.type_packet = STANDARD;
			gettimeofday(&(down_packets[index].arrived), NULL);
			if (s_2_i(&us_zero) == 0)
				us_zero = down_packets[index].arrived;
			down_packets[index].udp_data.the_mighty_fix = s_2_i(&(down_packets[index].arrived)) - (s_2_i(&us_zero) + (INTER_PACKET * (down_packets[index].udp_data.id_packet - ID_FIRST_SENT)));
			down_packets[index].flag_to_send = NEW_ENTRY_FLAG;
			FD_SET(udp_gate, &writefds);
		}
		if (FD_ISSET(udp_gate, &s_writefds)) {
			uint8_t	index;
			struct errno_port ep;
			double rand_n;

			if ((index = first_pack_to_send(down_packets)) != BUFF_PACK_SIZE) {
				/* spedizione del pacchetto verso il monitor */
				ep.type_errno = EXIT_FAILURE;
				while ((ep.type_errno != EXIT_SUCCESS) && (ep.type_errno != EAGAIN) && (port_list != NULL)) {
					gettimeofday(&(down_packets[index].time_last_send), NULL);
					down_packets[index].udp_data.time_buffered = BUFFERED_TIME(down_packets[index]);
					ep = send_the_pack((int32_t*) &(down_packets[index]), STANDARD, udp_gate, port_list);
				/*	printf("utilizzato porta id: %u\n", ep.id_port); */
				}
			#ifdef O_DEBUG
				if (ep.type_errno == EXIT_SUCCESS) {
					printf("spedito il pacchetto con id %d\n", down_packets[index].udp_data.id_packet);
				}
			#endif
				RANDOM_N(rand_n);
				if ((rand_n > (good_percentual - 0)) && (down_packets[index].n_resending > 0)) {
					down_packets[index].flag_to_send = NACK_FLAG;	/* forza la riconsiderazione del pacchetto */
					down_packets[index].n_resending--;
				}
				if ((index = first_pack_to_send(down_packets)) == BUFF_PACK_SIZE)
					FD_CLR(udp_gate, &writefds);
			}
			else {
				FD_CLR(udp_gate, &writefds);
			}
		}
		if (FD_ISSET(udp_gate, &s_readfds)) {
			udp_packet tmp;
			struct sockaddr_in mon_side;
			
			/* ricezione dei dal monitor */
			general_recvfrom((int32_t *) &(tmp), sizeof(udp_packet), udp_gate, &mon_side);

			if (check_port(port_list, ntohs(mon_side.sin_port)) == FALSE) /* inserimento di una, forse, nuova porta monitor */
				insert_port(&port_list, ntohs(mon_side.sin_port));
			switch (tmp.type_packet) {
				case STANDARD: {
				#ifdef O_DEBUG
					printf("id pacchetto ricevuto: %d\n", tmp.id_packet);
				#endif
					up_packets[BUFF_PACK_SIZE - 1].udp_data = tmp;	/* speranza che sia vuoto per la legge di Gigio */
					gettimeofday(&(up_packets[BUFF_PACK_SIZE - 1].delivered), NULL);
					if ((actual_zero.tv_usec == 0) && (actual_zero.tv_sec == 0))	/* nuovo zero dell'opposto balancer */
						i_2_s(s_2_i(&(up_packets[BUFF_PACK_SIZE - 1].delivered)) - (INTER_PACKET * tmp.id_packet), &actual_zero);
					mighty_f_revenge(up_packets, BUFF_PACK_SIZE - 1, &actual_zero, 0);
					if (tmp.id_packet == (last_uploaded + 1)) {	/* e' il successivo */
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
					break;
				}
				default : {		/* pacchetto di notifica */
					port_monitoring *ptmp = NULL;
					stat_notify *trick = (stat_notify*) &tmp;
				#ifdef O_DEBUG_N
					printf("pacchetto id: %d\n", tmp.id_packet);
					printf("pacchetto di notifica con porta id: %d\n", trick->id_port);
				#endif
				
					if ((last_notify < trick->id_notify) || (last_seq == trick->id_sequence)) { /* non e' in ritardo abissale */
						if ((trick->id_sequence < last_seq) || (trick->id_sequence > last_seq)) { /* nuova sequenza di notifiche */
							m_id_ports.message = trick->type_packet - NOTIFY_F;
							last_seq = trick->id_sequence;
						}
						last_notify = trick->id_notify;
						m_id_ports.message--; /* nuova notifica della stessa sequenza, decrementa contatore */
						m_id_ports.ports[m_id_ports.message] = trick->id_port + 1000;
						ptmp = find_port(port_list, trick->id_port + 1000); /* abbiamo spedito ID lato mobile, vogliamo quello fixed */
						if (ptmp == NULL) { /* porta di cui il fixed non era a conoscenza */
							if (m_id_ports.message == 0) {
							/* non ci sono piu' pacchetti di notifica, costruzione della lista delle porte aperte
							 * e reset-default per la prossima volta */
								m_id_ports.message = trick->type_packet - NOTIFY_F; /* lunghezza lista porte del mobile */
								build_act_ports(&port_list, &m_id_ports);
								m_id_ports.message = 0; /* reset per la prossima notifica */
								break;
							}
							insert_port(&port_list, trick->id_port + 1000);
							break; /* esce forzatamente dallo switch */
						}
						ptmp->not_to_fixed = *trick;	/* aggiorna la notifica */
						ptmp->last_tot_packets = ptmp->tot_packets - ptmp->medium_tot_packets;
						good_percentual = partial_pp_lost(port_list);
						ptmp->ack_packets += ptmp->not_to_fixed.n_received;
						ptmp->nack_packets = ptmp->tot_packets - ptmp->ack_packets;
						ptmp->medium_tot_packets = ptmp->tot_packets;
						mighty_f_ports(&port_list); /* calcolo performance delle porte */
						if (m_id_ports.message == 0) {
						/* non ci sono piu' pacchetti di notifica, costruzione della lista delle porte aperte
						* e reset-default per la prossima volta */
							m_id_ports.message = trick->type_packet - NOTIFY_F; /* lunghezza lista porte del mobile */
							build_act_ports(&port_list, &m_id_ports);
							m_id_ports.message = 0; /* reset per la prossima notifica */
						}
					}
				}
			}
		}
		if (FD_ISSET(new_app_fd, &s_writefds)) {
			memcpy(data_app_fixed, &(to_app_packets[0].id_packet), sizeof(uint32_t));
			memcpy(&(data_app_fixed[4]), &(to_app_packets[0].data), DATA_BUFF_SIZE - sizeof(uint32_t));
			safe_write(new_app_fd, DATA_BUFF_SIZE, data_app_fixed);
			udp_shift_pack(to_app_packets, 0);	/* clear e shift */
			if (to_app_packets[0].type_packet == FALSE)	/* non ce ne sono piu' */
				FD_CLR(new_app_fd, &writefds);
		}
		if (nready == 0) {	/* timeout espirato, nessun fd pronto */
			if (check_inline_pack(up_packets, to_app_packets, &last_uploaded, up_packets[0].udp_data.id_packet - 1))
				FD_SET(new_app_fd, &writefds);
		}
		index_to = BUFF_PACK_SIZE;
		timeout.tv_sec = timeout.tv_usec = 0;
		index_to = manage_timeout(up_packets, &timeout);
	}

	return 0;
}
