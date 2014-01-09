#ifndef __OUR_CONST
#define __OUR_CONST

/************************************************
 * Costanti globali e macro -- LabReti 2007/2008
 *
 * Alberti Michele - Pomili Luigi
 ***********************************************/

#define HIDDEN					static
#define ERROR					((int) -1)
#define FALSE					0
#define TRUE					1
#define CLEAN_TRUE				201			/* true per il campo clean */
#define CLEAN_FALSE				200			/* false per il campo clean */
#define STANDARD				1			/* stardard packet */
#define NOTIFY_F				3			/* statistiche della rete da spedire al fixed */
#define NOTIFY_SENT				1			/* spedizione della notifica effettuata, attesa di ack o nack */
#define NOTIFY_SUCC				2			/* spedizione della notifica ha ricevuto ack dal monitor */
#define NOTIFY_FAIL				3			/* spedizione della notifica ha ricevuto nack dal monitor */
#define BUFF_PACK_SIZE			5
#define BUFF_TO_APP				10
#define N_PORT_UDP				3			/* numero porte UDP */
#define TYPE_SIZE				1
#define MESSAGE_SIZE			4
#define PORT_SIZE				2
#define DATA_BUFF_SIZE			100
#define MAX_TIME				148000		/* microsec */
#define	ONE_MILLION				1000000
#define SELECT_LAG				10000		/* ritardo introdotto dalla select */
#define	PERCENTUAL				100
#define INIT_PERCENTUAL			33			/* 33% iniziale */
#define INIT_BAD_PERCENTUAL		25			/* 25% fiducia per ogni porta scadente rispetto al "sindaco" */
#define	DIFF_RANGE				35			/* 35% scarto tra il "sindaco" e la porta in esame */
#define NACK_FLAG				2
#define NEW_ENTRY_FLAG			1
#define INTER_PACKET			40000		/* interleaving tra pacchetti */
#define	MAX_P_TIME				150000		/* microsec */
#define STEP_NOTIFY				MAX_P_TIME	/* scarto di ID per pacchetti di notifica */
#define NOTIFY_ID_OFFSET		10			/* scarto di ID per i pacchetti di notifica con eccezione */
#define N_MAX_RESEND			1			/* massimo numero di resending per ogni pacchetto dal fixed */

#define HOST_IP					(int8_t*) "127.0.0.1"

#define RANDOM_N(n)			((n) = ((PERCENTUAL - 1) * ((double) rand() / RAND_MAX + 0.0)))
#define S_2_I(s)			(((uint64_t) (s).tv_sec * ONE_MILLION) + (s).tv_usec)
#define BUFFERED_TIME(packet)	(s_2_i(&((packet).time_last_send)) - s_2_i(&((packet).arrived))) / 1000
#define PACK_FROM_LNOT(m, n)	((m) >= (n) ? ((m) - (n)) : ((n) - (m)))

#endif
