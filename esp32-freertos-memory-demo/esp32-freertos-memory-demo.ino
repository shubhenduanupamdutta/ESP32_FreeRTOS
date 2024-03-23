// Use only one core for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0
#else
  static const BaseType_t app_cpu = 1
#endif

// Task: perform some mundane task
void testTask(void * parameter) {
  while (1) {
    int a = 1;
    int b[100];

    // Do something with array so that it doesn't get optimized out by the compiler
    for (int i = 0; i < 100; i++) {
      b[i] = a + 1;
    }

    Serial.println(b[0]);
  }
}

void setup() {
  // put your setup code here, to run once:

  // Configure Serial
  Serial.begin(115200);
  // Wait a moment to start, so that we don't miss the serial output
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println():
  Serial.println("---- FreeRTOS Memory Demo ----");

  // Start the only other task
  xTaskCreatePinnedToCore(testTask, "Test Task", 1024, NULL, 1, NULL, app_cpu);

  // Delete setup and loop task to make sure that only one task is running
  vTaskDelete(NULL);

}

void loop() {
  // put your main code here, to run repeatedly:

}