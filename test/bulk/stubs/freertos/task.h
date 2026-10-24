#pragma once
int xTaskCreate(void (*entry)(void *), const char *, unsigned, void *, unsigned, void *);
void vTaskDelay(unsigned);
