#include "state.h"

static AppState appState;
static SemaphoreHandle_t mtx;
static QueueHandle_t cmdQueue;

void stateBegin() {
  mtx = xSemaphoreCreateMutex();
  cmdQueue = xQueueCreate(8, sizeof(Command));
}

void stateLock()   { xSemaphoreTake(mtx, portMAX_DELAY); }
void stateUnlock() { xSemaphoreGive(mtx); }
AppState &stateRef() { return appState; }

AppState stateGet() {
  stateLock();
  AppState copy = appState;
  stateUnlock();
  return copy;
}

void sendCommand(Command c) { xQueueSend(cmdQueue, &c, 0); }

bool receiveCommand(Command &c, uint32_t waitMs) {
  return xQueueReceive(cmdQueue, &c, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
