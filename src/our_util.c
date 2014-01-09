/************************************************
 * Utility generali -- LabReti 2007/2008
 *
 * Alberti Michele - Pomili Luigi
 ***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

#include "datastruct.h"
#include "list_util.h"

#define SOCKET_ERROR	((int) -1)
#define BACK_LOG		0

/* funzioni implementate in questo modulo */
int32_t setup_conn(int32_t type, int16_t sin_family, uint16_t local_port, const int8_t *local_ip, uint32_t dimBuftx, uint32_t dimBufrx, uint8_t tcp_no_delay);
void connect_TCP(int32_t fd, const struct sockaddr_in *server);
int32_t listen_accept(int32_t listen_fd, struct sockaddr_in *new_cli);
void setsockoption(int32_t fd, int32_t level, int32_t optname, uint32_t optval);
void setting_up_side(struct sockaddr_in *side, int16_t sin_family, uint16_t port, const int8_t *ip);
uint8_t manage_notify(mon_notify *new_notify, int32_t fd);
void build_act_ports(port_monitoring **plist, mon_notify *config);
void clear_notify_s(mon_notify *config);
void clear_buffer(buf_packet *buffer);
uint8_t oldest_pack(buf_packet *buffer);
uint8_t index_pack(int32_t *buffer, uint8_t type_packet, uint32_t id);
uint8_t index_free_pack(buf_packet *buffer);
void drop_expired(buf_packet *buffer);
void clear_pack(buf_packet *buffer, uint8_t index);
uint8_t first_pack_to_send(buf_packet *buffer);
void safe_read(int32_t fd, int32_t size, int8_t *buffer);
void safe_write(int32_t fd, int32_t size, int8_t *buffer);
void mighty_f_ports(port_monitoring **list);
port_monitoring* choose_port(port_monitoring *list, uint32_t number);
struct errno_port send_the_pack(int32_t *data, uint8_t type_packet, int32_t fd, port_monitoring *list);
int32_t general_sendto(int32_t *data, uint16_t size_data, int32_t fd, struct sockaddr_in *monitor);
void general_recvfrom(int32_t *data, uint16_t size_data, int32_t fd, struct sockaddr_in *monitor);
void sort_buffer(buf_packet *buffer);
void mighty_f_revenge(buf_packet *buffer, uint8_t index, struct timeval *act_zero, uint32_t first_pack_id);
void i_2_s(int64_t i, struct timeval *s);
inline int64_t s_2_i(struct timeval *s);
uint8_t check_inline_pack(buf_packet *buffer, udp_packet *buffer_udp, uint32_t *lup, uint32_t id);
uint8_t manage_timeout(buf_packet *buffer, struct timeval *timeout);
void udp_clear_buffer(udp_packet *buffer);
void udp_shift_pack(udp_packet *buffer, uint8_t index);
uint8_t index_udpfree_pack(udp_packet *buffer);
uint32_t min_to_before_rsd(buf_packet *buffer, uint8_t ppack_lost, uint32_t lag, struct timeval *timeout);

/* implementazione */

/* settaggio connessione TCP/UDP */
int32_t setup_conn(int32_t type, int16_t sin_family, uint16_t local_port, const int8_t *local_ip, uint32_t dimBuftx, uint32_t dimBufrx, uint8_t tcp_no_delay) {

	struct sockaddr_in	local;
	int32_t	socket_fd;

	socket_fd = socket(sin_family, type, 0);
	if (socket_fd == SOCKET_ERROR) {
		perror("error performing socket()");
		exit(EXIT_FAILURE);
	}
	/* opzioni varie */
	if (dimBuftx > 0)	setsockoption(socket_fd, SOL_SOCKET, SO_SNDBUF, dimBuftx);
	if (dimBufrx > 0)	setsockoption(socket_fd, SOL_SOCKET, SO_RCVBUF, dimBufrx);
	if (tcp_no_delay == 1)	setsockoption(socket_fd, IPPROTO_TCP, TCP_NODELAY, tcp_no_delay);

	/* evitiamo EADDRINUSE */
	setsockoption(socket_fd, SOL_SOCKET, SO_REUSEADDR, 1);

	/* settaggio del lato local */
	setting_up_side(&local, AF_INET, local_port, local_ip);

	/* bind del socket */
	if (bind(socket_fd, (struct sockaddr*) &local, sizeof(local)) == SOCKET_ERROR) {
		perror("error performing bind()");
		exit(EXIT_FAILURE);
	}
	return socket_fd;
}

/* connessione ad un "server" remoto */
void connect_TCP(int32_t fd, const struct sockaddr_in *server) {
	
	if (connect(fd, (struct sockaddr*) server, sizeof(*server)) == SOCKET_ERROR) {
		perror("error performing connect()");
		exit(EXIT_FAILURE);
	}
}

/* ascolto-accettazione di nuovi "client" */
int32_t listen_accept(int32_t listen_fd, struct sockaddr_in *new_cli) {

	int32_t	new_fd;
	socklen_t len;
	
	/* aspetta nuove connnessioni, zero connessioni pendenti */
	if (listen(listen_fd, BACK_LOG) == SOCKET_ERROR) {
		perror("error performing listen()");
		exit(EXIT_FAILURE);
	}
	/* accettazione nuova connessione */
	len = sizeof(struct sockaddr_in);
	if ((new_fd = accept(listen_fd, (struct sockaddr*) new_cli, &len)) == SOCKET_ERROR) {
		perror("error performing accept()");
		exit(EXIT_FAILURE);
	}

	return new_fd;
}

/* settaggio parametri del socket passato */
void setsockoption(int32_t fd, int32_t level, int32_t optname, uint32_t optval) {
	
	if (setsockopt(fd, level, optname, (char*) &optval, sizeof(optval)) == SOCKET_ERROR) {
		perror("error performing setsockopt()");
		exit(EXIT_FAILURE);
	}
	return ;
}

/* settaggio del "side" della connessione */
void setting_up_side(struct sockaddr_in *side, int16_t sin_family, uint16_t port, const int8_t *ip) {

	memset(side, 0, sizeof(struct sockaddr_in));
	side->sin_family = sin_family;
	side->sin_port = htons(port);

	if (strcmp((char *) ip, "INADDR_ANY") == 0) {
		side->sin_addr.s_addr = htonl(INADDR_ANY);
		printf("sembra andare...\n");
	}
	else {
		side->sin_addr.s_addr = inet_addr((char *)ip);
		printf("hai specificato un IP: %s, porta: %u\n", ip, port);
	}
}

/* gestione della notifica da parte del monitor (C, A, N)*/
uint8_t manage_notify(mon_notify *new_notify, int32_t fd) {
	static	uint16_t readb = TYPE_SIZE;
	static	uint16_t tmp = 0;
	ssize_t	ris;
	
	switch (readb)	{
		case TYPE_SIZE: {	/* lettura del primo byte, determina il tipo di notifica */
			if ((ris = read(fd, &(new_notify->type), readb)) == ERROR) {
				fprintf(stderr, "[TYPE_SIZE] fatal error performing read. errno %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			readb = MESSAGE_SIZE;
			return FALSE;
		}
		case MESSAGE_SIZE: {	/* lettura del corpo della notifica */
			if ((ris = read(fd, &(new_notify->message), readb)) == ERROR) {
				fprintf(stderr, "[MESSAGE_SIZE] fatal error performing read. errno %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			/* switch interno */
			switch (new_notify->type) {	/* e' una notifica di "config", i prossimi byte sono id di porte */
				case 'C': {
					readb = PORT_SIZE;
					tmp = new_notify->message;
					break;
				}
				default: {
					readb = TYPE_SIZE;	/* n/ack packet ricevuto */
					return TRUE;
				}
			}
			/* fine switch interno */
			break;
		}
		case PORT_SIZE:	{
			/* salvataggio configurazione propria delle porte attive */
			if ((ris = read(fd, &(new_notify->ports[tmp - 1]), readb)) == ERROR) {
				fprintf(stderr, "[PORT_SIZE] fatal error performing read. errno %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (--tmp == 0) {
				readb = TYPE_SIZE;
				return TRUE;
			}
		}
	}
	return FALSE;
}

/* costruisce la lista delle porte in determinato istante, solitamente dopo
 * una notifica di configurazione da parte del monitor */
void build_act_ports(port_monitoring **plist, mon_notify *config) {
	uint8_t	i, j, length, found;

	length = list_lenght(*plist);

	/* elimina le porte non piu' attive dalla lista opportuna */
	for (; length > 0; length--) {
		found = FALSE;
		for (i = 0; i < config->message; i++) {
			if (config->ports[i] == (*plist)->id_port)
				found = TRUE;
		}
		if (!found)	/* porta non piu' usata */
			remove_port(plist);
		else {
			(*plist) = (*plist)->pm_next;
		}
	}

	j = 0;
	/* elimina dalla config->ports le porte gia' attive */
	for (i = 0; i < config->message; i++) {
		if (check_port(*plist, config->ports[i]) == TRUE) {
			config->ports[i] = 0;
			j++;
		}
	}

	if (j == config->message)
		return;
	
	/* inserimento delle nuove porte */
	for (i = 0; i < config->message; i++) {
		if (config->ports[i] != 0) {
			insert_port(plist, config->ports[i]);
		}
	}
}

/* reset della struttura della notifica dal monitor */
void clear_notify_s(mon_notify *config) {
	register uint8_t i;

	config->type = config->message = 0;
	for (i = 0; i < N_PORT_UDP; i++)
		config->ports[i] = 0;
}

/* reset dell'intero buffer in entrata/uscita */
void clear_buffer(buf_packet *buffer) {
	register uint8_t i;

	for (i = 0; i < BUFF_PACK_SIZE; i++) {
		clear_pack(buffer, i);
	}
}

/* determina il pacchetto che e' stato bufferizzato da piu' tempo */
uint8_t oldest_pack(buf_packet *buffer) {
	register uint8_t i, j;
	uint32_t min;
	
	/* determinazione del tempo in base agli ID dei pacchetti */
	min = buffer[0].udp_data.id_packet;
	j = 0;
	for(i = 1; i < BUFF_PACK_SIZE; i++)
		if (min > buffer[i].udp_data.id_packet) {
			min = buffer[i].udp_data.id_packet;
			j = i;
		}
	
	return j;
}

/* indice dell'elemento del buffer contenente l'id del pacchetto cercato */
uint8_t index_pack(int32_t *buffer, uint8_t type_packet, uint32_t id) {
	register uint8_t i;

	switch (type_packet) {
		case STANDARD: {
			for (i = 0; i < BUFF_PACK_SIZE; i++)
				if ((((buf_packet*) buffer)[i].udp_data.id_packet == id) && (s_2_i(&(((buf_packet*) buffer)[i].arrived)) != 0))
					return i;
			break;
		}
		case NOTIFY_F: {
		}
	}

	return BUFF_PACK_SIZE;	/* id non trovato */
}

/* indice del primo elemento libero nel buffer */
uint8_t index_free_pack(buf_packet *buffer) {
	register uint8_t i;

	for (i = 0; i < BUFF_PACK_SIZE; i++) {
		if (buffer[i].udp_data.type_packet == FALSE)
			return i;
	}

	return BUFF_PACK_SIZE;	/* nessun elemento libero */
}

/* calcola il tempo trascorso dal pacchetto nel buffer in uscita verso il monitor */
static inline int8_t calc_u_time(struct timeval *tmp, struct timeval *arrived) {
	if (((tmp->tv_usec - arrived->tv_usec) + ((tmp->tv_sec - arrived->tv_sec) * ONE_MILLION)) >= MAX_TIME)
		return TRUE;
	return FALSE;
}

/* scarta dal buffer in uscita i pacchetti che superano un tempo limite per avere alla fine
 * una spediazione nei 150ms totali */
void drop_expired(buf_packet *buffer) {
	register uint8_t i;
	struct timeval tmp;

	gettimeofday(&tmp, NULL);

	for (i = 0; i < BUFF_PACK_SIZE; i++) {
		if (calc_u_time(&tmp, &(buffer[i].arrived)) && (s_2_i(&(buffer[i].arrived)) != 0)) {
			clear_pack(buffer, i);
		}
	}
}

/* ritorna l'indice del buffer del primo pacchetto da spedire */
uint8_t first_pack_to_send(buf_packet *buffer) {
	register uint8_t i, j;
	uint32_t min;

	for (i = 0; i < BUFF_PACK_SIZE; i++)
		if (buffer[i].flag_to_send != FALSE) {
			min = buffer[i].udp_data.id_packet;
			break;
		}

	if (i < BUFF_PACK_SIZE) {
		for (j = i + 1; j < BUFF_PACK_SIZE; j++) {
			if (buffer[j].flag_to_send != FALSE)
				if (min > buffer[j].udp_data.id_packet) {
					min = buffer[j].udp_data.id_packet;
					i = j;
				}
		}
	}
	return i;
}

/* lettura dal socket TCP, dall'applicazione */
void safe_read(int32_t fd, int32_t size, int8_t *buffer) {
	int32_t	ris, position;

	position = 0;
	
	do {
		if ((ris = recv(fd, &(buffer[position]), size - position, MSG_NOSIGNAL)) > 0) {
			position += ris;
			if ((size - position) == 0)
				break;
		}
		else if (ris == FALSE) {		/* chiusura comunicazione */
			close(fd);
			exit(EXIT_SUCCESS);
		}
	} while ((ris > 0) || (errno == EINTR));
	if (ris == ERROR) {
		fprintf(stderr, "[SAFE READ] fatal error performing read. errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/* scrittura sul socket TCP, verso l'applicazione */
void safe_write(int32_t fd, int32_t size, int8_t *buffer) {
	int32_t	ris, position;

	position = 0;
	
	do {
		if ((ris = send(fd, &(buffer[position]), size - position, MSG_NOSIGNAL)) > 0) {
			position += ris;
			if ((size - position) == 0)
				break;
		}
	} while ((ris > 0) || (errno == EINTR));
	if (ris == ERROR) {
		fprintf(stderr, "[SAFE WRITE] fatal error performing read. errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static inline void up_performance(port_monitoring **list, uint16_t bound);

/* formula per il calcolo prestazionale della rete, porta per porta */
void mighty_f_ports(port_monitoring **list) {
	uint16_t length, length_tmp, major;
	uint32_t sum_ack, sum_nack, sum_tot;
	double sum2_tot;

	length = list_lenght(*list);
	sum_ack = sum_nack = sum_tot = sum2_tot = major = 0;

	for (length_tmp = length; length_tmp > 0; length_tmp--) {
		sum_ack += (*list)->ack_packets;
		sum_nack += (*list)->nack_packets;
		if ((*list)->nack_packets != 0)
			(*list)->performance = ((double) (*list)->ack_packets / (double) (*list)->nack_packets);
		else
			(*list)->performance = (*list)->ack_packets;
		/* major: porta con prestazioni maggiori */
		if (major < (*list)->tot_packets)
			major = (*list)->tot_packets;
		(*list) = (*list)->pm_next;
	}

	/* inizialmente assegnazione valori equi per tutte le porte */
	if (((sum_tot += (sum_ack + sum_nack)) == 0) || sum_ack == 0) {
		for (length_tmp = length; length_tmp > 0; length_tmp--) {
			(*list)->performance = PERCENTUAL / length;
			(*list) = (*list)->pm_next;
		}
		return ;
	}

	for (length_tmp = length; length_tmp > 0; length_tmp--) {
		(*list)->performance *= ((double) (*list)->tot_packets / (double) sum_tot);
		sum2_tot += (*list)->performance;
		(*list) = (*list)->pm_next;
	}

	/* normalizzazzione da 0 - 100 */
	for (length_tmp = length; length_tmp > 0; length_tmp--) {
		(*list)->performance = ((PERCENTUAL * (*list)->performance) / sum2_tot);
		(*list) = (*list)->pm_next;
	}

	up_performance(list, major);
}

/* cerca di dare fiducia alle porte che attualmente non hanno grandi prestazioni
 * altrimenti non potrebbero essere piu' scelte */
static inline void up_performance(port_monitoring **list, uint16_t bound) {
	uint8_t length, length_tmp, bad_ports;
	int32_t perc_bound = ((double) bound / PERCENTUAL) * DIFF_RANGE;
	int32_t sub_perc;
	port_monitoring* the_most = *list;
	
	length = list_lenght(*list);
	bad_ports = 0;
	for (length_tmp = length; length_tmp > 0; length_tmp--) {
		if (((*list)->performance) > (the_most->performance))
			the_most = *list; 
		*list = (*list)->pm_next;
	}

	for (length_tmp = length; length_tmp > 0; length_tmp--) {
		if (((*list)->tot_packets < perc_bound) && (((*list)->performance) < INIT_BAD_PERCENTUAL)) {
			bad_ports++;
			the_most->performance += (*list)->performance;
			(*list)->performance = INIT_BAD_PERCENTUAL;		/* 25% */
		}
		(*list) = (*list)->pm_next;
	}

	/* calcola quanto togliere alle porte "buone" in base a quante
	 * porte "non buone" ci sono */
	sub_perc = (INIT_BAD_PERCENTUAL * bad_ports) / (length - bad_ports);
	the_most->performance -= sub_perc;
}

/* sceglie la porta UDP del monitor da utilizzare in base alle performance
 * attuali di tutte quelle attive
 * number: risultato del lancio di un dado tra 0 e 99 */
port_monitoring* choose_port(port_monitoring *list, uint32_t number) {
	uint8_t length;
	double	tmp_performance;

	length = list_lenght(list);
	tmp_performance = list->performance;

	/* si cerca la porta "contenente" il numero casuale, quella e'
	 * la porta che verra' utilizzata */
	for (; length > 0; length--) {
		if (number >= tmp_performance) {
			list = list->pm_next;
			tmp_performance += list->performance;
		}
		else
			break;
	}
	
	return list;
}

/* spedisce il pacchetto verso il monitor, polimorfa */
struct errno_port send_the_pack(int32_t *data, uint8_t type_packet, int32_t fd, port_monitoring *list) {
	double rand_n;
	port_monitoring *ptmp;
	int32_t tmp_errno;
	struct errno_port ris;

	/* default */
	ris.type_errno = EXIT_SUCCESS;
	ris.id_port = 0;
	
	mighty_f_ports(&list);	/* calcolo performance */
	RANDOM_N(rand_n);	/* lancio del dado per la scelta della porta */
	ptmp = choose_port(list, rand_n);
	/* spedizione del pacchetto in base al tipo */
	switch (type_packet) {
		case STANDARD : {
			((buf_packet*) data)->id_port = ptmp->id_port;
			if ((tmp_errno = general_sendto((int32_t*) &(((buf_packet*) data)->udp_data), sizeof(udp_packet), fd, &(ptmp->recv))) != EXIT_SUCCESS) {
				/* ritorna l'errno verificatosi e la porta utilizzata */
				ris.type_errno = tmp_errno;
				ris.id_port = ptmp->id_port;
				return ris;
			}
			((buf_packet*) data)->flag_to_send = FALSE;
			break;
		}
		case NOTIFY_F : {
			if ((tmp_errno = general_sendto((int32_t*) ((stat_notify*) data), sizeof(stat_notify), fd, &(ptmp->recv))) != EXIT_SUCCESS) {
				/* ritorna l'errno verificatosi e la porta utilizzata */
				ris.type_errno = tmp_errno;
				ris.id_port = ptmp->id_port;
				return ris;
			}
			break;
		}
	}
	ptmp->tot_packets++;	/* aumento pacchetti totali spediti sulla porta scelta */
	ris.id_port = ptmp->id_port;
	return ris;
}

/* spedizione effettiva verso il monitor, diversi tipi di pacchetti */
int32_t general_sendto(int32_t *data, uint16_t size_data, int32_t fd, struct sockaddr_in *monitor) {	/* data e' un puntatore generico! */
	int32_t bytes;
	uint32_t tot_send, tolen;

	tot_send = size_data;
	tolen = sizeof(struct sockaddr);
	bytes = sendto(fd, data, tot_send, MSG_DONTWAIT, (struct sockaddr*) monitor, tolen);
	if (bytes == ERROR) {
		fprintf(stderr, "fatal error performing sendto. errno: %s\n", strerror(errno));
		return(errno);
	}

	return EXIT_SUCCESS;
}

/* riceve pacchetti provenienti dal monitor */
void general_recvfrom(int32_t *data, uint16_t size_data, int32_t fd, struct sockaddr_in *monitor) {
	uint32_t bytes, tot_send, tolen;

	tot_send = size_data;
	tolen = sizeof(struct sockaddr);
	bytes = recvfrom(fd, data, tot_send, 0, (struct sockaddr*) monitor, &tolen);
	if (bytes == ERROR) {
		fprintf(stderr, "fatal error performing recvfrom. errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (bytes == FALSE) {		/* chiusura comunicazione */
		close(fd);
		exit(EXIT_FAILURE);
	}
}

/* ordina i pacchetti attualmente bufferizzati in entrata */
void sort_buffer(buf_packet *buffer) {
	register uint8_t i,j;
	buf_packet tmp;
	uint32_t id_packet_i, id_packet_j;

	/* ordina per ID dei pacchetti */
	for (i = 0; i < BUFF_PACK_SIZE - 1; i++) {
		id_packet_i = buffer[i].udp_data.id_packet;
		for (j = i + 1; j < BUFF_PACK_SIZE; j++) {
			id_packet_j = buffer[j].udp_data.id_packet;
			if (id_packet_i > id_packet_j) {
				tmp = buffer[i];
				buffer[i] = buffer[j];
				buffer[j] = tmp;
				id_packet_i = id_packet_j;
			}
		}
	}

	/* shifta a destra tutti i pacchetti in modo che gli ID siano (es.): 1230000 */
	for (i = 0; i < BUFF_PACK_SIZE - 1; i++) {
		for (j = i + 1; j < BUFF_PACK_SIZE; j++) {
			if (buffer[i].udp_data.type_packet == FALSE) {
				if (buffer[j].udp_data.type_packet != FALSE) {
					tmp = buffer[i];
					buffer[i] = buffer[j];
					buffer[j] = tmp;
				}
			}
			else
				break;
		}
	}
}

/* reset della struttura contenente il pacchetto */
void clear_pack(buf_packet *buffer, uint8_t index) {
	buffer[index].udp_data.time_buffered = CLEAN_TRUE;
	buffer[index].udp_data.id_packet = 0;
	buffer[index].udp_data.type_packet = FALSE;
	buffer[index].udp_data.the_mighty_fix = 0;
	buffer[index].time_last_send.tv_sec = buffer[index].time_last_send.tv_usec = 0;
	buffer[index].arrived.tv_sec = buffer[index].arrived.tv_usec = 0;
	buffer[index].flag_to_send = FALSE;
	buffer[index].n_resending = N_MAX_RESEND;
	buffer[index].delivered.tv_sec = buffer[index].delivered.tv_usec = 0;
	buffer[index].delay.tv_sec = buffer[index].delay.tv_usec = 0;
}

/* da struttura timeval a valore totale in microsecondi */
inline int64_t s_2_i(struct timeval *s) {
	return (((int64_t) s->tv_sec * ONE_MILLION) + s->tv_usec);
}

/* formula per il calcolo approssimato degli usec da attendere per ogni pacchetto,
 * ad ogni pacchetto ricevuto si aggiorna il valore del "nostro" zero teorico e 
 * successivamente quindi l'aggiornamento dei delay di tutti i paccheti bufferizzati */
void mighty_f_revenge(buf_packet *buffer, uint8_t index, struct timeval *act_zero, uint32_t first_pack_id) {
	int64_t tmp_act_zero, diff;
	register uint8_t i;
	
	diff = s_2_i(&(buffer[index].delivered)) - (s_2_i(act_zero) + (INTER_PACKET * (buffer[index].udp_data.id_packet - first_pack_id))) - buffer[index].udp_data.the_mighty_fix;
	i_2_s(diff, &(buffer[index].delay));
	if ((diff = (diff - buffer[index].udp_data.time_buffered)) < 0) {
		/* aggiornamenti del nostro "zero" dei pacchetti arrivati */
		tmp_act_zero = s_2_i(act_zero);
		tmp_act_zero += diff;
		i_2_s(tmp_act_zero, act_zero);
		/* aggiornamento delay dei pacchetti bufferizzati in entrata */
		for (i = 0; i < BUFF_PACK_SIZE; i++) {
			if (buffer[i].udp_data.type_packet != FALSE) {
				diff = s_2_i(&(buffer[i].delivered)) - (tmp_act_zero + (INTER_PACKET * (buffer[i].udp_data.id_packet - first_pack_id)));
				i_2_s(diff, &(buffer[i].delay));
			}
		}
	}
}

/* da microsecondi a struttura timeval */
void i_2_s(int64_t i, struct timeval *s) {
	s->tv_sec = i / ONE_MILLION;
	s->tv_usec = i % ONE_MILLION;
}

/* invia al buffer "ultimo" tutti i pacchetti bufferizzati i cui ID sono in linea */
uint8_t check_inline_pack(buf_packet *buffer, udp_packet *buffer_udp, uint32_t *lup, uint32_t id) {
	register uint8_t i;
	uint8_t doit = FALSE;

	for (i = 0; i < BUFF_PACK_SIZE; i++) {
		if ((buffer[i].udp_data.id_packet == (id + 1)) && (buffer[i].udp_data.type_packet != FALSE)) {
			buffer_udp[index_udpfree_pack(buffer_udp)] = buffer[i].udp_data;
			(*lup) = buffer[i].udp_data.id_packet;	/* aggiorno ultimo upload verso il buffer "ultimo" */
			clear_pack(buffer, i);
			id++;
			doit = TRUE;
		}
		else
			break;
	}
	sort_buffer(buffer);
	return doit;
}

uint8_t manage_timeout(buf_packet *buffer, struct timeval *timeout) {
	struct timeval tmp;
	int64_t min;

	gettimeofday(&tmp, NULL);
	min = 0;
	
	/* all'inizio la formula mighty_f_revenge e' scarsa, quindi
	 * bisognerebbe aggiungere un valore di "trust" basato sul numero di 
	 * pacchetti spediti, perche' la mighty_f_revenge migliora nel tempo */
	if (buffer[0].udp_data.type_packet != FALSE) {
		if ((min = (MAX_P_TIME - s_2_i(&(buffer[0].delay))) - (s_2_i(&tmp) - s_2_i(&(buffer[0].delivered))) - SELECT_LAG) < 0)
			min = 0; /* up immediatamente */
		i_2_s(min, timeout);
		return FALSE;
	}
	else
		return BUFF_PACK_SIZE;
}

/* reset della struttura contenente il pacchetto, shift dei pacchetti */
void udp_shift_pack(udp_packet *buffer, uint8_t index) {
	register uint8_t i;

	for (i = index; i < BUFF_TO_APP - 1; i++) {
		buffer[i] = buffer[i + 1];
	}
	/* ha shiftato tutti tranne l'ultimo, lo pulisce */
	buffer[BUFF_TO_APP - 1].time_buffered = CLEAN_TRUE;
	buffer[BUFF_TO_APP - 1].id_packet = 0;
	buffer[BUFF_TO_APP - 1].type_packet = FALSE;
}

/* reset dell'intero buffer in entrata/uscita udp */
void udp_clear_buffer(udp_packet *buffer) {
	register uint8_t i;

	for (i = 0; i < BUFF_TO_APP; i++) {
		buffer[i].time_buffered = CLEAN_TRUE;
		buffer[i].id_packet = 0;
		buffer[i].type_packet = FALSE;
	}
}

/* indice del primo elemento libero nel buffer */
uint8_t index_udpfree_pack(udp_packet *buffer) {
	register uint8_t i;

	for (i = 0; i < BUFF_TO_APP; i++) {
		if (buffer[i].type_packet == FALSE)
			return i;
	}

	return BUFF_TO_APP;	/* nessun elemento libero */
}

/* tempo minimo di attesa prima di rispedire il paccheto da fixed a mobile */
uint32_t min_to_before_rsd(buf_packet *buffer, uint8_t ppack_lost, uint32_t lag, struct timeval *timeout) {
	uint8_t i, index; 
    int64_t ttmp; 
    struct timeval time; 
 
    index = BUFF_PACK_SIZE; 
    gettimeofday(&time, NULL); 
    for (i = 0; i < BUFF_PACK_SIZE; i++) {
		if ((buffer[i].udp_data.type_packet != FALSE) && (s_2_i(&buffer[i].time_last_send) != 0)) {
			/* dubious formula dal pc di Gigio: calcola il minimo tempo di attesa prima di resend verso il mobile */
			ttmp = (MAX_P_TIME - (s_2_i(&time) - s_2_i(&(buffer[i].arrived)))) - lag;
			ttmp -= (((double)ppack_lost / PERCENTUAL) * MAX_P_TIME);
			if (ttmp > 0) {
				if ((s_2_i(timeout) > ttmp) || s_2_i(timeout) == 0) {
					i_2_s(ttmp, timeout);
					index = i;
				}
			}
		}
	}
	return index;
}
