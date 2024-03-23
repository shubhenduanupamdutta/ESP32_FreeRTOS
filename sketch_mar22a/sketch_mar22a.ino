// Using only one core for demo purposes


#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

// Pins
static const int led_pin = LED_BUILTIN;


// Our task: blink an LED
void toggleLED(void * parameter) {
  while(1) {
    digitalWrite(led_pin, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(led_pin, LOW);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(led_pin, OUTPUT);

  // Task to run forver, telling the scheduler I want to run it on one of the cores
  xTaskCreatePinnedToCore(   // Use xTaskCreate() in vanilla FreeRTOS
    toggleLED,        // Function to be called
    "Toggle LED",     // Name of the task
    1024,             // Stack size, bytes in ESP32, words in FreeRTOS
    NULL,             // Paramter to pass to the function
    1,                // Task priority, higher the number higher the priority (0 to configMAX_PRIORITY)
    NULL,             // Task Handle
    app_cpu           // Run on one core for demo purposes (ESP32 only)

  );

}

void loop() {
  // put your main code here, to run repeatedly:

}
