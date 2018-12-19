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
#define PASAJEROS 10
#define PUESTOS 2

//VARIABLES GLOBALES
pthread_mutex_t semaforoLog;  //accesos al fichero de log
pthread_mutex_t semaforoCola;  //accesos a la cola
int contadorPasajeros;
FILE *logFile;
char *logFileName ="registroTiempos.log";


















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