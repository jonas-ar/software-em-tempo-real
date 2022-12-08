#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

/* defines - LCD */
#define LCD_16X2_CLEAN_LINE                "                "
#define LCD_16X2_I2C_ADDRESS               0x27
#define LCD_16X2_COLS                      16 
#define LCD_16X2_ROWS                      2 

/* defines - LED */
#define LED_PIN                      12
#define LED_THRESHOLD                3.58 /* V

/* defines - ADC */
#define ADC_MAX                      1023.0
#define MAX_VOLTAGE_ADC              5.0

/* definição de tipos */
#define portCHAR		char
#define portFLOAT		float
#define portDOUBLE		double
#define portLONG		long
#define portSHORT		int
#define portSTACK_TYPE	unsigned portCHAR
#define portBASE_TYPE	char

/* tasks */
void task_breathing_light( void *pvParameters );
void task_serial( void *pvParameters );
void task_lcd( void *pvParameters );
void task_sensor( void *pvParameters );
void task_led( void *pvParameters );

/* Variaveis relacionadas ao LCD */
LiquidCrystal_I2C lcd(LCD_16X2_I2C_ADDRESS, 
                      LCD_16X2_COLS, 
                      LCD_16X2_ROWS);

/* filas (queues) */
QueueHandle_t xQueue_LCD, xQueue_LED;

/* semaforos utilizados */
SemaphoreHandle_t xSerial_semaphore;

void setup() {
  
  /* Inicializa serial (baudrate 9600) */
  Serial.begin(9600);

  /* Inicializa o LCD, liga o backlight e limpa o LCD */
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  /* Inicializa e configura GPIO do LED */ 
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  while (!Serial) {
    ; /* Somente vai em frente quando a serial estiver pronta para funcionar */
  }

  /* Criação das filas (queues) */ 
  xQueue_LCD = xQueueCreate( 1, sizeof( float ) );
  xQueue_LED = xQueueCreate( 1, sizeof( float ) );

  /* Criação dos semaforos */
  xSerial_semaphore = xSemaphoreCreateMutex();

  if (xSerial_semaphore == NULL)
  {
     Serial.println("Erro: nao e possivel criar o semaforo");
     while(1); /* Sem semaforo o funcionamento esta comprometido. Nada mais deve ser feito. */
  }
  
  /* Criação das tarefas */
  xTaskCreate(
    task_sensor                     /* Funcao a qual esta implementado o que a tarefa deve fazer */
    ,  (const portCHAR *) "sensor"   /* Nome (para fins de debug, se necessário) */
    ,  128                          /* Tamanho da stack (em words) reservada para essa tarefa */
    ,  NULL                         /* Parametros passados (nesse caso, não há) */
    ,  3                            /* Prioridade */
    ,  NULL );                      /* Handle da tarefa, opcional (nesse caso, não há) */

  xTaskCreate(
    task_lcd
    ,  (const portCHAR *) "LCD"
    ,  156  
    ,  NULL
    ,  2 
    ,  NULL );


  xTaskCreate(
    task_led
    ,  (const portCHAR *)"LED"
    ,  128  
    ,  NULL
    ,  1  
    ,  NULL );

  /* A partir deste momento, o scheduler de tarefas entra em ação e as tarefas executam */
}

void loop()
{
  /* Tudo é executado nas tarefas. Há nada a ser feito aqui. */
}

/* --------------------------------------------------*/
/* ---------------------- Tarefas -------------------*/
/* --------------------------------------------------*/

void task_sensor( void *pvParameters )
{
    (void) pvParameters;
    int adc_read=0;
    UBaseType_t uxHighWaterMark;
    float voltage = 0.0;
    
    while(1)
    {   
        adc_read = analogRead(0);
        voltage = ((float)adc_read/ADC_MAX)*MAX_VOLTAGE_ADC;
        
        /* Envia tensão lida em A0 para as tarefas a partir de filas */
        xQueueOverwrite(xQueue_LCD, (void *)&voltage);
        xQueueOverwrite(xQueue_LED, (void *)&voltage);
        
        /* Espera um segundo */
        vTaskDelay( 1000 / portTICK_PERIOD_MS ); 

        /* Para fins de teste de ocupação de stack, printa na serial o high water mark */
        xSemaphoreTake(xSerial_semaphore, portMAX_DELAY );
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        Serial.print("task_sensor high water mark (words): ");
        Serial.println(uxHighWaterMark);
        Serial.println("---");
        xSemaphoreGive(xSerial_semaphore);
    }
}

void task_lcd( void *pvParameters )
{
    (void) pvParameters;
    float voltage_rcv = 0.0;
    UBaseType_t uxHighWaterMark;

    while(1)
    {        
        /* Espera até algo ser recebido na queue */
        xQueueReceive(xQueue_LCD, (void *)&voltage_rcv, portMAX_DELAY);
        
        /* Uma vez recebida a informação na queue, a escreve no display LCD */
        lcd.setCursor(0,0);
        lcd.print("Voltage: ");
        lcd.setCursor(0,1);
        lcd.print(LCD_16X2_CLEAN_LINE);
        lcd.setCursor(0,1);
        lcd.print(voltage_rcv);
        lcd.setCursor(15,1);
        lcd.print("V");

        /* Para fins de teste de ocupação de stack, printa na serial o high water mark */
        xSemaphoreTake(xSerial_semaphore, portMAX_DELAY );
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        Serial.print("task_lcd high water mark (words): ");
        Serial.println(uxHighWaterMark);
        Serial.println("---");
        xSemaphoreGive(xSerial_semaphore);
    }  
}

void task_led( void *pvParameters )
{
    (void) pvParameters;
    float voltage_rcv = 0.0;
    UBaseType_t uxHighWaterMark;

    while(1)
    {    
        /* Espera até algo ser recebido na queue */ 
        xQueueReceive(xQueue_LED, (void *)&voltage_rcv, portMAX_DELAY);   

        /* Uma vez recebida a informação na queue, verifica se o LED deve acender ou não */
        if (voltage_rcv > LED_THRESHOLD)
            digitalWrite(LED_PIN, HIGH);  
        else
            digitalWrite(LED_PIN, LOW);  

        /* Para fins de teste de ocupação de stack, printa na serial o high water mark */
        xSemaphoreTake(xSerial_semaphore, portMAX_DELAY );
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        Serial.print("task_led high water mark (words): ");
        Serial.println(uxHighWaterMark);
        Serial.println("---");    
        xSemaphoreGive(xSerial_semaphore);
    }  
}