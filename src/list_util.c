/*************************************************
 * Utility per liste di porte -- LabReti 2007/2008
 *
 * Alberti Michele - Pomili Luigi
 ************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "datastruct.h"
#include "our_util.h"

/* funzioni implementate in questo modulo */
void make_list(port_monitoring **list);
port_monitoring* insert_port(port_monitoring** list, uint16_t id_port);
void remove_port(port_monitoring** to_remove);
int8_t check_port(port_monitoring *list, uint16_t id_port);
int8_t list_lenght(port_monitoring *list);
void list_print(port_monitoring *list);
port_monitoring* find_port(port_monitoring *list, uint16_t id_port);
port_monitoring* min_port(port_monitoring* list, uint16_t id_port);
uint8_t max_port(port_monitoring* list, uint16_t id_port);
port_monitoring* find_notify(port_monitoring *list, uint32_t id_notify);
port_monitoring* first_notify_snd(port_monitoring *list);
port_monitoring* first_notify_rsd(port_monitoring *list);
uint8_t reset_all_notify(port_monitoring *list);
uint8_t partial_pp_lost(port_monitoring* list);

/* implementazione */

/* nuova lista di porte del monitor */
void make_list(port_monitoring **list) {
	(*list) = NULL;
}

/* inserimento di una nuova porta */
port_monitoring* insert_port(port_monitoring** list, uint16_t id_port) {
	
	/* allocazione memoria e fill dei dati */
	port_monitoring* new = (port_monitoring *) malloc(sizeof(port_monitoring));
	if (new == NULL) {
		fprintf(stderr, "fatal error performing malloc. errno %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	new->id_port = id_port;
	new->tot_packets = new->ack_packets = new->nack_packets = new->last_tot_packets = new->medium_tot_packets = 0;
	setting_up_side(&(new->recv), AF_INET, id_port, HOST_IP);
	/* dettagli struttura notifica verso il fixed */
	new->not_to_fixed.id_port = id_port;
	new->not_to_fixed.n_received = 0;
	new->state_notify = FALSE;

 	
	if ((*list) == NULL) {
		new->pm_next = new;
		new->pm_prev = new;
	}
	else {
		new->pm_next = (*list);
		new->pm_prev = (*list)->pm_prev;
		(*list)->pm_prev->pm_next = new;
		(*list)->pm_prev = new;
	}
	(*list) = new;

	return (*list);
}

/* rimozione di una porta dalla lista delle attive */ 
void remove_port(port_monitoring** to_remove) {
	port_monitoring *tmp = (*to_remove);

	if ((*to_remove)->pm_next != (*to_remove)) {
 		((*to_remove)->pm_prev)->pm_next = (*to_remove)->pm_next;
		((*to_remove)->pm_next)->pm_prev = (*to_remove)->pm_prev;
		(*to_remove) = (*to_remove)->pm_next;
		free(tmp);
	}
	else {
		free(*to_remove);
		*to_remove = NULL;
	}
}

/* determina la presenza o meno di un determinato ID porta nella lista */
int8_t check_port(port_monitoring *list, uint16_t id_port) {
	port_monitoring *tmp = list;

	if (list == NULL)
		return FALSE;

	/* ricerca elemento nella lista, la scorre tutta */
	do {
		if (tmp->id_port == id_port)
			return TRUE;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return FALSE;
}

/* ritorna il puntatore nella lista alla porta specificata tramite ID */
port_monitoring* find_port(port_monitoring *list, uint16_t id_port) {
	port_monitoring *tmp = list;

	if (list == NULL)
		return NULL;

	/* ricerca elemento nella lista, la scorre tutta */
	do {
		if (tmp->id_port == id_port)
			return tmp;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return NULL;
}

/* ritorna la lunghezza della lista porte disponibili */
int8_t list_lenght(port_monitoring *list) {
	port_monitoring *tmp = list;
	int8_t	i = 0;

	if (list == NULL)
		return 0;

	do {
		i++;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return i;
}

/* stampa la lista delle porte disponibili */
void list_print(port_monitoring *list) {
	port_monitoring *tmp = list;
	
	do {
		printf("id port: %d\n", tmp->id_port);
		fflush(stdout);
		tmp = tmp->pm_next;
	} while (tmp != list);
}

/* ritorna il puntatore nella lista della porta con ID minore */
port_monitoring* min_port(port_monitoring* list, uint16_t id_port) {
	port_monitoring *tmp = list;
	port_monitoring *ret;
	uint16_t min = 0;

	if (list == NULL)
		return 0;

	do {
		if ((tmp->id_port != id_port) && (min == 0)) {
			min = tmp->id_port;
			ret = tmp;
		}
		else if ((tmp->id_port != id_port) && (min > tmp->id_port)) {
			min = tmp->id_port;
			ret = tmp;
		}
		tmp = tmp->pm_next;
	} while (tmp != list);

	return ret;
}

/* ritorna il puntatore nella lista della porta con ID maggiore */
uint8_t max_port(port_monitoring* list, uint16_t id_port) {
	port_monitoring *tmp = list;

	if (list == NULL)
		return FALSE;

	do {
		if (tmp->id_port > id_port)
			return FALSE;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return TRUE;
}

/* ritorna il puntatore alla struttura contenente l'ID di notifica passato */
port_monitoring* find_notify(port_monitoring *list, uint32_t id_notify) {
	port_monitoring *tmp = list;
	
	if (list == NULL)
		return NULL;

	do {
		if (tmp->not_to_fixed.id_notify == id_notify)
			return tmp;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return NULL;
}

/* ritorna il puntatore alla prima struttura da spedire */
port_monitoring* first_notify_snd(port_monitoring *list) {
	port_monitoring *tmp = list;
	
	if (list == NULL)
		return NULL;

	do {
		if (tmp->state_notify == FALSE)
			return tmp;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return NULL;
}

/* ritorna il puntatore alla prima struttura da ri-spedire */
port_monitoring* first_notify_rsd(port_monitoring *list) {
	port_monitoring *tmp = list;
	
	if (list == NULL)
		return NULL;

	do {
		if (tmp->state_notify == NOTIFY_FAIL)
			return tmp;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return NULL;
}

/* reset dello stato delle notify */
uint8_t reset_all_notify(port_monitoring *list) {
	port_monitoring *tmp = list;

	if (list == NULL)
		return FALSE;

	do {
		tmp->state_notify = FALSE;
		tmp->not_to_fixed.id_notify = 0;
		tmp = tmp->pm_next;
	} while (tmp != list);

	return TRUE;
}

/* calcola la percentuale dei pacchetti persi nell'ultimo burst */
uint8_t partial_pp_lost(port_monitoring* list) {
	int8_t llenght = list_lenght(list);
	uint32_t tot_sum, ack_sum;
	register uint8_t i;

	tot_sum = 0;
	ack_sum = 0;

	for (i = 0; i < llenght; i++) {
		tot_sum += list->last_tot_packets;
		ack_sum += list->not_to_fixed.n_received;
		list = list->pm_next;
	}

	return ((double) PERCENTUAL / (double) tot_sum) * (double) ack_sum;
}
