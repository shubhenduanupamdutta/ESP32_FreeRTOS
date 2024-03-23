// Use only one core for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Some random string to print
const char msg[] = "Chanin Kuli ki main kuli ki";

// Task handles
static TaskHandle_t task_1 = NULL;
static TaskHandle_t task_2 = NULL;

// ******************************************************************************************************************
// Tasks
// Task 1: Print to serial terminal with lower priority
void startTask1(void * parameter) {
  // Count number of characters in string
  int msg_len = strlen(msg);

  while(1) {
    Serial.println();
    for (int i = 0; i < msg_len; i++) {
      Serial.print(msg[i]);
    }
    Serial.println();

    // Wait
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  // Print to serial terminal
}


// Task 2: Print to serial terminal with higher priority
void startTask2(void * parameter) {
  while (1) {
    Serial.print("*");
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}


void setup() {
  // put your setup code here, to run once: Effectively main
  // Configure serial, go slow so that we can see preemption
  Serial.begin(300);

  // Wait a moment to start so we don't miss serial output
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("----FreeRTOS Task Demo----");

  // Print self priority
  Serial.print("Setup and loop task running on core: ");
  Serial.print(xPortGetCoreID());
  Serial.print(", with priority: ");
  Serial.println(uxTaskPriorityGet(NULL));

  // Task to run forever
  xTaskCreatePinnedToCore(startTask1, "Task 1", 1024, NULL, 1, &task_1, app_cpu);

  xTaskCreatePinnedToCore(startTask2, "Task 2", 1024, NULL, 2, &task_2, app_cpu);
                          
                          

}

void loop() {
  // put your main code here, to run repeatedly:

  // Suspend higher priority task for some intervals
  for (int i = 0; i < 10; i++) {
    vTaskSuspend(task_2);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    vTaskResume(task_2);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

  }

  // Delete the lower priority task
  if (task_1 != NULL) {
    vTaskDelete(task_1);
    task_1 = NULL;
  }

}
