#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <commons/collections/list.h>
#include <commons/log.h>

#include "../commons/comunicacion/protocol.h"
#include "../commons/comunicacion/sockets.h"
#include "../commons/consola/consola.h"
#include "../commons/lissandra-threads.h"
#include "../commons/consola/consola.h"
#include "memoria-logger.h"
#include "memoria-config.h"
#include "memoria-main.h"

#define CLIENTES_CANT 10

int clientes[CLIENTES_CANT];
int pos_actual=0;

void atender_cliente(void* entrada) { // le saque el puntero
	//lissandra_thread_t *l_thread = (lissandra_thread_t*) entrada;
	int cliente = *((int*) l_thread->entrada); // QUE SIGNIFICA . lthread
	fd_set descriptores, copia;
	FD_ZERO(&descriptores);
	FD_SET(cliente, &descriptores);
	int select_ret, error;
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 250000000;
	int tamanio_buffers = get_max_msg_size();
	uint8_t buffer[tamanio_buffers], respuesta[tamanio_buffers];

	// SAQUE EL WHILE
	copia = descriptores;
	select_ret = pselect(cliente + 1, &copia, NULL, NULL, &ts, NULL);
	if (select_ret == -1) {
		memoria_log_to_level(LOG_LEVEL_TRACE, false,
				"Fallo en la ejecucion del select del cliente");
		break;
	} else if (select_ret > 0) {
		if ((error = recv_msg(cliente, buffer, tamanio_buffers) < 0)) {
			memoria_log_to_level(LOG_LEVEL_TRACE, false,
					"Fallo al recibir el mensaje del cliente"); // aviso al cliente del error
			if (error == SOCKET_ERROR || error == CONN_CLOSED) {
				break;
			} else {
				continue;
			}
		}
		destroy(buffer);
		if ((error = send_msg(cliente, respuesta) < 0)) { // intento reconectarme
			memoria_log_to_level(LOG_LEVEL_TRACE, false,
					"Fallo al enviar el mensaje al cliente");
			if (error == SOCKET_ERROR || error == CONN_CLOSED) {
				break;
			} else {
				continue;
			}
		}
	}
	memset(buffer, 0, tamanio_buffers);
	memset(respuesta, 0, tamanio_buffers);

	close(cliente);
	free(l_thread->entrada); // warning por lthread
	pthread_exit(NULL);
}

void multiplexar() {
	for(int i = 0 ; i < pos_actual; i++) {
		atender_cliente(clientes[pos_actual]);
	}
	pos_actual = 0;
}

void agregarCliente(cliente) {
	clientes[pos_actual] = cliente;
	pos_actual++;
}

void aceptar_cliente(int servidor) {
	int cliente;
	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;
	int *nuevo_cliente;

	if ((cliente = accept(servidor, (struct sockaddr*) &their_addr, &addr_size))
			== -1) {
		memoria_log_to_level(LOG_LEVEL_TRACE, false,
				"Fallo al aceptar la conexion del cliente");
		return;
	}
	*nuevo_cliente = cliente;
	agregarCliente(nuevo_cliente);
	free(nuevo_cliente);
}

/*
bool termino_cliente(void *elemento) {
	lissandra_thread_t *l_thread = (lissandra_thread_t*) elemento;
	return (bool) l_thread_finalizo(l_thread);
}*/

/*
void finalizar_hilo(void* elemento) {
	lissandra_thread_t *l_thread = (lissandra_thread_t*) elemento;
	l_thread_solicitar_finalizacion(l_thread);
	l_thread_join(l_thread, NULL);
	free(elemento);
}*/

void manejar_consola_memoria(char* linea, void* request) {
	if (get_msg_id(request) == EXIT_REQUEST_ID) {
		finalizar_memoria();
	}
	//llamar a lissandra(request)
	//manejar error (si tira error)
	destroy(request);
}

void* correr_servidor_memoria(void* entrada) {
	lissandra_thread_t *l_thread = (lissandra_thread_t*) entrada;
	char puerto[6];
	sprintf(puerto, "%d", get_puerto_escucha_mem());

	int servidor = create_socket_server(puerto, 10);
	if (servidor < 0) {
		memoria_log_to_level(LOG_LEVEL_TRACE, false, "Fallo al crear el servidor");
		pthread_exit(NULL);
	}

	iniciar_consola(&manejar_consola);

	fd_set descriptores, copia;
	FD_ZERO(&descriptores);
	FD_SET(servidor, &descriptores);
	FD_SET(STDIN_FILENO, &descriptores);

	int select_ret;
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 250000000;
	//t_list* hilos_clientes = list_create();

	while (!l_thread_debe_finalizar(l_thread)) {
		copia = descriptores;
		select_ret = pselect(servidor + 1, &copia, NULL, NULL, &ts, NULL);
		if (select_ret == -1) {
			memoria_log_to_level(LOG_LEVEL_TRACE, false,
					"Fallo en la ejecucion del select del servidor");
			break;
		} else if (select_ret > 0) {
			if (FD_ISSET(servidor, &copia)) {
				aceptar_cliente(servidor);
				//crear_hilo_cliente(hilos_clientes, servidor);
			}
			if (FD_ISSET(STDIN_FILENO, &copia)) {
				//leer_siguiente_caracter();
			}
		}
		//list_remove_and_destroy_by_condition(hilos_clientes, &termino_cliente, &finalizar_hilo);
	}
	//list_destroy_and_destroy_elements(hilos_clientes, &finalizar_hilo);
	cerrar_consola();
	close(servidor);
	l_thread_indicar_finalizacion(l_thread);
	pthread_exit(NULL);
}















/*

#define BACKLOG 10

pthread_t server_thread;
struct server_input input;
start_server(&server_thread, &input);

int on_new_client(int socket, void* cantidad_de_handlers_ejecutados) {
  printf("Nuevo cliente: %d\n", socket);

  // Modificamos el estado compartido, puede hacerse de forma segura directamente porque
  // el servidor bloquea el mutex antes de ejecutar el handler
  *((int*) cantidad_de_handlers_ejecutados) = *((int*) cantidad_de_handlers_ejecutados) + 1;
  return 0;
}

int on_can_read(int socket, void* cantidad_de_handlers_ejecutados) {
  uint8_t buf[100];
  int num_bytes;

  *((int*) cantidad_de_handlers_ejecutados) = *((int*) cantidad_de_handlers_ejecutados) + 1;

  if((num_bytes = recv(socket, buf, 100 - 1, 0)) == -1) {
    printf("Error recv. Errno: %d\n", errno);
    return -1;
  }

  if(num_bytes == 0) {
    // El cliente cerró la conexión
    return CLOSE_CLIENT;
  }

  // Usar los datos recibidos

  return 0;
}





  int PORT = get_puerto_escucha_mem();
  int server_fd;
  pthread_t server_thread;
  int cantidad_de_handlers_ejecutados = 0;
  struct handler_set handlers;
  struct server_input input;

  if((server_fd = create_socket_server(PORT, BACKLOG)) == -1) {
    printf("Socket error\n");
    return -1;
  }

  handlers.on_new_client = &on_new_client;
  handlers.on_can_read = &on_can_read;

  init_server_input(&input, server_fd, handlers, &cantidad_de_handlers_ejecutados);

  start_server(&server_thread, &input);

  sleep(10);

  stop_server_and_join(server_thread, &input);

  printf("Cantidad de handlers ejecutados mientras corría el servidor: %d\n", cantidad_de_handlers_ejecutados);

  close(server_fd);

  return 0;
}
*/
