#include "Logger.h"

std::atomic<bool> Logger::enabled;
SemaphoreHandle_t Logger::loggerMutex = xSemaphoreCreateMutex();