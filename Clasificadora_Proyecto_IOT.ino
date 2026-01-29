#include <SoftwareSerial.h>
#include <DHT.h>
#include <Servo.h>

// ------------------------------------------------------------------------------ CONFIGURACIÓN--------------------------------------------------------------------------

SoftwareSerial esp8266(2, 3); // RX=2, TX=3

// WiFi y ThingSpeaks
String ssid = "CAMPUS_EPN";
String password = "politecnica**";
String apiKey = "URPH3Y6MH10ND3YQ";
String host = "api.thingspeak.com";

// ------------------------------------------------------------------------------ PINES ------------------------------------------------------------------------------
#define PIN_TRIG 4
#define PIN_ECHO 5
#define S2 6
#define S3 7
#define OUT 8
#define PIN_SERVO_SW 9
#define PIN_SERVO_CL 10
#define PIN_DHT 11
#define PIN_LDR A0

// ------------------------------------------------------------------------------ OBJETOS ------------------------------------------------------------------------------

DHT dht(PIN_DHT, DHT11);
Servo servoBanda;
Servo servoClasif;

// ------------------------------------------------------------------------------ VARIABLES------------------------------------------------------------------------------

bool MODO_CALIBRACION = false; // true = ver valores RGB, esto sirve para calibrar un nuevo color en el sensor 
unsigned long tiempoUltimaDeteccion = 0;
unsigned long tiempoUltimoEnvio = 0;
const unsigned long TIEMPO_APAGADO_BANDA = 10000; 
const unsigned long INTERVALO_ENVIO = 20000; 

bool bandaEncendida = false;
int colorDetectadoID = 0; // 0=Nada, 1=Rojo, 2=Verde, 3=Azul, 4=Descarte

// Posiciones servo (AJUSTAR según tu montaje)
const int POS_ROJO = 0;
const int POS_VERDE = 60;
const int POS_AZUL = 120;
const int POS_DESCARTE = 180;

// Umbrales de color (CALIBRAR con MODO_CALIBRACION=true)
const int UMBRAL_ROJO = 150;
const int UMBRAL_VERDE = 300;
const int UMBRAL_AZUL = 250;

void setup() {
  Serial.begin(9600);
  Serial.println("\n=== INICIANDO SISTEMA ===");
  
  // Inicializar ESP-01
  Serial.println("Configurando ESP-01...");
  esp8266.begin(115200); // Velocidad por defecto
  delay(100);
  
  // Cambiar velocidad a 9600 
  sendCommand("AT+UART_DEF=9600,8,1,0,0", 2000);
  esp8266.begin(9600);
  delay(100);
  
  //---------------------------------------------------------- Inicializar sensores----------------------------------------------------
  dht.begin();
  servoBanda.attach(PIN_SERVO_SW);
  servoClasif.attach(PIN_SERVO_CL);
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(OUT, INPUT);
  pinMode(PIN_LDR, INPUT);
  
  // Posición inicial
  servoBanda.write(0);
  servoClasif.write(POS_DESCARTE);
  
  Serial.println("Hardware inicializado OK");
  
  // Conectar WiFi
  connectWiFi();
  
  if (MODO_CALIBRACION) {
    Serial.println("\n*** MODO CALIBRACION ACTIVO ***");
    Serial.println("Coloca objetos de colores y anota los valores R, G, B");
    Serial.println("Formato: R:xxx G:xxx B:xxx");
    Serial.println("Valor MÁS BAJO = color detectado\n");
  } else {
    Serial.println("\n=== SISTEMA LISTO ===");
    Serial.println("Esperando objetos...\n");
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {

  // 1. Monitoreo ambiental
  verificarAmbiente();

  // 2. Gestión de banda (detección ultrasónica)
  gestionarBanda();
  

  
  // 3. Clasificación de color
  if (bandaEncendida || MODO_CALIBRACION) {
    identificarColor();
  }
  
  // 4. Envío IoT cada 20 segundos
  if (!MODO_CALIBRACION && (millis() - tiempoUltimoEnvio > INTERVALO_ENVIO)) {
    enviarDatos();
  }
  
  delay(1000); // Pequeña pausa para estabilidad
}

// -------------------------------------------------------------------- FUNCIONES --------------------------------------------
////////////////////////////////BANDA////////////////////////////////////////////////////////
void gestionarBanda() {
  // Disparar pulso ultrasónico
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  // Medir eco
  long duracion = pulseIn(PIN_ECHO, HIGH, 30000); 
  int distancia = duracion * 0.034 / 2;
  float temp = dht.readTemperature();
  
  // Detectar objeto cercano
  if (distancia > 3 && distancia < 7) {
    tiempoUltimaDeteccion = millis();
    
    if (!bandaEncendida) {
      servoBanda.write(90); // ON 
      bandaEncendida = true;
      Serial.println(">>> BANDA ENCENDIDA (objeto a " + String(distancia) + " cm)");
    }
  }
  
  // Parar la banda para que seleccione el color
  if (distancia > 15 && distancia < 18 ) {
    tiempoUltimaDeteccion = millis();
    
    if (!bandaEncendida) {
      servoBanda.write(0); // OFF 
      bandaEncendida = false;
      Serial.println(">>> BANDA APAGADA (objeto a " + String(distancia) + " cm)");
    }
  }
  if (temp > 26){
      Serial.println("⚠️ ALERTA: Temperatura de la fabrica no optima (valor: " + String(temp) + ")");
      servoBanda.write(0); // OFF
      bandaEncendida = false;
      Serial.println(">>> BANDA APAGADA (Falla en el ambiente de la fábrica)");
    }
    
  // Apagar por inactividad
  if (bandaEncendida && (millis() - tiempoUltimaDeteccion > TIEMPO_APAGADO_BANDA)) {
    servoBanda.write(0); // OFF
    bandaEncendida = false;
    Serial.println(">>> BANDA APAGADA (inactividad)");
  }
}
////////////////////////////////AMBIENTE/////////////////////////////////////////////////////////////////
void verificarAmbiente() {
  
  static unsigned long ultimaVerificacion = 0;
  
  // Verificar cada 5 segundos
  if (millis() - ultimaVerificacion > 5000) {
    int luz = analogRead(PIN_LDR);
    float temp = dht.readTemperature();
    
    if (luz > 800) {
      Serial.println("⚠️ ALERTA: Fuga de luz detectada (valor: " + String(luz) + ")");
    }

    if (isnan(temp)) {
      Serial.println("⚠️ Error leyendo DHT11");
    }
    
    ultimaVerificacion = millis();
  }
}

/////////////////////////////////////////////////////////////
void identificarColor() {
  int R, G, B;
  
  // Leer componente ROJO
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  R = pulseIn(OUT, LOW);
  delay(40);
  
  // Leer componente VERDE
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  G = pulseIn(OUT, LOW);
  delay(40);
  
  // Leer componente AZUL
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  B = pulseIn(OUT, LOW);
  delay(40);
  
  // MODO CALIBRACIÓN: solo mostrar valores
  if (MODO_CALIBRACION) {
    Serial.print("R:"); Serial.print(R);
    Serial.print("  G:"); Serial.print(G);
    Serial.print("  B:"); Serial.println(B);
    delay(500);
    return;
  }
  
  // CLASIFICACIÓN (el valor MÁS BAJO es el color detectado)
  bool colorReconocido = false;
  
  if (R < G && R < B && R < UMBRAL_ROJO) {
    Serial.println("✓ ROJO detectado (R:" + String(R) + ")");
    servoClasif.write(POS_ROJO);
    colorDetectadoID = 1;
    colorReconocido = true;
  }
  else if (G < R && G < B && G < UMBRAL_VERDE) {
    Serial.println("✓ VERDE detectado (G:" + String(G) + ")");
    servoClasif.write(POS_VERDE);
    colorDetectadoID = 2;
    colorReconocido = true;
  }
  else if (B < R && B < G && B < UMBRAL_AZUL) {
    Serial.println("✓ AZUL detectado (B:" + String(B) + ")");
    servoClasif.write(POS_AZUL);
    colorDetectadoID = 3;
    colorReconocido = true;
  }
  else {
    Serial.println("❌ Color desconocido → DESCARTE");
    Serial.println("   (R:" + String(R) + " G:" + String(G) + " B:" + String(B) + ")");
    servoClasif.write(POS_DESCARTE);
    colorDetectadoID = 4;
  }
  
  if (colorReconocido) {
    delay(5000); // Esperar que caiga el objeto
    servoClasif.write(POS_DESCARTE); // Volver a neutral
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
void connectWiFi() {
  Serial.println("\n--- Conectando WiFi ---");
  
  sendCommand("AT+RST", 2000); // Reiniciar ESP-01
  sendCommand("AT+CWMODE=1", 1000); // Modo estación
  
  String cmd = "AT+CWJAP=\"" + ssid + "\",\"" + password + "\"";
  String resp = sendCommand(cmd, 8000); // Tiempo generoso para conectar
  
  if (resp.indexOf("OK") != -1 || resp.indexOf("CONNECTED") != -1) {
    Serial.println("✓ WiFi conectado");
  } else {
    Serial.println("❌ Error conectando WiFi");
    Serial.println("Verifica SSID y contraseña");
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void enviarDatos() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int l = analogRead(PIN_LDR);
  
  // Validar lecturas
  if (isnan(t) || isnan(h)) {
    Serial.println("❌ Error DHT11, saltando envío");
    tiempoUltimoEnvio = millis();
    return;
  }
  
  Serial.println("\n--- Enviando a ThingSpeak ---");
  Serial.println("Banda: " + String(bandaEncendida ? "ON" : "OFF"));
  Serial.println("Temp: " + String(t) + "°C");
  Serial.println("Color: " + String(colorDetectadoID));
  Serial.println("Luz: " + String(l));
  
  // Abrir conexión TCP
  String cmd = "AT+CIPSTART=\"TCP\",\"" + host + "\",80";
  String resp = sendCommand(cmd, 3000);
  
  if (resp.indexOf("ERROR") != -1) {
    Serial.println("❌ Error abriendo conexión");
    tiempoUltimoEnvio = millis();
    return;
  }
  
  // Construir petición GET
  String datos = "GET /update?api_key=" + apiKey;
  datos += "&field1=" + String((int)bandaEncendida);
  datos += "&field2=" + String(t, 1);
  datos += "&field3=" + String(colorDetectadoID);
  datos += "&field4=" + String(l);
  datos += " HTTP/1.1\r\n";
  datos += "Host: " + host + "\r\n";
  datos += "Connection: close\r\n\r\n";
  
  // Enviar datos
  cmd = "AT+CIPSEND=" + String(datos.length());
  sendCommand(cmd, 1000);
  
  esp8266.print(datos);
  delay(3000); // Esperar respuesta del servidor
  
  // Leer respuesta
  String response = "";
  while (esp8266.available()) {
    response += (char)esp8266.read();
  }
  
  // Cerrar conexión
  sendCommand("AT+CIPCLOSE", 1000);
  
  if (response.indexOf("200 OK") != -1) {
    Serial.println("✓ Datos enviados exitosamente");
  } else {
    Serial.println("⚠️ Respuesta del servidor:");
    Serial.println(response.substring(0, 100)); 
  }
  
  tiempoUltimoEnvio = millis();
}

String sendCommand(String cmd, int timeout) {
  String response = "";
  
  esp8266.println(cmd);
  
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (esp8266.available()) {
      char c = esp8266.read();
      response += c;
    }
  }
  
  // Mostrar respuesta en modo debug
  if (response.length() > 0) {
    Serial.print("[ESP-01] ");
    Serial.println(response);
  }
  
  return response;
}
