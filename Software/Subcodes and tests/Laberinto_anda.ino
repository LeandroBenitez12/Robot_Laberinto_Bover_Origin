#include "BluetoothSerial.h"
// DEBUG
#define DEBUG_DE_CASOS 1
#define DEBUG_DE_SENSORES 1
#define DEBUG_DE_BUZZER 1
// declaramos pines
// motores
#define ENA 15
#define MR1 18
#define MR2 5
#define ENB 19
#define ML1 4
#define ML2 2
// ultrasonidos
#define ECHO1 35
#define ECHO2 33
#define ECHO3 26
#define ECHO4 14
#define TRIG1 32
#define TRIG2 25
#define TRIG3 27
#define TRIG4 12
#define DISTANCIA_MINIMA 7
#define DISTANCIA_LADOS 8
#define TICK_STOP 1000
#define TICK_ULTRASONIDO 10
#define VELOCIDAD1 180
#define VELOCIDAD2 180
#define BUZZER 23
#define BUTTON 13
// pwm DE UN MOTOR
const int freq = 5000;
const int PWMChannel = 0;
const int resolution = 8;
// pwm DE UN 2DO MOTOR
const int freq2 = 5000;
const int PWMChannel2 = 1;
const int resolution2 = 8;
// Sensores
int sensor_derecho;
int sensor_frontal;
int sensor_izquierdo;
// button
bool button_start;

int periodo = 1000;
#define PERIODO 100
unsigned long tiempo_actual = 0;
unsigned long tiempo_actual_stop = 0;
unsigned long tiempo_actual_button = 0;
// PIN_SENSORES en una matriz
int pin_sensores_ultrasonido[4][2] = {
    {ECHO1, TRIG1},
    {ECHO2, TRIG2},
    {ECHO3, TRIG3},
    {ECHO4, TRIG4},
};
// establecemos conexion bluetooth
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
//------------------------------------------------------------------------------------------

// ESTADOS DEL SENSOR

enum UBICACIONES
{
  INICIAL,
  PASILLO,
  DESVIO_IZQUIERDA,
  DESVIO_DERECHA,
  CALLEJON,
  CRUCE_T,
  CRUCE,
};

//------------------------------------------------------------------------------------------

void AsignacionpinesMOTORES()
{
  pinMode(MR1, OUTPUT);
  pinMode(MR2, OUTPUT);
  pinMode(ML1, OUTPUT);
  pinMode(ML2, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLDOWN);
}
// Asignacion de pines utilizando la matriz para no declarar cada sensor y hacer 100 lines de cod
void asignacionPinesSensores()
{
  for (int fila = 0; fila < 4; fila++)
  {
    int pin_echo = pin_sensores_ultrasonido[fila][0];
    int pin_trig = pin_sensores_ultrasonido[fila][1];
    pinMode(pin_echo, INPUT);
    pinMode(pin_trig, OUTPUT);
    digitalWrite(pin_trig, LOW);
  }
}
void AsignacionPinesPWM()
{
  ledcSetup(PWMChannel, freq, resolution);
  ledcSetup(PWMChannel2, freq2, resolution2);
  ledcAttachPin(ENA, PWMChannel);
  ledcAttachPin(ENB, PWMChannel2);
}
//---------------------------------------------------------------------------------------------------
enum MOTOR_ESTADO
{
  AVANZAR,
  RETROCEDER,
  STOP
};
void Motor(int estado, int pin1, int pin2, int pwm, int velocidad)
{

  ledcWrite(pwm, velocidad);

  switch (estado)
  {
  case AVANZAR:
  {
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, HIGH);
    break;
  }
  case RETROCEDER:
  {
    digitalWrite(pin1, HIGH);
    digitalWrite(pin2, LOW);
    break;
  }
  case STOP:
  {
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    break;
  }
  }
}

void MotorDer(int estado)
{
  Motor(estado, MR1, MR2, PWMChannel,VELOCIDAD1);
}
void MotorIzq(int estado)
{
  Motor(estado, ML1, ML2, PWMChannel2,VELOCIDAD2);
}
// HACIA ADELANTE
void Forward()
{
  MotorDer(AVANZAR);
  MotorIzq(AVANZAR);
}
// HACIA ATRAS
void Backward()
{
  MotorDer(RETROCEDER);
  MotorIzq(RETROCEDER);
}
// GIRO SOBRE SU PROPIO EJE A LA DERECHA
void Right()
{
  MotorDer(RETROCEDER);
  MotorIzq(AVANZAR);
}
// GIRO SOBRE SU PROPIO EJE A LA IZQUIERDA
void Left()
{
  MotorDer(AVANZAR);
  MotorIzq(RETROCEDER);
}
// PARAR
void Stop()
{
  MotorDer(STOP);
  MotorIzq(STOP);
}
void Buzzer()
{
  digitalWrite(BUZZER, HIGH);
}
//-------------------------------------------------------------
int LeerUltrasonidos(int pin_trig, int pin_echo)
{
  long distancia;
  long duracion;
  // SENSOR
  digitalWrite(pin_trig, HIGH);
  delayMicroseconds(10); // Enviamos un pulso de 10us
  digitalWrite(pin_trig, LOW);
  duracion = pulseIn(pin_echo, HIGH);
  distancia = duracion / 58.2;
  return distancia;
}
int LeerButton(int button)
{
  bool estado_button;
  estado_button = digitalRead(button);
  return estado_button;
}

//-------------------------------------------------------------
void ImprimirEstadoRobot(int ubicacion) 
{
  String estado_robot = "";
  if (ubicacion == INICIAL)
    {estado_robot = "STANDBY";}
   else if (ubicacion == PASILLO)
   {estado_robot = "PASILLO";} 
  else if (ubicacion == DESVIO_IZQUIERDA)
    {estado_robot = "DOBLAR IZQUIERDA";}
  else if (ubicacion == DESVIO_DERECHA)
    {estado_robot = "DOBLAR DERECHA";}
  else if (ubicacion == CRUCE_T)
    {estado_robot = "ES UNA T";}
  else if (ubicacion == CRUCE)
    {estado_robot = " LIBRE ";}
  else if (ubicacion == CALLEJON)
    {estado_robot = " ENCERRADO";}

  SerialBT.print("State: ");
  SerialBT.print(estado_robot);
  SerialBT.print(" | ");
  SerialBT.print(button_start);
  SerialBT.print(" | ");
}
void ImprimirDatos(int sd, int sai, int si)
{
  SerialBT.print("   sensor_derecho: ");
  SerialBT.print(sd);
  SerialBT.print("   - sensor_frontal: ");
  SerialBT.print(sai);
  SerialBT.print("   - sensor_izquierdo: ");
  SerialBT.println(si);
}

//----------------------------------------------------------------
// Maquina de estados
int ubicacion = INICIAL;
void MovimientosDelRobot()
{
  switch (ubicacion)
  {
  case INICIAL:
  {
    if (millis() > tiempo_actual_button + PERIODO)
    {
      tiempo_actual_button = millis();
      button_start = LeerButton(BUTTON);
    }
    if (button_start)
    {
      {ubicacion = PASILLO;}
    }
    else
      {
        Stop();
      }

    break;
  }

  case PASILLO:
  {
    if (sensor_frontal >= DISTANCIA_MINIMA && sensor_izquierdo <= DISTANCIA_LADOS && sensor_derecho <= DISTANCIA_LADOS)
    {
      Forward();
    }
    if (sensor_frontal < DISTANCIA_MINIMA)
    {

      if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
      {
        ubicacion = DESVIO_DERECHA;
      }
      if (sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
      {
        ubicacion = DESVIO_IZQUIERDA;
      }
      if (sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
      {
        ubicacion = CRUCE_T;
      }
      if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
      {
        ubicacion = CALLEJON;
      }
      }
      if (sensor_frontal > DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
      {
        ubicacion = CRUCE;
      }

    break;
  }

  case DESVIO_DERECHA:
  {

    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      Right();
    }

    if (sensor_frontal > DISTANCIA_MINIMA)
      {ubicacion = PASILLO;}
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = DESVIO_IZQUIERDA;
    }
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = CRUCE_T;
    }
    if (sensor_frontal > DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = CRUCE;
    }
    if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = CALLEJON;
    }
    break;
  }
  case DESVIO_IZQUIERDA:
  {
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      Left();
    }

    if (sensor_frontal > DISTANCIA_MINIMA)
      {ubicacion = PASILLO;}
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = DESVIO_DERECHA;
    }
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = CRUCE_T;
    }
    if (sensor_frontal > DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = CRUCE;
    }
    if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = CALLEJON;
    }
    break;
  }
  case CRUCE_T:
  {
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      Right();
    }

    if (sensor_frontal > DISTANCIA_MINIMA)
      {{ubicacion = PASILLO;}}
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = DESVIO_IZQUIERDA;
    }
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = DESVIO_DERECHA;
    }
    if (sensor_frontal > DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = CRUCE;
    }
    if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = CALLEJON;
    }
    break;
  }

  case CRUCE:
  {
    if (sensor_frontal > DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      Right();
    }

    if (sensor_frontal > DISTANCIA_MINIMA)
      {ubicacion = PASILLO;}
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = DESVIO_IZQUIERDA;
    }
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = DESVIO_DERECHA;
    }    
    if (sensor_frontal <= DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
    {
      ubicacion = CRUCE_T;
    }
    if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      ubicacion = CALLEJON;
    }

    break;
  }
  case CALLEJON:{
    if (sensor_frontal < DISTANCIA_MINIMA && sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
    {
      Right();
    }
    if (sensor_frontal > DISTANCIA_MINIMA)
    {
      ubicacion = PASILLO;
    }
    
    if (sensor_frontal <= DISTANCIA_MINIMA)
    {
      
      if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
      {
        ubicacion = DESVIO_DERECHA;
      }
      if (sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
      {
        ubicacion = DESVIO_IZQUIERDA;
      }
      if (sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
      {
        ubicacion = CRUCE_T;
      }
      if (sensor_izquierdo < DISTANCIA_MINIMA && sensor_derecho < DISTANCIA_MINIMA)
      {
        ubicacion = CALLEJON;
      }
      }
      if (sensor_frontal > DISTANCIA_MINIMA && sensor_izquierdo > DISTANCIA_MINIMA && sensor_derecho > DISTANCIA_MINIMA)
      {
        ubicacion = CRUCE;
      }
    
    
    break;
  }
  }
}
//-------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  SerialBT.begin("ESP32_Benitez"); // Nombre del Bluetooth
  AsignacionpinesMOTORES();
  asignacionPinesSensores();
  AsignacionPinesPWM();
}
void loop()
{
  if (Serial.available())
  {
    SerialBT.write(Serial.read());
  }

  if (millis() > tiempo_actual + TICK_ULTRASONIDO)
  {
    tiempo_actual = millis();
    sensor_izquierdo = LeerUltrasonidos(TRIG1, ECHO1);
    sensor_frontal = LeerUltrasonidos(TRIG2, ECHO2);
    sensor_derecho = LeerUltrasonidos(TRIG4, ECHO4);
  }
  ubicacion = PASILLO;
  MovimientosDelRobot();
  if (SerialBT.available())
  {

    if (DEBUG_DE_CASOS)
    {
      ImprimirEstadoRobot(ubicacion);
    }
    if (DEBUG_DE_SENSORES)
    {
      ImprimirDatos(sensor_derecho, sensor_frontal, sensor_izquierdo);
    }
  }
}