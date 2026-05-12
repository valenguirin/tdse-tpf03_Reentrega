/*
  Se presenta la información pública del cyclic executive de la aplicación
  main.c solo incluye este .h y llama a dos funciones:
  app_init()
  app_update()
  (ver app.c)
  Todo el orquestado de sensores, sistema y actuadores vive en el .c.
 */
#ifndef APP_H
#define APP_H

void app_init(void);   // se llama una vez al arrancar
void app_update(void); // se llama cada iteración del while(1) en main.c

#endif
