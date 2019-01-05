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
int contadorUsuarios;
int embarca;
int usuariosAtentidosPuestos[PUESTOS]; //Pasajeros atendidos por cada puesto
int idUsuarioEsperando; //Usuarios sin atender
int colaPuestos[USUARIOS]; //Pasajeros en una cola unica
int primerPuestoCola; //De 0 a USUARIOS-1
//int usuariosAtendiendo;
int usuariosTotal;
int finalizaPrograma;
FILE *logFile;
char *logFileName = "registroTiempos.log";


//FUNCIONES
void nuevoUsuarioNormal(int sig);
void nuevoUsuarioVip(int sig);
void iniciaVariablesUsuario(int posicionPuntero, int id, int puesto, int normal); //Hay normales y vip
void *AccionesUsuario(void *arg);
void *AccionesFacturador(void *arg);
void *AccionesAgenteSeguridad (void *arg);
void usuariosActuales(int sig);
void embarcar(int sig);
void writeLogMessage(char *id,char *msg);
int numeroAleatorio(int min, int max);


struct usuario {
	pthread_t usuarioHilo;
	//pthread_t ganasDeIrAlBanho; hilo que comprueba si tiene ganas de ir al baño.
	int id;  //Va de 1 a USUARIOS
	int atendidoFacturacion;
	int atendidoSeguridad;
	int ha_Facturado;
	int tipo; //1 para normal, 2 para vip
	int esperando_Seguridad;
};

struct usuario *punteroUsuarios;

int main() {
	char id[10];  
	char msg[100]; 
	pthread_t puesto1, puesto2, puestoSeguridad;
	int idPuesto1, idPuesto2, idPuestoSeguridad;

	//int usuariosAtendiendo = 0;
	int usuariosTotal = 0;

	if(signal(SIGUSR1, nuevoUsuarioNormal) == SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	if(signal(SIGUSR2, nuevoUsuarioVip) == SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	if(signal(SIGINT, embarcar) == SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}



	//Recursos
	if (pthread_mutex_init(&entradaFacturacion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&entradaSeguridad, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoLog, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoCola, NULL) != 0) exit(-1);
	

	idPuesto1 = 1;
	idPuesto2 = 2;
	contadorUsuarios = 0;  //Va de 0 a USUARIOS-1
	embarcar = 0;
	primerPuestoCola = 0;
	//Igual hay que inicializar los que están esperando por seguridad


	int i;
	for (i = 0; i < PUESTOS; i++){
		usuariosAtentidosPuestos[i] = 0;
	}

	for (i = 0; i < USUARIOS; i++){  //Posiciones libres
		colaPuestos[i] = 0;
	}


	//Reserva de memoria para el puntero de los usuarios
	punteroUsuarios = (struct usuario*)malloc(USUARIOS*sizeof(struct usuario));

	//Hilos para los dos puestos de facturación
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

	writeLogMessage("Bienvenido","Aeropuerto Carles Puigdemon");

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
	
	usuariosTotal++;

	pthread_mutex_lock(&entradaFacturacion);
	if (contadorUsuarios < USUARIOS){

		//Iniciar variables
		contadorUsuarios[contadorUsuarios].id = contadorUsuarios+1;
		contadorUsuarios[contadorUsuarios].atendidoFacturacion = 0;
		contadorUsuarios[contadorUsuarios].atendidoSeguridad = 0;

		if (sig == 10) {  //SIGUSR1
			contadorUsuarios[contadorUsuarios].tipo = nuevoUsuarioNormal;
		} 
		if (sig == 12) {  //SIGUSR2
			contadorUsuarios[contadorUsuarios].tipo = nuevoUsuarioVip;
		}

		//contadorUsuarios[contadorUsuarios].seguridad = 0;

		//Creamos el hilo de usuario
		pthread_create(&contadorUsuarios[contadorUsuarios].usuarioHilo, NULL, AccionesUsuario, (void *)&contadorUsuarios[contadorUsuarios].id);
		//¿Crear un hilo para comprobar cada 3 segundos si se tiene ganas de ir al baño?
		//pthread_create(&contadorUsuarios[contadorUsuarios].ganasDeIrAlBanho, NULL, GanasDeIrAlBanho, (void *)&contadorUsuarios[contadorUsuarios].id);

		contadorUsuarios++;
	}

	pthread_mutex_unlock(&entradaFacturacion);

}


//FALTAN COSAS
//Metodo de las acciones del usuario
void *AccionesUsuario(void *arg){
	char id[10];
	char msg[100];
    int idUsuario = *(int *)arg;  //Numero del usuario (de 1 a USUARIOS)

    pthread_mutex_lock(&semaforoLog);
	sprintf(id, "Usuario_%d", idUsuario);
	sprintf(msg, "Ha llegado al aeropuerto, factura en el puesto %d", punteroUsuarios[idUsuario-1-1].puesto_Asignado);
	writeLogMessage(id, msg);
    pthread_mutex_unlock(&semaforoLog);

    sleep(2);

    //Se le asigna al usuario su puesto en la cola
    pthread_mutex_lock(&semaforoCola);
	colaPuestos[idUsuario-1] = idUsuario;
	pthread_mutex_unlock(&semaforoCola);
	
	//Qué hace el usuario mientras espera
	while(punteroUsuarios[idUsuario-1].atendidoFacturacion == 0){
	    int estadoAleatorio = numeroAleatorio(1, 100);
	    
	    //Faltan los 3 segundos 
	    if(estadoAleatorio<=20){ //Comprobar si se cansa de esperar
	        pthread_mutex_lock(&semaforoLog);
	        sprintf(id, "Usuario_%d", idUsuario);
			sprintf(msg, "Se cansa de esperar y se va del aeropuerto");
			writeLogMessage(id, msg);
	        pthread_mutex_unlock(&semaforoLog);
          
          	//Se libera espacio en la cola
	        pthread_cancel(punteroUsuarios[idUsuario-1].usuarioHilo);

	    } else if(estadoAleatorio<=30){ //Comprobar si va al baño
	        pthread_mutex_lock(&semaforoLog);
	        sprintf(id, "Usuario_%d", idUsuario);
			sprintf(msg, "Va al baño");
			writeLogMessage(id, msg);
	        pthread_mutex_unlock(&semaforoLog);
	        
	        //Se libera espacio en la cola
	        pthread_cancel(punteroUsuarios[idUsuario-1].usuarioHilo);

	    } else{
	        sleep(4);
	    }
	}

	//Esperando a terminar de ser atendido...
	//COMPROBAR SI ESTA BIEN ESTO
	while(punteroUsuarios[idUsuario-1].atendidoFacturacion == 1) {  
		sleep(1);
	}

	//Comprobar si ha facturado ya o no
	if(punteroUsuarios[idUsuario-1].ha_Facturado == 1) { //Si ha facturado
    	pthread_mutex_lock(&semaforoCola); //TODO comrpobar que semaforo usar aqui
        contadorUsuarios--; //Liberamos la cola de facturacion
	primerPuestoCola++%USUARIOS; //Se incrementa el primer puesto de la cola
        while(punteroUsuarios[idUsuario-1].atendidoSeguridad == 1) {  
			sleep(1);
		}	
    	pthread_mutex_unlock(&semaforoCola); 
    
    //No sé cómo van estos dos logs
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

    	pthread_cancel(punteroUsuarios[idUsuario-1].usuarioHilo);
	}
}


void *AccionesFacturador(void *arg){
	char id[10];
	char msg[100];
    int idPuesto = *(int *)arg;
    int eventoAleatorio, duermeAleatorio;
    int i = 0, j = 0;

    while(1){
		pthread_mutex_lock(&semaforoCola);
		
		j = 0;

		//Comprueba los usuarios que tiene que atender (normales o vips)
		while(j < USUARIOS && (colaPuestos[(j+primerPuestoCola)%USUARIOS] == 0 || colaPuestos[(j+primerPuestoCola)%USUARIOS] != 0 && punteroUsuarios[colaPuestos[(j+primerPuestoCola)%USUARIOS]-1].atendidoFacturacion == 1 || colaPuestos[(j+primerPuestoCola)%USUARIOS] != 0 && punteroUsuarios[colaPuestos[(j+primerPuestoCola)%USUARIOS]-1].puesto_Asignado != idPuesto)) {
			j++;
		}

		if((idPuesto == 2) && (j >= USUARIOS){
			if (colaPuestos[(j+primerPuestoCola)%USUARIOS] == 0 || punteroUsuarios[colaPuestos[(j+primerPuestoCola)%USUARIOS]-1].ha_Facturado!=0 || colaPuestos[(j+primerPuestoCola)%USUARIOS] != 0 && punteroUsuarios[colaPuestos[(j+primerPuestoCola)%USUARIOS]-1].puesto_Asignado != idPuesto) { //no hay atletas esperando en nuestra cola, buscamos en otras
				//No hay VIPS, coge un usuario de cualquier cola
				j = 0;
				while(j < USUARIOS && (colaPuestos[(j+primerPuestoCola)%USUARIOS] == 0 || colaPuestos[(j+primerPuestoCola)%USUARIOS] != 0 && punteroUsuarios[colaPuestos[(j+primerPuestoCola)%USUARIOS]-1].ha_Facturado!=0)) {
					j++;
				}
				if (colaPuestos[(j+primerPuestoCola)%USUARIOS] == 0 || punteroUsuarios[colaPuestos[(j+primerPuestoCola)%USUARIOS]-1].ha_Facturado!=0) { //No hay usuarios esperando
					j = -1;
				}
			}
		} else {
			j = -1
		} 

		if(j == -1){
			//Esperamos por nuevos usuarios
			pthread_mutex_unlock(&semaforoCola);
			sleep(1);
		} else {
			i = colaPuestos[(j+primerPuestoCola)%USUARIOS]-1;

			punteroUsuarios[i].atendidoFacturacion = 1;
			pthread_mutex_unlock(&semaforoCola);

			//Calculamos comportamientos
			eventoAleatorio = generarAleatorio(1, 100);

			//Facturación
			pthread_mutex_lock(&semaforoLog);
			sprintf(id, "Puesto%d", idPuesto);
			sprintf(msg, "Usuario_%d facturando", colaPuestos[i]);
			writeLogMessage(id, msg);
			pthread_mutex_unlock(&semaforoLog);

			if (eventoAleatorio <= 80) {  //Facturacion correcta
				duermeAleatorio = generarAleatorio(1, 4);
				sleep(duermeAleatorio);
				punteroUsuarios[i].ha_Facturado = 1;

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Usuario_%d", colaPuestos[i]);
				sprintf(msg, "Ha facturado, todo correcto, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else if (eventoAleatorio <= 90 ){  //Exceso de peso
				duermeAleatorio = generarAleatorio(2, 6);
				sleep(duermeAleatorio);
				punteroUsuarios[i].ha_Facturado = 1;

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Usuario_%d", colaPuestos[i]);
				sprintf(msg, "Ha facturado pero tiene un exceso de peso, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else {  //Visado no esta en regla
				duermeAleatorio = generarAleatorio(6, 10);
				sleep(duermeAleatorio);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Usuario_%d", colaPuestos[i]);
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




//FALTAN COSAS
void *AccionesAgenteSeguridad (void *arg){
    char id[10];
	char msg[100];
	int numUsuarios = *(int *)arg;
	i = 0;
	int atendidoControl = 0;
	int eventoControlAleatorio, duermeControlAleatorio;

	while(i<numUsuarios && seHaAtendidoAAlguien!=1) { //TODO QUEDARSE ESPERANDO A QUE HAYA ALGUIEN
	    if(punteroUsuarios[i].esperando_Seguridad){
		
		pthread_mutex_lock(&entradaSeguridad);    
		    
	    	pthread_mutex_lock(&semaforoLog);
       		sprintf(id, "Usuario_%d", punteroUsuarios[i].id);
       		sprintf(msg, "Entra al control de seguridad");
       		writeLogMessage(id, msg);
       		pthread_mutex_unlock(&semaforoLog);

	       	eventoControlAleatorio = generarAleatorio(1,100);

	       	if(eventoControlAleatorio <= 60){ //Pasa el control sin problemas
	       		duermeControlAleatorio = generarAleatorio(2,3);
				sleep(duermeControlAleatorio);
				punteroUsuarios[i].atendidoControl = 1;

				pthread_mutex_lock(&semaforoLog);
				//Aqui habria que usar ya la cola de la seguridad sprintf(id, "Usuario_%d", cola[i]);
				sprintf(msg, "Pasa el control de seguridad, tiempo = %d", duermeControlAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else { //Es inspeccionado en el control
				duermeControlAleatorio = generarAleatorio(10,15);
				sleep(duermeControlAleatorio);
				punteroUsuarios[i].atendidoControl = 1;

				pthread_mutex_lock(&semaforoLog);
				//Aqui habria que usar ya la cola de la seguridad 
				sprintf(id, "Usuario_%d", cola[i]);
				sprintf(msg, "Es inspeccionado en el control, tiempo = %d", duermeControlAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);
			}
	    

	    	pthread_mutex_lock(&entradaSeguridad);
			printf(id, "Usuario_%d", punteroUsuarios[i].id);
       		sprintf(msg, "Abandona el control de seguridad");
       		writeLogMessage(id, msg);
       		pthread_mutex_unlock(&semaforoLog);
		   
		pthread_mutex_unlock(&entradaSeguridad); 

       		}
	}		
}



void finalizaPrograma(int sig) {
	char id[10];
	char msg[100];

	pthread_mutex_lock(&semaforoLog);

	if (idUsuarioEsperando != 0) { //Comprobar si queda algún usuario
		sprintf(id, "Usuario_%d", idUsuarioEsperando);
		sprintf(msg, "El aeropuerto va a cerrar. Me voy a casa.");
		writeLogMessage(id, msg);
		
		pthread_cancel(punteroUsuarios[idUsuarioEsperando-1].usuarioHilo);
	}

	sprintf(msg, "Han sido atendidos %d usuarios por tarima1 y %d usuarios por tarima2", usuariosAtentidosPuestos[0], usuariosAtentidosPuestos[1]);
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
int numeroAleatorio(int min, int max){
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}
