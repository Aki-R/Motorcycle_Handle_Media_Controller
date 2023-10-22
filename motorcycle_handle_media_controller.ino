#include <BleKeyboard.h>

#define BOOT_SW 15
#define LED_RED 18

// Timer
hw_timer_t * timer = NULL;
// Queues
#define QUEUE_LENGTH 1
QueueHandle_t xQueue;
//Timer Task Handler
TaskHandle_t taskHandle;
// BLE Media Controller
BleKeyboard bleKeyboard;

// SW status
bool switch_status = false;
bool switch_status_k1 =false;

// Switch status timer
int timer_sw_on = 0; // ms
int timer_sw_off = 0; //ms

// Switch press counter
int press_counter_short = 0;
int press_counter_long = 0;

//
bool flag_enable_play_resume = true;

// Constant Parameter
const int dt_10ms = 10;
// Configuration Parameter
// short time press time
const int time_sw_press_short = 500;  //ms
// long time press time
const int time_sw_press_long = 500;  //ms
// time off time fix
const int time_sw_fix = 500;  //ms

// Timer Interrupt
void IRAM_ATTR onTimer() {
  int8_t data;
  // Send Queue
  xQueueSendFromISR(xQueue, &data, 0);
}

// 10ms Task
void task(void *pvParameters) {
  while (1) {
    int8_t data;
    // wait until timer interruption
    xQueueReceive(xQueue, &data, portMAX_DELAY);
    // task
    task_10ms();
  }
}

void task_10ms(void){
  switch_status = !digitalRead(BOOT_SW);
  if(bleKeyboard.isConnected()) {
    digitalWrite(LED_RED, HIGH);

    if(switch_status){
      timer_sw_on += dt_10ms;
      if(timer_sw_on > time_sw_press_long && flag_enable_play_resume){
        press_counter_long++;
      }
    }
    if(!switch_status){
      timer_sw_off += dt_10ms;
    }
    
    // rising edge
    if(switch_status && !switch_status_k1){
      timer_sw_on = 0;
      Serial.println("pressed");
    }
    // falling edge
    if(!switch_status && switch_status_k1){
      if(timer_sw_on < time_sw_press_short){
        press_counter_short++;
      }
      Serial.println("released");
      flag_enable_play_resume = true;
      timer_sw_off = 0;
    }

    

    // FIX command
    if(timer_sw_off > time_sw_fix){
      if(press_counter_short == 1){
        bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
        Serial.println("next truck");
      }else if(press_counter_short == 2){
        bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
        Serial.println("previous truck");
      }else if(press_counter_short == 3){
        bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
        bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
        Serial.println("previous previous truck");
      }else{
        // Do nothing
      }
      press_counter_short = 0;
    }

    if(press_counter_long > 0 && flag_enable_play_resume){
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
      Serial.println("play/pause");
      Serial.println(press_counter_long);
      press_counter_short = 0;
      press_counter_long = 0;
      flag_enable_play_resume = false;
    }

  }else{
    digitalWrite(LED_RED, LOW);
  }
  switch_status_k1 = switch_status;
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleKeyboard.begin();
  pinMode(BOOT_SW,INPUT_PULLUP);
  pinMode(LED_RED,OUTPUT);

  // Create Queue
  xQueue = xQueueCreate(QUEUE_LENGTH, sizeof(int8_t));
  xTaskCreateUniversal(
    task,           // task function
    "task",         // task name
    8192,           // stack size
    NULL,           // arguments
    5,              // priority
    &taskHandle,    // task handler
    APP_CPU_NUM     // CPU(PRO_CPU_NUM or APP_CPU_NUM)
  );
  // Use 1 timer of 4 timers
  // Count every 1 micro second
  // true: count up
  timer = timerBegin(0, getApbFrequency() / 1000000, true);
  // Config of timer interrupt
  timerAttachInterrupt(timer, &onTimer, true);
  // Set 10000 counts (=10ms)
  timerAlarmWrite(timer, 10000, true);
  // Start Timer
  timerAlarmEnable(timer);
}

void loop() {
  delay(1);
}