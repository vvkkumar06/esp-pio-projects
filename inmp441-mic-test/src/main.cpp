#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_WS 5
#define I2S_SD 19
#define I2S_SCK 18
#define I2S_PORT I2S_NUM_0

void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1, // not used
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  i2s_install();
  i2s_setpin();
  Serial.println("INMP441 mic test started...");
}

void loop() {
  int32_t sample = 0;
  size_t bytes_read;
  i2s_read(I2S_PORT, (char*)&sample, sizeof(sample), &bytes_read, portMAX_DELAY);

  // INMP441 outputs 24-bit data in 32-bit container
  sample >>= 8;

  Serial.println(sample);  // print raw waveform values
}
