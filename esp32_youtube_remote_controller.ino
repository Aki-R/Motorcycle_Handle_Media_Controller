#include "BleKeyboard.h"

#define LED_RED 2
#define SW_UP 32
#define SW_DOWN 12
#define SW_LEFT 4
#define SW_RIGHT 21

#define MODIFIER_NONE  0x00
#define MODIFIER_CTRL  0x01
#define MODIFIER_SHIFT 0x02
#define MODIFIER_ALT   0x04

using ctrl_cmd = uint8_t;

// Timer
hw_timer_t * timer = NULL;
// Queues
#define QUEUE_LENGTH 1
QueueHandle_t xQueue;
// Task Handler
TaskHandle_t taskHandle;
// BLE Media Controller
BleKeyboard bleKeyboard;

// Constant Parameter
const int dt_20ms = 20;

// Switch Control Class
template <typename ShortActionType, typename LongActionType>
class SwitchControl {
public:
    SwitchControl(int pin, int longTime, ShortActionType shortPressAction, LongActionType longPressAction, uint8_t short_mod, uint8_t long_mod) 
        : pin(pin), longTime(longTime), short_mod(short_mod), long_mod(long_mod){
        this->shortPressAction = shortPressAction;
        this->longPressAction = longPressAction;
        pinMode(pin, INPUT_PULLUP);
        switch_status = false;
        switch_status_k1 = false;
        timer_sw_on = 0;
        flag_enable_action = true;
    }

    void update() {
        switch_status = !digitalRead(pin);
        
        if (switch_status and !switch_status_k1) {
            timer_sw_on = 0;
            Serial.println("pressed");
        }
        
        if (switch_status) {
            timer_sw_on += dt_20ms;
            if((timer_sw_on > longTime) and flag_enable_action){
              // Long Press Action
              Serial.println("Long press action");
              send_cmd(longPressAction, long_mod);  // 再生/一時停止などのアクション
              // Disable action
              flag_enable_action = false;
              Serial.println("disabled");
            }
        }
        
        if (!switch_status and switch_status_k1) {
            if ((timer_sw_on < longTime) and flag_enable_action) {
              send_cmd(shortPressAction, short_mod);  // 次のトラックなどのアクション
              Serial.println("Short press action");
            }
            Serial.println("released");
            flag_enable_action = true;
            Serial.println("enabled");
        }

        switch_status_k1 = switch_status;
    }

private:
    int pin;
    int longTime;
    ShortActionType shortPressAction;
    LongActionType longPressAction;
    uint8_t short_mod;
    uint8_t long_mod;

    bool switch_status;
    bool switch_status_k1;
    bool flag_enable_action;

    int timer_sw_on;

    template <typename T>
    void send_cmd(T cmd, uint8_t mod){
      Serial.println(mod);
      if((mod&MODIFIER_CTRL) > 0x00){
        bleKeyboard.press(KEY_LEFT_CTRL);
      }
      if((mod&MODIFIER_SHIFT) > 0x00){
        bleKeyboard.press(KEY_LEFT_SHIFT);
        Serial.println("Shift Mod");
      }
      if((mod&MODIFIER_ALT) > 0x00){
        bleKeyboard.press(KEY_LEFT_ALT);
      }
      bleKeyboard.press(cmd);
      bleKeyboard.releaseAll();
    };
};

uint8_t key_next_truck[2] = {1, 0};
uint8_t key_previous_truck[2] = {2, 0};

// 各スイッチ用のインスタンス
SwitchControl<uint8_t, uint8_t> downSwitch(SW_DOWN, 500, KEY_DOWN_ARROW, KEY_DOWN_ARROW, MODIFIER_NONE, MODIFIER_NONE);
SwitchControl<uint8_t, uint8_t> upSwitch(SW_UP, 500, KEY_UP_ARROW, KEY_UP_ARROW, MODIFIER_NONE, MODIFIER_NONE);
SwitchControl<char, char> rightSwitch(SW_RIGHT, 500, 'l', 'n', MODIFIER_NONE, MODIFIER_SHIFT);
SwitchControl<char, char> leftSwitch(SW_LEFT, 500, 'j', 'p', MODIFIER_NONE, MODIFIER_SHIFT);

void IRAM_ATTR onTimer() {
    int8_t data;
    // Send Queue
    xQueueSendFromISR(xQueue, &data, 0);
}

void task(void *pvParameters) {
    while (1) {
        int8_t data;
        xQueueReceive(xQueue, &data, portMAX_DELAY);
        task_20ms();
    }
}

void task_20ms(void) {
    if (bleKeyboard.isConnected()) {
        digitalWrite(LED_RED, HIGH);

        // 各スイッチの状態を更新
        downSwitch.update();
        upSwitch.update();
        leftSwitch.update();
        rightSwitch.update();

    } else {
        digitalWrite(LED_RED, LOW);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    bleKeyboard.begin();
    pinMode(LED_RED, OUTPUT);

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
    
    // Timer setup
    timer = timerBegin(0, 80, true); // 80 is for 1 us per tick
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 20000, true);
    timerAlarmEnable(timer);
}

void loop() {
    delay(1);
}
