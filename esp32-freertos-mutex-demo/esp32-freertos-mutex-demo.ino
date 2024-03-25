/******************************************************************************************
* Extension from Race-condiiton demo and implement mutex
******************************************************************************************/

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Globals
static int shared_var = 0;
static SemaphoreHandle_t mutex;  // FreeRTOS Generatlizes Semaphores and Mutex as just Semaphores

// Increment shared variable the wrong way
void incTask(void * parameters) {
  int local_var;

  // Loop forever
  while(1) {

    // Take mutex prior to critical section
    if (xSemaphoreTake(mutex, 0) == pdTRUE) {
      
      // Roundabout way to "shared_var++" randomly and poorly
      local_var = shared_var;
      local_var++;
      vTaskDelay(random(100, 500) / portTICK_PERIOD_MS);
      shared_var = local_var;

      // Return Mutex after critical section
      xSemaphoreGive(mutex);

      // Printout the new shared_var
      Serial.println(shared_var);
    }
    else {
      // Do something if you can't get the mutex
    }
  }
}

void setup() {
  // put your setup code here, to run once:

  randomSeed(analogRead(0));

  // Configure Serial
  Serial.begin(115200);

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  Serial.println();
  Serial.println("---- FreeRTOS Mutex Demo ----");

  // Create Mutex before tasks
  mutex = xSemaphoreCreateMutex();

  // Start Task 1
  xTaskCreatePinnedToCore(incTask, "Increment Task 1", 1024, NULL, 1, NULL, app_cpu);

  // Start Task 2
  xTaskCreatePinnedToCore(incTask, "Increment Task 2", 1024, NULL, 1, NULL, app_cpu);

  // Delete setup and loop tasks
  vTaskDelete(NULL);

}

void loop() {
  // put your main code here, to run repeatedly:

}
