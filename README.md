# Practica-3-Controlador-Máquina-Expendedora

---

## Autor: Yassir El Kasmi
## Fecha de entrega: 27/11/2025
© 2025 *Yassir El Kasmi El Kasmi*

Algunos derechos reservados. Este trabajo se entrega bajo la licencia **CC BY-SA 4.0**

---
### Hardware
Arduino UNO: La placa principal de control.

LCD: Pantalla para mostrar información al usuario.

Joystick: Módulo de entrada para la navegación y selección.

DHT11: Sensor de temperatura y humedad.

HC-SR04: Sensor ultrasónico para medir la distancia.

LEDs: Indicadores visuales de estado.

Botón: Interruptor para controlar el sistema.

Resistencias: Para limitar la corriente y proteger los componentes.

Protoboard: Para montar el circuito de prueba.

Cables de conexión: Para realizar las conexiones entre los componentes.

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

4. Despues implemento todos los estados de SERVICIO y comprabando su funcionamiento.

5. Y por último, todos los estados de ADMIN y comprobando su funcionamiento.

6. Además utilizar un watchdog de 2 segundos.

### Inconvenientes físicos:
- En mi kit no venia placa protoboard ni potenciometro, después de pedir la placa protoboard, me dieron una placa defectuosa (donde costaba mucho esfuerzo conectar cualquier componente, habia que hacer una cantidad de fuerza totalmente exagerada) y tuve que pedir una segunda placa. Además de un joystick defectuoso.
- En la prueba del LCD, la resistencia que va conectada al pin A del LCD, la introduje demasiado en el rail positivo de la placa lo que provocaba que cruzase al rail negativo y se generase un cortocircutio.
- El joysick tiene cambiados los ejes cuando lo conectas diractementa a la placa asi que lo conecta con cables y uso el joystick girado 90º en sentido horario con respecto a conectarlo directamente.
- El boton lo conectaba mal, conectandolo como si no implenetase INPUT_PULLUP, es decir, usaba tres cables (5v---->PIN---->BOTON---->GND), en vez de dos que son los que hacen falta ( PIN--->BOTON--->GND).
- El sensor de ultrasonido a veces daba numeros negativos.
- El sensor DHT daba algunos errores.

### Incovenientes software:
(Para todos estos incovenientes use los recursos mencionados en el siguiente aparatado)
- No sabia como generar un número aleatorio de segundos para la preparación del café, y usar ese número para que el LED2 aumente su intensidad gradualmente.
- No tenia muy claro como hacer los estados y pasar de uno a otro.
- "Esperando cliente" no cabia en una fila del LCD, asi que lo separe en dos con setCursor();.
- No sabia como cambiar de un tipo de de datos a otro (int <---> float).
- A veces contaba mas de un pulsación en el boton del joystick, asi que le puse interrupciones

### Recursos utilizados:
* Librerias y apuntes vistos en clase.
* ChatGPT (Para resolver los inconvenientes y revisar la ortografia de esta memoria).
* Mi Github (utilizé un código para calcular **la distancia con el ultrasonido y el metodo de antirrebote** que hice este mismo año en la asignatura de Sensores y Actuadores).


### Modelo de fritzing
<img width="1541" height="891" alt="Captura desde 2025-11-26 22-19-23" src="https://github.com/user-attachments/assets/9acf263b-5699-4415-9f96-519ad8002b55" />
