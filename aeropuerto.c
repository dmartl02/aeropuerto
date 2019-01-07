//PRACTICA FINAL DE SISTEMAS OPERATIVOS

//LIBRERIAS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

//CONSTANTES
#define USUARIOS 10
#define PUESTOS 2

//VARIABLES GLOBALES
pthread_mutex_t entradaFacturacion ; //Accesos a punteroUsuarios
pthread_mutex_t semaforoLog;  //Accesos al fichero de log
pthread_mutex_t semaforoCola;  //Accesos a la cola
pthread_mutex_t entradaSeguridad; //Accesos al control de seguridad
int usuariosFacturacion;
int usuariosAtentidosPuestos[PUESTOS]; //Pasajeros atendidos por cada puesto
int idUsuarioEsperando[100]; //Usuarios sin atender
int colaPuestos[USUARIOS]; //Pasajeros en una cola unica
int usuariosTotal; 
int finalizaPrograma;
int puertaSeguridad;
FILE *logFile;
char *logFileName = "registroTiempos.log";


//FUNCIONES
void nuevoUsuario(int sig);
void *AccionesUsuario(void *arg);
void *AccionesFacturador(void *arg);
void *AccionesAgenteSeguridad (void *arg);
void *ganasDeIrAlBanho(void *arg);
void finalizarPrograma(int sig);
void writeLogMessage(char *id,char *msg);
int generarAleatorio(int min, int max);


struct usuario {
	pthread_t usuarioHilo;
	pthread_t banhoHilo; //Hilo que comprueba si tiene ganas de ir al baño.
	int id;  //Va de 1 a USUARIOS
	int atendidoFacturacion;
	int atendidoSeguridad;
	int ha_Facturado;
	int tipo; //1 para normal, 2 para vip
	int esperando_Seguridad;
	int ganasDeIrAlBanho;
	int puesto_Asignado;
	int puestoCola;

};

struct usuario *punteroUsuarios;

int main() {
	char id[10];  
	char msg[100]; 
	pthread_t puesto1, puesto2, puestoSeguridad;
	int idPuesto1, idPuesto2, idPuestoSeguridad;

	//int usuariosAtendiendo = 0;
	int usuariosTotal = 0;

	if(signal(SIGUSR1, nuevoUsuario) == SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	if(signal(SIGUSR2, nuevoUsuario) == SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	if(signal(SIGINT, finalizarPrograma) == SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	srand(time(NULL));

	//Recursos
	if (pthread_mutex_init(&entradaFacturacion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&entradaSeguridad, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoLog, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoCola, NULL) != 0) exit(-1);
	
	idPuesto1 = 1;
	idPuesto2 = 2;
	usuariosFacturacion = 0;  //Va de 0 a USUARIOS-1
	puertaSeguridad = 0;

	for(int i= 0; i<100; i++){
		idUsuarioEsperando[i] = 0;
	}

	int i;
	for (i = 0; i < PUESTOS; i++){
		usuariosAtentidosPuestos[i] = 0;
	}

	for (i = 0; i < USUARIOS; i++){  //Posiciones libres
		colaPuestos[i] = 0;
	}


	//Reserva de memoria para el puntero de los usuarios
	punteroUsuarios = (struct usuario*)malloc(USUARIOS*sizeof(struct usuario));

	//Hilos para los dos puestos de facturación y el puesto de seguridad
	pthread_create(&puesto1, NULL, AccionesFacturador, (void *)&idPuesto1);
	pthread_create(&puesto2, NULL, AccionesFacturador, (void *)&idPuesto2);
	pthread_create(&puestoSeguridad, NULL, AccionesAgenteSeguridad, (void *)&idPuestoSeguridad);

	//Se crea el archivo log si no existe
	//Si existe, se sobreescribe
	logFile = fopen(logFileName, "w");
	if(logFile == NULL){
		char *err = strerror(errno);
		printf("%s", err);
		fclose(logFile);
	}

	writeLogMessage("Bienvenido","Aeropuerto Carles Puigdemont");

	//Espera hasta recibir señal
	//La señal SIGINT finaliza el programa
	while(finalizaPrograma != 1){
		pause();
	}

	return 0;
}


void nuevoUsuario (int sig){

	if(signal(SIGUSR1,nuevoUsuario)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoUsuario)==SIG_ERR){
		exit(-1);
	}
	
	
	
	pthread_mutex_lock(&entradaFacturacion);
	if (usuariosFacturacion < USUARIOS){

		//Iniciar variables
		punteroUsuarios[usuariosTotal].id = usuariosTotal+1;
		idUsuarioEsperando[usuariosTotal] = 1;
		punteroUsuarios[usuariosTotal].atendidoFacturacion = 0;
		punteroUsuarios[usuariosTotal].atendidoSeguridad = 0;
		punteroUsuarios[usuariosTotal].ganasDeIrAlBanho = 0;
		punteroUsuarios[usuariosTotal].esperando_Seguridad = 0;
		punteroUsuarios[usuariosTotal].puestoCola = 0;

		if (sig == 10) {  //SIGUSR1
			punteroUsuarios[usuariosTotal].tipo = 1;
		} 
		if (sig == 12) {  //SIGUSR2
			punteroUsuarios[usuariosTotal].tipo = 2;
		}

		//Creamos el hilo de usuario
		pthread_create(&punteroUsuarios[usuariosTotal].usuarioHilo, NULL, AccionesUsuario, (void *)&punteroUsuarios[usuariosTotal].id);
		//Creamos un hilo para comprobar cada 3 segundos si se tiene ganas de ir al baño
		pthread_create(&punteroUsuarios[usuariosTotal].banhoHilo, NULL, ganasDeIrAlBanho, (void *)&punteroUsuarios[usuariosTotal].id);

		usuariosTotal++;
		usuariosFacturacion++;
	}

	pthread_mutex_unlock(&entradaFacturacion);

}


//Metodo de las acciones del usuario
void *AccionesUsuario(void *arg){

	char id[10];
	char msg[100];
    int idUsuario = *(int *)arg;  //Numero del usuario (de 1 a USUARIOS)

    pthread_mutex_lock(&semaforoLog);
	sprintf(id, "Usuario_%d", idUsuario);
	sprintf(msg, "Ha llegado al aeropuerto");
	writeLogMessage(id, msg);
    pthread_mutex_unlock(&semaforoLog);

    sleep(2);

    //Se le asigna al usuario su puesto en la cola
    pthread_mutex_lock(&semaforoCola);
    int puesto_Asignado = 0;
    int contador = 0;

    while((puesto_Asignado == 0) && (contador<USUARIOS)){
    	if(colaPuestos[contador] == 0){
			colaPuestos[idUsuario-1] = idUsuario;
			puesto_Asignado = 1;
			punteroUsuarios[idUsuario-1].puestoCola = contador+1;
		}
		contador++;
	}
	pthread_mutex_unlock(&semaforoCola);
	
	//Qué hace el usuario mientras espera
	while(punteroUsuarios[idUsuario-1].atendidoFacturacion == 0){
	    int estadoAleatorio = generarAleatorio(1, 100);
	    
	    //Faltan los 3 segundos 
	    if(estadoAleatorio<=20){ //Comprobar si se cansa de esperar
	        pthread_mutex_lock(&semaforoLog);
	        sprintf(id, "Usuario_%d", idUsuario);
			sprintf(msg, "Se cansa de esperar y se va del aeropuerto");
			writeLogMessage(id, msg);
	        pthread_mutex_unlock(&semaforoLog);
          
          	//Se libera espacio en la cola
          	idUsuarioEsperando[idUsuario-1] = 0;
          	
          	pthread_mutex_lock(&semaforoCola);
          	for(int i = punteroUsuarios[idUsuario-1].puestoCola; i<usuariosFacturacion; i++){
          		colaPuestos[i] = colaPuestos[i+1];
          		punteroUsuarios[colaPuestos[i]].puestoCola--;
          	}

          	usuariosFacturacion--;
          	pthread_mutex_unlock(&semaforoCola);

          	pthread_cancel(punteroUsuarios[idUsuario-1].banhoHilo);
	        pthread_cancel(punteroUsuarios[idUsuario-1].usuarioHilo);

	    } else if(punteroUsuarios[idUsuario-1].ganasDeIrAlBanho == 1){ //Comprobar si va al baño
	        pthread_mutex_lock(&semaforoLog);
	        sprintf(id, "Usuario_%d", idUsuario);
			sprintf(msg, "Va al baño");
			writeLogMessage(id, msg);
	        pthread_mutex_unlock(&semaforoLog);
	        
	        //Se libera espacio en la cola
	        idUsuarioEsperando[idUsuario-1] = 0;

	        pthread_mutex_lock(&semaforoCola);
          	for(int i = punteroUsuarios[idUsuario-1].puestoCola; i<usuariosFacturacion; i++){
          		colaPuestos[i] = colaPuestos[i+1];
          		punteroUsuarios[colaPuestos[i]].puestoCola--;
          	}
          	usuariosFacturacion--;
          	pthread_mutex_unlock(&semaforoCola);

	        pthread_cancel(punteroUsuarios[idUsuario-1].usuarioHilo);

	    } else{
	        sleep(4);
	    }
	}

	pthread_cancel(punteroUsuarios[idUsuario-1].banhoHilo);

	//Esperando a terminar de ser atendido...
	while(punteroUsuarios[idUsuario-1].atendidoFacturacion == 1) {  
		sleep(1);
	}

	//Comprobar si ha facturado ya o no
	if(punteroUsuarios[idUsuario-1].ha_Facturado == 1) { //Si ha facturado

       	pthread_mutex_lock(&semaforoCola);
      	for(int i = punteroUsuarios[idUsuario-1].puestoCola; i<usuariosFacturacion; i++){
	  		colaPuestos[i] = colaPuestos[i+1];
	  		punteroUsuarios[colaPuestos[i]].puestoCola--;
        }
      	usuariosFacturacion--;
      	pthread_mutex_unlock(&semaforoCola);

		pthread_mutex_lock(&entradaSeguridad);
		puertaSeguridad = idUsuario;

		while(punteroUsuarios[idUsuario-1].atendidoSeguridad == 0){
			sleep(1);
		}
		pthread_mutex_unlock(&entradaSeguridad);

      
    	pthread_mutex_lock(&semaforoLog);
    	sprintf(id, "Usuario_%d", idUsuario);
		sprintf(msg, "Deja el control de seguridad");
		writeLogMessage(id, msg);
    	pthread_mutex_unlock(&semaforoLog);
       
       	printf("Usuario %d va a embarcar\n", idUsuario);

        pthread_mutex_lock(&semaforoLog);
    	sprintf(id, "Usuario_%d", idUsuario);
		sprintf(msg, "Va a embarcar");
		writeLogMessage(id, msg);
    	pthread_mutex_unlock(&semaforoLog);
	
	} else if(punteroUsuarios[idUsuario-1].ha_Facturado == 0){ //No ha facturado
	    pthread_mutex_lock(&semaforoLog);
    	sprintf(id, "Usuario_%d", idUsuario);
		sprintf(msg, "Se va porque no ha facturado");
		writeLogMessage(id, msg);
    	pthread_mutex_unlock(&semaforoLog);

    	idUsuarioEsperando[idUsuario-1] = 0;
    	usuariosFacturacion--;
    	pthread_cancel(punteroUsuarios[idUsuario-1].usuarioHilo);

	}
}


void *AccionesFacturador(void *arg){

	char id[10];
	char msg[100];
    int idPuesto = *(int *)arg;
    printf("id: %d\n", idPuesto);
    int eventoAleatorio, duermeAleatorio;
    int i = 0, j = 0;


    while(1){
		pthread_mutex_lock(&semaforoCola);
		
		j = 0;

		//Comprueba los usuarios que tiene que atender (normales o vips)
		while(j < USUARIOS && (colaPuestos[j] == 0 || colaPuestos[j] != 0 && punteroUsuarios[colaPuestos[j]-1].atendidoFacturacion == 1 || colaPuestos[j] != 0 && punteroUsuarios[colaPuestos[j]-1].tipo != idPuesto)) {
			j++;
		}

		if((idPuesto == 2) && (j >= USUARIOS)){
			if (colaPuestos[j] == 0 || punteroUsuarios[colaPuestos[j]-1].ha_Facturado!=0 || colaPuestos[j] != 0 && punteroUsuarios[colaPuestos[j]-1].tipo != idPuesto) { //No hay usuarios en esta cola y buscamos en otra
				//No hay VIPS, coge un usuario de cualquier cola
				j = 0;
				while(j < USUARIOS && (colaPuestos[j] == 0 || colaPuestos[j] != 0 && (punteroUsuarios[colaPuestos[j]-1].ha_Facturado!=0 || punteroUsuarios[colaPuestos[j]-1].atendidoFacturacion!=0))) {
					j++;
				}
				if (colaPuestos[j] == 0 || punteroUsuarios[colaPuestos[j]-1].ha_Facturado!=0 || punteroUsuarios[colaPuestos[j]-1].atendidoFacturacion!=0) { //No hay usuarios esperando
					j = -1;
				}
			}
		} else if (j>=USUARIOS){
			j = -1;
		}

		pthread_mutex_unlock(&semaforoCola);

		if(j == -1){
			//Esperamos por nuevos usuarios
			
			sleep(1);
		} else {
			
			pthread_mutex_lock(&semaforoCola);
			i = colaPuestos[j];			
			pthread_mutex_unlock(&semaforoCola);

			punteroUsuarios[i-1].atendidoFacturacion = 1;
			punteroUsuarios[i-1].puesto_Asignado = idPuesto;

			//Calculamos comportamientos
			eventoAleatorio = generarAleatorio(1, 100);

			//Facturación
			printf("idPuesto: %d\n",idPuesto);
			pthread_mutex_lock(&semaforoLog);
			sprintf(id, "Puesto%d", idPuesto);
			sprintf(msg, "Usuario_%d facturando", i);
			writeLogMessage(id, msg);
			pthread_mutex_unlock(&semaforoLog);

			if (eventoAleatorio <= 80) {  //Facturacion correcta
				duermeAleatorio = generarAleatorio(1, 4);
				sleep(duermeAleatorio);
				punteroUsuarios[i].ha_Facturado = 1;

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Usuario_%d", i);
				sprintf(msg, "Ha facturado, todo correcto, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else if (eventoAleatorio <= 90 ){  //Exceso de peso
				duermeAleatorio = generarAleatorio(2, 6);
				sleep(duermeAleatorio);
				punteroUsuarios[i].ha_Facturado = 1;

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Usuario_%d", i);
				sprintf(msg, "Ha facturado pero tiene un exceso de peso, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else {  //Visado no esta en regla
				duermeAleatorio = generarAleatorio(6, 10);
				sleep(duermeAleatorio);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Usuario_%d", i);
				sprintf(msg, "No tiene el visado en regla y no puede facturar, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);
			}

			usuariosAtentidosPuestos[idPuesto-1]++; //Usuarios en los puestos

			//Descanso de los facturadores
			if (usuariosAtentidosPuestos[idPuesto-1] % 5 == 0) {

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Facturador_%d", idPuesto);
				sprintf(msg, "Inicio del descanso. Va a tomarse un café");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

				sleep(10);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Facturador_%d", idPuesto);
				sprintf(msg, "Fin del descanso. Vuelve al trabajo.");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);
			}

		}

	}
}





void *AccionesAgenteSeguridad (void *arg){

    char id[10];
	char msg[100];
	int numUsuarios = *(int *)arg;
	int i = 0;
	int atendidoControl = 0;
	int eventoControlAleatorio, duermeControlAleatorio;

	while(puertaSeguridad != 0){ //Quedarse esperando a que haya alguien
	    
	    i = puertaSeguridad-1;
	
		//pthread_mutex_lock(&entradaSeguridad);    
	    
    	pthread_mutex_lock(&semaforoLog);
   		sprintf(id, "Usuario_%d", punteroUsuarios[i].id);
   		sprintf(msg, "Entra al control de seguridad");
   		writeLogMessage(id, msg);
   		pthread_mutex_unlock(&semaforoLog);

       	eventoControlAleatorio = generarAleatorio(1,100);

       	if(eventoControlAleatorio <= 60){ //Pasa el control sin problemas
       		duermeControlAleatorio = generarAleatorio(2,3);
			sleep(duermeControlAleatorio);
			
			pthread_mutex_lock(&semaforoLog);
			sprintf(id, "Usuario_%d", punteroUsuarios[i].id);
			sprintf(msg, "Pasa el control de seguridad, tiempo = %d", duermeControlAleatorio);
			writeLogMessage(id, msg);
			pthread_mutex_unlock(&semaforoLog);

		} else { //Es inspeccionado en el control
			duermeControlAleatorio = generarAleatorio(10,15);
			sleep(duermeControlAleatorio);
	
			pthread_mutex_lock(&semaforoLog);
			sprintf(id, "Usuario_%d", punteroUsuarios[i].id);
			sprintf(msg, "Es inspeccionado en el control, tiempo = %d", duermeControlAleatorio);
			writeLogMessage(id, msg);
			pthread_mutex_unlock(&semaforoLog);
		}

		puertaSeguridad = 0;
    

    	pthread_mutex_lock(&semaforoLog);
		printf(id, "Usuario_%d", punteroUsuarios[i].id);
   		sprintf(msg, "Abandona el control de seguridad y va a embarcar.");
   		writeLogMessage(id, msg);
   		pthread_mutex_unlock(&semaforoLog);

   		punteroUsuarios[i].atendidoSeguridad = 1;
	   
		//pthread_mutex_unlock(&entradaSeguridad); 
	}		
}


void *ganasDeIrAlBanho(void *arg){
	
	int eventoBanho;
	int idUsuario = *(int *)arg;

	while(1){
		sleep(3);
		eventoBanho = generarAleatorio(1,100);
		if(eventoBanho <= 10){
			punteroUsuarios[idUsuario-1].ganasDeIrAlBanho = 1;
			
			pthread_cancel(punteroUsuarios[idUsuario-1].banhoHilo);
		}
	}
}



void finalizarPrograma(int sig){
	char id[10];
	char msg[100];

	pthread_mutex_lock(&semaforoLog);

	for(int i=0; i<usuariosTotal; i++){
		if (idUsuarioEsperando[i] != 0) { //Comprobar si queda algún usuario
			sprintf(id, "Usuario_%d", idUsuarioEsperando[i]);
			sprintf(msg, "El aeropuerto va a cerrar. Me voy a casa.");
			writeLogMessage(id, msg);
			
			pthread_cancel(punteroUsuarios[i].usuarioHilo);
		}
	}

		sprintf(msg, "Han sido atendidos %d usuarios por puesto1 y %d usuarios por puesto2", usuariosAtentidosPuestos[0], usuariosAtentidosPuestos[1]);
		writeLogMessage("Información", msg);

		writeLogMessage("Final del programa", "El aeropuerto ha cerrado");
		pthread_mutex_unlock(&semaforoLog);

		finalizaPrograma = 1;

}



//Metodo para escribir en el log
void writeLogMessage(char *id, char *msg){

	//Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	
	//Escribimos en el log
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	//Para seguir el logfile por pantalla
	printf("###logFile:[%s] %s: %s\n", stnow, id, msg); 
	fclose(logFile);	
}

//Generación de números aleatorios
int generarAleatorio(int min, int max){
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}
