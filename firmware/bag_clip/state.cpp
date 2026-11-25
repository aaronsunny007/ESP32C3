#include "state.h"

static AppState appState;
static SemaphoreHandle_t mtx;
static QueueHandle_t cmdQueue, netQueue;

void stateBegin() {
  mtx = xSemaphoreCreateMutex();
  cmdQueue = xQueueCreate(8, sizeof(Command));
  netQueue = xQueueCreate(8, sizeof(Command));
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

void stateToast(const char *msg) {
  stateLock();
  strlcpy(appState.toast, msg, sizeof(appState.toast));
  appState.toastUntil = millis() + 2500;
  stateUnlock();
}

void toDisplayable(const char *in, char *out, size_t outLen) {
  size_t o = 0;
  if (!in || outLen == 0) return;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < outLen; p++) {
    if (*p < 0x80) out[o++] = (char)*p;
    else if ((*p & 0xC0) == 0xC0) out[o++] = '?';   // first byte of a multi-byte character
    // continuation bytes (10xxxxxx) are skipped
  }
  out[o] = 0;
}

void sendCommand(Command c) { xQueueSend(cmdQueue, &c, 0); }
bool receiveCommand(Command &c, uint32_t waitMs) {
  return xQueueReceive(cmdQueue, &c, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

void sendNetCommand(Command c) { xQueueSend(netQueue, &c, 0); }
bool receiveNetCommand(Command &c, uint32_t waitMs) {
  return xQueueReceive(netQueue, &c, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
