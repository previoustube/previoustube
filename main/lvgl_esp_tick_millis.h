#include <esp_timer.h>

inline static uint32_t millis()
{
  return esp_timer_get_time() / 1000LL;
}
