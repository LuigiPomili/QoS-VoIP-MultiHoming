#ifndef	__OUR_STRUCT
#define __OUR_STRUCT

/************************************************
 * Strutture dati -- LabReti 2007/2008
 *
 * Alberti Michele - Pomili Luigi
 * *********************************************/

#include "const.h"

typedef struct udp_packet {
	uint32_t id_packet;	/* id del pacchetto da spedire */
	uint8_t type_packet;	/* denota il tipo di pacchetto: 0 - FALSE, 1 - standard, 2 - ack, 3 - stat_notify */
	int8_t data[DATA_BUFF_SIZE - sizeof(uint32_t)];
	uint8_t time_buffered;		/* differenza tra (time_last_send - arrived) */
	int64_t the_mighty_fix;
} __attribute__((packed)) udp_packet;

/* data struct per la bufferizzazione dei pacchetti */
typedef struct buf_packet {
	udp_packet udp_data;
	struct timeval arrived;	/* dall'applicazione */
	struct timeval time_last_send;
	uint16_t id_port;		/* porta monitor utilizzata nella spedizione */
	int8_t flag_to_send;	/* denota se il pacchetto e' da spedire (TRUE) o meno */
	struct timeval delivered;	/* arrivo in questo balancer */
	struct timeval delay;		/* ritardo accumulato all'incirca */
	uint8_t n_resending;	/* massimo numero di rispedizioni possibili */
} buf_packet;

/* data struct per il mantenimento dell'ultimo pacchetto
 * ricevuto dal monitor */
typedef struct mon_notify {
	int8_t type;
	uint32_t message;
	uint16_t ports[N_PORT_UDP];
} mon_notify;

/* struttura utilizzata per memorizzare quale porta del monitor ha dato
 * errore nella ricerca di spedire qualcosa, e che tipo di errore */
typedef struct errno_port {
	int32_t type_errno;
	uint16_t id_port;
} errno_port;

/* pacchetto di notifica delle statistiche delle porte */
typedef struct stat_notify {
	uint32_t id_notify;
	uint8_t type_packet;	/* utilizzata anche per spedire la lunghezza della lista delle porte
							attive nel lato mobile */
	uint8_t id_sequence;	/* sequenza a cui appartiene il pacchetto di notifica, distinzione lato fixed */
	uint8_t n_received;	/* numero pacchetti ricevuti dalla porta */
	uint16_t id_port;	/* porta monitor dal quale si e' ricevuto */
} __attribute__((packed)) stat_notify;

/* data struct per il monitoraggio delle prestazioni delle porte */
typedef struct port_monitoring {
	uint16_t id_port;
	uint32_t tot_packets;
	uint32_t ack_packets;
	uint32_t nack_packets;
	uint8_t last_tot_packets;	/* [FIXED] numero pacchetti spediti prima dell'ultima notifica */
	uint32_t medium_tot_packets;	/* [FIXED] copia del tot_packets per il calcolo di good_percentual */
	struct sockaddr_in recv;	/* config lato monitor */
	struct port_monitoring	
					*pm_next, 
					*pm_prev;
	double performance;	/* % bonta' */
	uint8_t state_notify; /* determina lo stato della notify: 0 - non spedita, 1 - spedita, 2 - successo, 3 - insuccesso */
	stat_notify not_to_fixed; /* notifiche riguardanti le prestazione per il fixed */
} port_monitoring;

/* utile a definire il ritardo introdotto dal monitor */
typedef struct lag_s {
	int64_t time;
	uint32_t counter;
} lag_s; 

#endif
