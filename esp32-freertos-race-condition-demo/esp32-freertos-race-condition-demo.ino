// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Globals
static int shared_var = 0;

// Increment shared variable the wrong way
void incTask(void * parameters) {
  int local_var;

  // Loop forever
  while(1) {
    // Roundabout way to "shared_var++" randomly and poorly
    local_var = shared_var;
    local_var++;
    vTaskDelay(random(100, 500) / portTICK_PERIOD_MS);
    shared_var = local_var;

    // Printout the new shared_var
    Serial.println(shared_var);
  }
}

void setup() {
  // put your setup code here, to run once:

  randomSeed(analogRead(0));

  // Configure Serial
  Serial.begin(115200);

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  Serial.println();
  Serial.println("---- FreeRTOS Race Condition Demo ----");

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
