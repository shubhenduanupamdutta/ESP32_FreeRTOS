// Use only one core for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
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

    // Print out remaining stack memory
    Serial.print("High water mark (words): ");
    Serial.println(uxTaskGetStackHighWaterMark(NULL));

    // Print out number of free heap memory bytes before malloc
    Serial.print("Heap before malloc (bytes): ");
    Serial.println(xPortGetFreeHeapSize());

    // Allocating some memory on heap
    int *ptr = (int *)pvPortMalloc(1024 * sizeof(int));

    // // do something with memeory so compiler doesn't optimize it out
    // for (int i = 0; i < 1024; i++) {
    //   ptr[i] = 3;
    // }

    // One way to check heap overflow is to check the malloc output
    if (ptr == NULL) {
      Serial.println("Not enough heap");
    } else {
      for (int i = 0; i < 1024; i++) {
        ptr[i] = 3;
      }
    }

    // Print out number of free heap memory bytes after malloc
    Serial.print("Heap after malloc (bytes): ");
    Serial.println(xPortGetFreeHeapSize());

    vPortFree(ptr);

    // Wait for a while
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // put your setup code here, to run once:

  // Configure Serial
  Serial.begin(115200);
  // Wait a moment to start, so that we don't miss the serial output
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("---- FreeRTOS Memory Demo ----");

  // Start the only other task
  xTaskCreatePinnedToCore(testTask, "Test Task", 1500, NULL, 1, NULL, app_cpu);
  // Stack size is changed to 1500 to accomodate all the variables specially the integer array

  // Delete setup and loop task to make sure that only one task is running
  vTaskDelete(NULL);

}

void loop() {
  // put your main code here, to run repeatedly:

}
