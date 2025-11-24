# Practica-3-Controlador-M-quina-Expendedora

---

## Autor: Yassir El Kasmi
## Fecha de entrega: 27/11/2025

---

### Objetivo
Diseñar y programar un sistema completo, basado en Arduino, que controle el funcionamiento de una máquina expendedora de café, utilizando tanto sensores (ultrasonido, DHT...) como actuadores (LEDS, LCD...).
Además de utilizar las técnicas y librerias vistas en clase, como las interrupciones o ArduinoThread.
Aprendiendo así diversas cosas: 
* Controlar procesos multioperativos con un solo monocore
* Usar sensores reales (distancia, temperatura y humedad)
* Mostrar información dinámica en pantalla LCD
* Implementar una máquina de estados robusta
* Usar interrupciones y medir la duración de pulsaciones
* Diseñar un menú navegable con joystick
* Implementar un flujo completo: detección → selección → preparación → entrega
* Crear un modo ADMIN protegido mediante pulsación larga
* Modificar parámetros internos (precios) sin reiniciar la máquina

### Planteamiento, Diseño, Implementación e Inconvenientes

Proceso completo en orden cronológico:
1. Plantee la disttribucción de los pines para cada componente, cuales necesitaban pines analogicos (como las direcciones del joystick), cuales necesitaban pines con PWM (como el LED2, el azul, para simular un comprotamiento analógico en un pin digital), y cuales necesitaban un pin que permitan interrupciones (los botones).

2. Intente montarlo todo junto y no probarlo ni hacer nada de codigo hasta que lo tuviese montado completo queriendo así hacer primero la parte de hardware y después hacer el software, lo cual fue un error ya que a la hora de ponerme a hacer el codigo fallaban unas pruebas muy sencillas fallaban y no podia averiguar el problema, asi que lo desmonté entero y fui componente por componente montado y probando.

3. Después de montarlo completo habiendo comprobado el correcto funcionamiento de todos los componentes, tenia que plantear el primer estado, el estado ARRANQUE. Donde como queria que hiciese dos acciones a la vez (parpadear el LED1 y mostrar mensaje en el LCD), por lo que como no se puede hacer multitasking en un sistema monocore, tanto esta como todas las peticiones de la práctica donde pidiesen algo similar usamos la libreria ArduinoThread para simular tareas que estan ocurriendo al mismo tiempo.

4.    
