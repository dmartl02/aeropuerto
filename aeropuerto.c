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
int usuariosAtendiendo;
int usuariosActualesTotal;
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
int generarAleatorio(int min, int max);


struct usuario {
	pthread_t usuarioHilo;
	int id;  //Va de 1 a USUARIOS
	int ha_Facturado;
	int puesto_Asignado;
	//int esperando_Seguridad (puede ser necesario)
};

struct usuario *punteroUsuarios;

int main() {
	char id[10];  
	char msg[100]; 
	pthread_t puesto1, puesto2, puestoSeguridad;
	int idPuesto1, idPuesto2, idPuestoSeguridad;

	int usuariosAtendiendo = 0;
	int usuariosActualesTotal = 0;

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





void finalizaPrograma(int sig) {
	char id[10];
	char msg[100];

	pthread_mutex_lock(&semaforoLog);

	if (idUsuarioEsperando != 0) { //Comprobar si queda algún usuario
		sprintf(id, "usuario%d", idUsuarioEsperando);
		sprintf(msg, "El aeropuerto va a cerrar. Me voy a casa.");
		writeLogMessage(id, msg);
		
		pthread_cancel(punteroUsuarios[idUsuarioEsperando-1].usuarioHilo);
	}

	sprintf(msg, "Han sido atendidos %d usuarios por tarima1 y %d usuarios por tarima2", usuariosAtentidosPuestos[0], usuariosAtentidosPuestos[1]);
	writeLogMessage("Información", msg);

	writeLogMessage("Final del programa", "El aeropuertoha cerrado");
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