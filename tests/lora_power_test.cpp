// Exercise the actual driver methods with a register-level I2C fake.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#define ESP_LOGW(...) (++warnings)
constexpr int ESP_OK = 0, GPIO_MODE_OUTPUT = 1;
using gpio_num_t = int;
using i2c_master_dev_handle_t = void *;
uint8_t regs[8];
int calls, fail_at, warnings, delay_ms;
bool cs_high[48], cs_output[48];
void gpio_set_level(int pin, int value) { cs_high[pin] = value; }
void gpio_set_direction(int pin, int mode) { cs_output[pin] = mode == GPIO_MODE_OUTPUT; }
int pdMS_TO_TICKS(int ms) { return ms; }
void vTaskDelay(int ms) { delay_ms += ms; }
int i2c_master_transmit_receive(void *, uint8_t *reg, int n, uint8_t *value, int size, int timeout) {
  assert(n == 1 && size == 1 && timeout == 50);
  assert(*reg == 2 || *reg == 6);
  if (++calls == fail_at) return -1;
  *value = regs[*reg];
  return ESP_OK;
}
int i2c_master_transmit(void *, uint8_t *data, int n, int timeout) {
  assert(n == 2 && timeout == 50);
  assert(data[0] == 2 || data[0] == 6);
  if (++calls == fail_at) return -1;
  if (data[0] == 6 && !(data[1] & 1) && (regs[2] & 1))
    assert(cs_high[12] && cs_high[46] && cs_output[12] && cs_output[46]);
  regs[data[0]] = data[1];
  return ESP_OK;
}
struct Panel {
  void *pca_ = (void *) 1;
  bool lora_enabled_ = false;
  // INJECT_METHODS
};
void reset() {
  const uint8_t initial[] = {0x5A, 0xA5, 0xAA, 0x35, 0x96, 0x69, 0xFF, 0xC0};
  std::memcpy(regs, initial, sizeof(regs));
  calls = fail_at = warnings = delay_ms = 0;
  std::memset(cs_high, 0, sizeof(cs_high));
  std::memset(cs_output, 0, sizeof(cs_output));
}
void check_unrelated() {
  assert((regs[2] & 0xFE) == 0xAA && (regs[6] & 0xFE) == 0xFE);
  assert(regs[0] == 0x5A && regs[1] == 0xA5 && regs[3] == 0x35);
  assert(regs[4] == 0x96 && regs[5] == 0x69 && regs[7] == 0xC0);
}
int main() {
  Panel p;
  reset();
  // Disabled must also shut down a rail left enabled across an ESP-only reset.
  regs[2] = 0xAB; regs[6] = 0xFE;
  p.setup_lora_power_();
  assert(regs[2] == 0xAA && regs[6] == 0xFE && !warnings && !delay_ms);
  assert(!cs_output[12] && !cs_output[46]);
  check_unrelated();
  reset(); p.lora_enabled_ = true;
  p.setup_lora_power_();
  assert(regs[2] == 0xAB && regs[6] == 0xFE && !warnings && delay_ms >= 1500);
  check_unrelated();
  for (bool enabled : {false, true}) {
    p.lora_enabled_ = enabled;
    for (int failure = 1; failure <= 4; ++failure) {
      reset(); fail_at = failure;
      p.setup_lora_power_();
      assert(calls == failure && warnings == 1 && !delay_ms);
      check_unrelated();
    }
    reset(); p.pca_ = nullptr;
    p.setup_lora_power_();
    assert(!calls && warnings == 1 && !delay_ms);
    check_unrelated(); p.pca_ = (void *) 1;
  }
  std::cout << "PASS: LoRa power, CS sequencing, register preservation and bounded I2C failures\n";
}
