#include <LiquidCrystal.h>
#include "DHT.h"
#include <Thread.h>
#include <ThreadController.h>
#include <avr/wdt.h>


#define DHTTYPE DHT11

// PINES
const byte PIN_DHT      = A4;
const byte PIN_TRIGGER  = 7;   // Trigger sensor ultrasonidos
const byte PIN_ECHO     = 8;   // Echo sensor ultrasonidos
const byte JOY_BOTON    = 2;   // Botón joystick
const byte VRx          = A1;  // Eje X joystick (izq/der)
const byte VRy          = A0;  // Eje Y joystick (arriba/abajo)
const byte PIN_LED1     = 9;   // LED1 (arranque / admin)
const byte PIN_LED2     = 6;   // LED2 (progreso / admin)
const byte PIN_BOTON    = 3;   // Botón principal (INT1)

//  CONSTANTES 
const float V_SOUND_AIR = 0.0343f;  // Velocidad sonido en cm/us aprox

//  VARIABLES GLOBALES PARA SERVICIO 
float  distance           = 0.0f;
int    duration           = 0;      // Duración pulso ECHO (us)
byte   led1BlinkCount     = 0;
const  byte led1BlinkMax  = 3;
bool   led1On             = false;
bool joyBlocked = false;
unsigned long joyBlockUntil = 0;


//  TIEMPOS Y ESTADO SERVICIO 
unsigned long ambienteStart     = 0;  // Inicio de INFO_AMBIENTE
unsigned long prepararStart     = 0;  // Inicio de PREPARANDO
int           tiempoPreparacion = 0;  // Duración total de preparación en ms

byte  menuIndex     = 0;              // Índice de producto en el menú Servicio
bool  joyMoved      = false;          // Antirrebote del joystick (arriba/abajo)

volatile bool cliente = false;        // ¿Hay cliente a menos de 1 metro?

unsigned long systemStartMillis = 0;  // Para el contador en modo Admin

//  BOTÓN PRINCIPAL (ANTIRREBOTE + DURACIÓN) 
unsigned long myTime = 0;                // Tiempo actual en ms (lo actualiza loop)
volatile unsigned long lastValidTime = 0;// Último instante válido (antirrebote)
volatile bool pulsado = false;           // Se pone a true en la interrupción

bool buttonHolding = false;              // ¿Estamos manteniendo el botón?
unsigned long buttonPressStart = 0;      // Momento de inicio de la pulsación
volatile bool joyClick = false;  



//  PRODUCTOS (NOMBRES + PRECIOS) 
const char* productNames[] = {
  "Cafe Solo",
  "Cortado",
  "Doble",
  "Premium",
  "Chocolate"
};

float productPrices[] = {
  1.00,  // Cafe Solo
  1.10,  // Cortado
  1.25,  // Doble
  1.50,  // Premium
  2.00   // Chocolate
};

const byte NUM_PRODUCTOS = 5;
//  ADMIN: MENÚ 
const char* adminOptions[] = {
  "Ver temperatura",
  "Ver distancia",
  "Ver contador",
  "Modificar precios"
};
const byte NUM_ADMIN_OPTIONS = 4;


byte adminMenuIndex     = 0;   // índice menú Admin
byte adminPriceIndex    = 0;   // producto seleccionado en Modificar precios
bool adminPriceEditing  = false;
float adminOriginalPrice = 0.0f;  // precio original por si se cancela


//  SENSORES
LiquidCrystal lcd(12, 11, A5, 13, 5, 4);
DHT dht(PIN_DHT, DHTTYPE);

Thread led1BlinkThread = Thread();
Thread distanceThread  = Thread();
ThreadController controller = ThreadController();

//  ESTADOS  
enum State {
  SERVICIO_ARRANQUE,      // Parpadeo LED1 + "CARGANDO..."
  SERVICIO_ESPERANDO,     // Esperando cliente a < 1m
  SERVICIO_INFO_AMBIENTE, // Temp/hum 5 s
  SERVICIO_MENU,          // Selector de café
  SERVICIO_PREPARANDO,    // Preparación (tiempo aleatorio, LED2 prog.)
  SERVICIO_ENTREGA,       // "RETIRE BEBIDA"
  ADMIN_MENU,             // Menú principal Admin
  ADMIN_TEMP,             // Ver temperatura/humedad
  ADMIN_DIST,             // Ver distancia
  ADMIN_CONTADOR,         // Ver segundos desde arranque
  ADMIN_PRECIOS           // Modificar precios
};

State currentState = SERVICIO_ARRANQUE;

void interrupcionBoton() {
  if (myTime - lastValidTime > 100) { // 200 ms de antirrebote
    pulsado = true;                   // registramos una pulsación válida
    lastValidTime = myTime;
  }
}
void interrupcionJoy() {
    if (myTime - lastValidTime > 200 && !joyBlocked) { 
    joyClick = true;
    lastValidTime = myTime;
  }
}


// Procesa la duración de la pulsación (2–3 s y >=5 s) 
void handleButtonDurations() {
  // 1) Si la ISR ha detectado una pulsación válida (flanco FALLING)
  if (pulsado) {
    pulsado = false;
    // Inicio de pulsación (solo una vez por pulsación válida)
    if (!buttonHolding) {
      buttonHolding = true;
      buttonPressStart = myTime;
    }
  }

  // 2) Si estamos manteniendo el botón, miramos cuándo se suelta
  if (buttonHolding) {
    int level = digitalRead(PIN_BOTON); 
    if (level == HIGH) {
      buttonHolding = false;
      unsigned long dur = myTime - buttonPressStart;

      bool inAdmin =
        (currentState == ADMIN_MENU)     ||
        (currentState == ADMIN_TEMP)     ||
        (currentState == ADMIN_DIST)     ||
        (currentState == ADMIN_CONTADOR) ||
        (currentState == ADMIN_PRECIOS);

      bool enEstadoB =
        (currentState == SERVICIO_INFO_AMBIENTE) ||
        (currentState == SERVICIO_MENU)         ||
        (currentState == SERVICIO_PREPARANDO)   ||
        (currentState == SERVICIO_ENTREGA);

      //  PULSACIÓN LARGA >= 5 s -> ENTRAR / SALIR ADMIN 
      if (dur >= 5000) {
        if (!inAdmin) {
          // Entramos en Admin desde cualquier estado
          distanceThread.enabled = false;
          digitalWrite(PIN_LED1, HIGH);
          digitalWrite(PIN_LED2, HIGH);
          currentState = ADMIN_MENU;
          adminMenuIndex = 0;
          lcd.clear();
          lcd.print("ADMIN");
        } else {
          // Salimos de Admin y volvemos a ESPERANDO
          digitalWrite(PIN_LED1, LOW);
          analogWrite(PIN_LED2, 0);
          currentState = SERVICIO_ESPERANDO;
          cliente = false;
          distanceThread.enabled = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("ESPERANDO");
          lcd.setCursor(0, 1);
          lcd.print("CLIENTE");
        }
        return; 
      }

      // PULSACIÓN 2–3 s -> REINICIO SERVICIO (estado b) 
      if (!inAdmin && enEstadoB && dur >= 2000 && dur <= 3000) {
        currentState = SERVICIO_ESPERANDO;
        cliente = false;
        distanceThread.enabled = true;
        analogWrite(PIN_LED2, 0);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ESPERANDO");
        lcd.setCursor(0, 1);
        lcd.print("CLIENTE");
      }
    }
  }
}



float calculateDistance() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  duration = pulseIn(PIN_ECHO, HIGH);              // tiempo HIGH en Echo (us)
  distance = duration * V_SOUND_AIR / 2.0f;        // cm (ida y vuelta)
  return distance;
}

// Hilo: detección cliente en ESPERANDO
void distanceCallback() {
  if (currentState != SERVICIO_ESPERANDO) {
    calculateDistance();  
    return;
  }

  lcd.clear();
  if (calculateDistance() < 100.0f) { 
    cliente = true;
  } else {
    lcd.setCursor(0, 0);
    lcd.print("ESPERANDO");
    lcd.setCursor(0, 1);
    lcd.print("CLIENTE");
    cliente = false;
  }
}


// Hilo: parpadeo LED1 en ARRANQUE para que parpadee
void led1BlinkCallback() {
  if (led1BlinkCount >= led1BlinkMax) {
    digitalWrite(PIN_LED1, LOW);
    led1BlinkThread.enabled = false;
    return;
  }

  led1On = !led1On;
  if (led1On) {
    digitalWrite(PIN_LED1, HIGH);
  } else {
    digitalWrite(PIN_LED1, LOW);
  }


  if (led1On) {
    led1BlinkCount++;
  }
}

// Muestra producto actual en menú Servicio
void showCurrentProduct() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(productNames[menuIndex]);
  lcd.setCursor(0, 1);
  lcd.print("Precio: ");
  lcd.print(productPrices[menuIndex], 2);
  lcd.print(" Eur"); // EL simbolo de euro no se puede
}

// Ir a SERVICIO_ESPERANDO
void goToServicioEsperando() {
  currentState = SERVICIO_ESPERANDO;
  cliente = false;
  distanceThread.enabled = true;
  analogWrite(PIN_LED2, 0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESPERANDO");
  lcd.setCursor(0, 1);
  lcd.print("CLIENTE");
}

// Mostrar temperatura y humedad 5 s en Servicio
void updateInfoAmbiente() {
  static unsigned long lastDhtRead = 0;
  unsigned long now = millis();

  if (now - lastDhtRead > 1000) {
    lastDhtRead = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(t);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(h);
    lcd.print("%");
  }

  if (now - ambienteStart >= 5000) {
    currentState = SERVICIO_MENU;
    menuIndex = 0;
    joyMoved = false;
    showCurrentProduct();
  }
}

// Navegación menú Servicio (productos)
void updateMenu() {
  int yValue = analogRead(VRy);

  // Arriba
  if (yValue < 300 && !joyMoved) {
    if (menuIndex == 0) menuIndex = NUM_PRODUCTOS - 1;
    else menuIndex--;
    joyMoved = true;
    showCurrentProduct();
  }
  // Abajo
  else if (yValue > 700 && !joyMoved) {
    menuIndex = (menuIndex + 1) % NUM_PRODUCTOS;
    joyMoved = true;
    showCurrentProduct();
  }
  // En Medio
  else if (yValue >= 300 && yValue <= 700) {
    joyMoved = false;
  }

  // Selección con botón del joystick (usando interrupción)
  if (joyClick && !joyBlocked) {  // Solo si joyClick no está bloqueado
    joyClick = false;   

    // Bloqueamos el joystick durante un tiempo
    joyBlocked = true;
    joyBlockUntil = millis() + 1500;  // Bloqueo durante 1.5 segundos (me parecia un buen tiempo de antirrebote)

    // Solo se ejecuta una vez cuando el usuario selecciona un producto
    currentState = SERVICIO_PREPARANDO;

    int segundos = random(4, 9);           // Tiempo aleatorio para preparar (4-8 segundos)
    tiempoPreparacion = segundos * 1000;   // Convertir a milisegundos
    prepararStart = millis();              // Guardamos el tiempo de inicio de preparación

    lcd.clear();  // Limpiamos la pantalla para mostrar el mensaje de preparación
    lcd.print("Preparando Cafe");
    analogWrite(PIN_LED2, 0);  // Apagamos LED2 al comenzar la preparación
  }

}


void updatePreparando() {
  unsigned long now = millis();
  unsigned long elapsed = now - prepararStart;

  // Limitar por si se pasa
  if (elapsed > (unsigned long)tiempoPreparacion) {  //Con (Tipo)variable se convierte a esa variable a ese tipo
    elapsed = tiempoPreparacion;
  }

  // Progreso entre 0.0 y 1.0
  float progress = (float)elapsed / (float)tiempoPreparacion;

  // Convertir progreso en brillo (0 a 255)
  int pwm = (int)(progress * 255.0f);

  // Escribir PWM al LED2
  analogWrite(PIN_LED2, pwm);

  // Fin del tiempo de preparación 
  if (elapsed >= (unsigned long)tiempoPreparacion) {
    currentState = SERVICIO_ENTREGA;
    prepararStart = now;
    lcd.clear();
    lcd.print("RETIRE BEBIDA");
  }
}

// Mostrar RETIRE BEBIDA 3 s
void updateEntrega() {
  unsigned long now = millis();

  if (now - prepararStart >= 3000) {
    analogWrite(PIN_LED2, 0);
    joyBlocked = true;                   
    joyBlockUntil = millis() + 1000;      
    goToServicioEsperando();
  }

}

void adminTurnLedsOn() {
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, HIGH);
}

// Mostrar menú Admin
void showAdminMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADMIN:");
  lcd.setCursor(0, 1);
  lcd.print(adminOptions[adminMenuIndex]);
}


// Navegación menú Admin
void updateAdminMenu() {
  adminTurnLedsOn();

  int yValue = analogRead(VRy);

  // Arriba
  if (yValue < 300 && !joyMoved) {
    if (adminMenuIndex == 0) adminMenuIndex = NUM_ADMIN_OPTIONS - 1;
    else adminMenuIndex--;
    joyMoved = true;
    showAdminMenu();
  }
  // Abajo
  else if (yValue > 700 && !joyMoved) {
    adminMenuIndex = (adminMenuIndex + 1) % NUM_ADMIN_OPTIONS;
    joyMoved = true;
    showAdminMenu();
  }
  // Zona neutra (sin movimiento)
  else if (yValue >= 300 && yValue <= 700) {
    joyMoved = false;
  }

  // Selección con botón del joystick (usando interrupción)
  if (joyClick) {  // Si joyClick es true, procesamos el clic
    joyClick = false;   // Consumimos el clic para evitar que se dispare varias veces

    // Cambiamos el estado dependiendo de la opción seleccionada en el menú
    switch (adminMenuIndex) {
      case 0: currentState = ADMIN_TEMP; break;    // Ver temperatura/humedad
      case 1: currentState = ADMIN_DIST; break;    // Ver distancia
      case 2: currentState = ADMIN_CONTADOR; break; // Ver contador de tiempo
      case 3:
        currentState = ADMIN_PRECIOS;             // Modificar precios
        adminPriceIndex = 0;                      // Empezamos con el primer producto
        adminPriceEditing = false;                // Aseguramos que no estamos editando aún
        break;
    }
    lcd.clear();  // Limpiamos la pantalla antes de mostrar la siguiente opción
  }
}


//ver temperatura/humedad
void updateAdminTemp() {
  adminTurnLedsOn();
  static unsigned long lastDhtRead = 0;
  unsigned long now = millis();

  // Volver al menú admin con movimiento claro a la izquierda
  static bool joyMovedX = false;
  int xValue = analogRead(VRx);
  Serial.println(xValue);
  if (xValue < 100) {


    currentState = ADMIN_MENU;
    showAdminMenu();
    
  } else if (xValue >= 350 && xValue <= 650) {
    joyMovedX = false;
  }

  if (now - lastDhtRead > 1000) {
    lastDhtRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(t);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(h);
    lcd.print("%");
  }
}

//ver distancia
void updateAdminDist() {
  adminTurnLedsOn();
  static unsigned long lastRead = 0;
  unsigned long now = millis();

  static bool joyMovedX = false;
  int xValue = analogRead(VRx);
  if (xValue < 150 && !joyMovedX) {
    joyMovedX = true;
    currentState = ADMIN_MENU;
    showAdminMenu();
    return;
  } else if (xValue >= 350 && xValue <= 650) {
    joyMovedX = false;
  }

  if (now - lastRead > 300) {
    lastRead = now;
    float d = calculateDistance();

    lcd.setCursor(0, 0);
    lcd.print("Distancia:");
    lcd.setCursor(0, 1);
    lcd.print(d);
    lcd.print(" cm     ");
  }
}


// contador segundos desde arranque
void updateAdminContador() {
  adminTurnLedsOn();
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  static bool joyMovedX = false;
  int xValue = analogRead(VRx);
  if (xValue < 150 && !joyMovedX) {
    joyMovedX = true;
    currentState = ADMIN_MENU;
    showAdminMenu();
    return;
  } else if (xValue >= 350 && xValue <= 650) {
    joyMovedX = false;
  }

  if (now - lastUpdate > 1000) {
    lastUpdate = now;
    unsigned long elapsed = (now - systemStartMillis) / 1000;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Segundos desde");
    lcd.setCursor(0, 1);
    lcd.print("arranque: ");
    lcd.print(elapsed);
  }
}

// Mostrar producto+precio en Admin (modificar precios)
void showAdminPriceScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(productNames[adminPriceIndex]);
  lcd.setCursor(0, 1);
  lcd.print("Precio: ");
  lcd.print(productPrices[adminPriceIndex], 2);
  lcd.print(" Eur");
}

// Admin: modificar precios 
void updateAdminPrecios() {
  adminTurnLedsOn();

  int xValue = analogRead(VRx);
  int yValue = analogRead(VRy);

  // Antirrebote para el eje X (izquierda/derecha)
  static bool joyMovedX = false;

  // Movimiento izquierda:
  // - si estamos editando: cancelar y volver a lista
  // - si no editamos: volver al menú Admin
  Serial.println(xValue);
  if (xValue < 150 && !joyMovedX) {  
    joyMovedX = true;
    if (adminPriceEditing) {
        productPrices[adminPriceIndex] = adminOriginalPrice;
        adminPriceEditing = false;
        showAdminPriceScreen();
    } else {
        currentState = ADMIN_MENU;
        showAdminMenu();
    }
    return;
  } else if (xValue >= 350 && xValue <= 650) {  // Tiene que volver al centro para poder hacer otro movimiento
    joyMovedX = false;
  }


  if (!adminPriceEditing) {
    // MODO LISTA: seleccionar producto 
    if (yValue < 300 && !joyMoved) {
      if (adminPriceIndex == 0) adminPriceIndex = NUM_PRODUCTOS - 1;
      else adminPriceIndex--;
      joyMoved = true;
      showAdminPriceScreen();
    }
    else if (yValue > 700 && !joyMoved) {
      adminPriceIndex = (adminPriceIndex + 1) % NUM_PRODUCTOS;
      joyMoved = true;
      showAdminPriceScreen();
    }
    else if (yValue >= 300 && yValue <= 700) {
      joyMoved = false;
    }

    // Botón para entrar en modo edición
  if (!adminPriceEditing && joyClick) {
    joyClick = false;
    adminPriceEditing = true;
    adminOriginalPrice = productPrices[adminPriceIndex];
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Editando:");
    lcd.setCursor(0, 1);
    lcd.print(productNames[adminPriceIndex]);
    showAdminPriceScreen();
  }
  } else {
    //  cambiar precio con ARRIBA/ABAJO 
    if (yValue < 300 && !joyMoved) {
      productPrices[adminPriceIndex] += 0.05f;
      joyMoved = true;
      showAdminPriceScreen();
    } else if (yValue > 700 && !joyMoved) {
      productPrices[adminPriceIndex] -= 0.05f;
      if (productPrices[adminPriceIndex] < 0.0f) {
        productPrices[adminPriceIndex] = 0.0f;
      }
      joyMoved = true;
      showAdminPriceScreen();
    } else if (yValue >= 300 && yValue <= 700) {
      joyMoved = false;
    }
    else if (adminPriceEditing && joyClick) {
      joyClick = false;
      adminPriceEditing = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Precio guardado");
    }
  }
}

void setup() {

  wdt_enable(WDTO_2S);

  Serial.begin(9600);

  dht.begin();
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  pinMode(PIN_BOTON, INPUT_PULLUP);       // Botón principal con pull-up interno
  pinMode(JOY_BOTON, INPUT_PULLUP);

  digitalWrite(PIN_TRIGGER, LOW);
  digitalWrite(PIN_LED1, LOW);
  analogWrite(PIN_LED2, 0);

  lcd.begin(16, 2);
  lcd.print("CARGANDO...");

  randomSeed(analogRead(A2));
  systemStartMillis = millis();

  led1BlinkThread.onRun(led1BlinkCallback);
  led1BlinkThread.setInterval(1000);
  led1BlinkThread.enabled = true;
  controller.add(&led1BlinkThread);

  distanceThread.onRun(distanceCallback);
  distanceThread.setInterval(200);
  distanceThread.enabled = false;   // en ARRANQUE no se usa
  controller.add(&distanceThread);

  // Interrupción antirrebote tanto al boton normal como al del joystick
  attachInterrupt(digitalPinToInterrupt(PIN_BOTON), interrupcionBoton, FALLING);
  attachInterrupt(digitalPinToInterrupt(JOY_BOTON), interrupcionJoy, FALLING);

}


// En el loop, verificamos si el joystick se desbloquea
void loop() {
  wdt_reset();  // Alimenta el watchdog
  // Tiempo actual para ISR de antirrebote y resto de lógica
  myTime = millis();

  if (joyBlocked && millis() > joyBlockUntil) {
    joyBlocked = false;   // Joystick desbloqueado, puede ser usado nuevamente
  }


  // Hilos (LED1, sensor ultrasonidos)
  controller.run();

  // Gestionar pulsaciones largas SOLO al soltar el botón
  handleButtonDurations();

  // Máquina de estados principal
  switch (currentState) {
    case SERVICIO_ARRANQUE:
      if (!led1BlinkThread.enabled) {
        distanceThread.enabled = true;
        goToServicioEsperando();
      }
      break;

    case SERVICIO_ESPERANDO:
      if (cliente) {
        cliente = false;
        currentState = SERVICIO_INFO_AMBIENTE;
        ambienteStart = millis();
        distanceThread.enabled = false;
        lcd.clear();
      }
      break;

    case SERVICIO_INFO_AMBIENTE:
      updateInfoAmbiente();
      break;

    case SERVICIO_MENU:
      updateMenu();
      break;

    case SERVICIO_PREPARANDO:
      updatePreparando();
      break;

    case SERVICIO_ENTREGA:
      updateEntrega();
      break;

    case ADMIN_MENU:
      updateAdminMenu();
      break;

    case ADMIN_TEMP:
      updateAdminTemp();
      break;

    case ADMIN_DIST:
      updateAdminDist();
      break;

    case ADMIN_CONTADOR:
      updateAdminContador();
      break;

    case ADMIN_PRECIOS:
      updateAdminPrecios();
      break;
  }
}
