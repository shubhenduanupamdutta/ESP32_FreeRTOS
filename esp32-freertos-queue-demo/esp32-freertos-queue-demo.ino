// Use only one core for demo purpose
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Settings
static const uint8_t msg_queue_len = 5;

// Globals
static QueueHandle_t msg_queue;

//********************************************************************************************************************************************************
// Tasks

// Task 1: Wait for item in a queue and print it
void printMessages(void *parameters) {
  int item;

  while (1) {
    // See if there's a message in the queue (do not block)
    if(xQueueReceive(msg_queue, (void *)&item, 0) == pdTRUE) {
      Serial.println(item);
    }
    // Wait before trying again
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}


void setup() {
  // put your setup code here, to run once:

  // Configure Serial
  Serial.begin(115200);

  // Wait a moment to start
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  Serial.println();
  Serial.println("---- FreeRTOS Queue Demo ----");

  // Create queue
  msg_queue = xQueueCreate(msg_queue_len, sizeof(int));

  // Start print task on a core
  xTaskCreatePinnedToCore(printMessages, "Print Messages", 1024, NULL, 1, NULL, app_cpu);

}

void loop() {
  // put your main code here, to run repeatedly:

  static int num = 0;

  // Try to add item for 10 ticks, fail if queue is full
  if (xQueueSend(msg_queue, (void *)&num, 10) != pdTRUE) {
    Serial.println("Queue full");
  }

  num++;

  // Wait before loop
  vTaskDelay(500 / portTICK_PERIOD_MS);

}
