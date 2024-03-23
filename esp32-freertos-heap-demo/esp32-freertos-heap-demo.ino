// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Settings
static const uint8_t buf_len = 255;

// Globals
static char *msg_ptr = NULL;
static volatile uint8_t msg_flag = 0;

//**********************************************************************************************
// Tasks

// Task 1: Read message from Serial Buffer
void readSerial(void *parameter) {
  char c;
  char buf[buf_len];
  uint8_t idx = 0;

  // Clear whole buffer
  memset(buf, 0, buf_len);

  // Loop forever
  while (1) {
    // Read characters from serial
    if (Serial.available() > 0) {
      c = Serial.read();

      // Store read character into buffer if not over buffer limit
      if (idx < buf_len - 1) {
        buf[idx++] = c;
      }

      if (c == '\n') {
        // The last character in string is \n, replace it with \0 for null character
        buf[idx - 1] = '\0';

        // Try to allocate memory and copy over message, if msg buffer is still in use, ignore the entire message
        if (msg_flag == 0) {
          msg_ptr = (char *)pvPortMalloc(idx * sizeof(char));

          // if malloc returns 0 out of memory, throw an error and reset
          configASSERT(msg_ptr);

          // Copy to msg_ptr
          memcpy(msg_ptr, buf, idx);

          // Notify other task that message is ready
          msg_flag = 1;
        }

        // Reset receive buffer and index counter
        memset(buf, 0, buf_len);
        idx = 0;
      }
    }
  }
}

// Task 2: print whenever flag is set and free buffer
void printMessage(void *parameter) {
  while(1) {
    // Wait for flag to be set and take action
    if (msg_flag == 1) {
      Serial.println(msg_ptr);

      // Give amount of heap memory free
      Serial.print("Free heap (bytes): ");
      Serial.println(xPortGetFreeHeapSize());

      // Free buffer, set pointer to null and free flag
      vPortFree(msg_ptr);
      msg_ptr = NULL;
      msg_flag = 0;

    }
  }
}

void setup() {
  // put your setup code here, to run once:

  // Configure Serial
  Serial.begin(115200);

  // Wait a moment to start
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  Serial.println();
  Serial.println("---- FreeRTOS HEAP Demo ----");
  Serial.println("Enter a string: ");

  // Start serial receive task
  xTaskCreatePinnedToCore(readSerial, "Read Serial", 1024, NULL, 1, NULL, app_cpu);

  // Start Serial print task
  xTaskCreatePinnedToCore(printMessage, "Print Message", 1024, NULL, 1, NULL, app_cpu);

  // Delete setup and loop tasks
  vTaskDelete(NULL);
}

void loop() {
  // put your main code here, to run repeatedly:

}
