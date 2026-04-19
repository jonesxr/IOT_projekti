#include "inmp441_sensor.h"
#include <driver/i2s.h>
#include "../config.h"

const i2s_port_t I2S_PORT = I2S_NUM_0;

bool initMicrophone() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("VIRHE: I2S ajurin asennus epaonnistui!");
    return false;
  }
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("VIRHE: I2S pinnien asettaminen epaonnistui!");
    return false;
  }
  
  Serial.println("I2S Mikrofoni (INMP441) alustettu.");
  return true;
}

int getMicrophoneVolume() {
  size_t bytes_read;
  int32_t samples[128]; // Pieni näytepuskuri nopeaan reaktioon
  
  // Käytetään 100ms timeoutia portMAX_DELAY sijaan, jotta mikrofoni ei voi jäädyttää ESP32:ta 
  // lopullisesti jos i2s-väylään tulee jokin häiriö tai se deaktivoituu odottamatta.
  esp_err_t result = i2s_read(I2S_PORT, &samples, sizeof(samples), &bytes_read, pdMS_TO_TICKS(100));
  if (result != ESP_OK) return 0;
  
  int samples_read = bytes_read / sizeof(int32_t);
  if (samples_read == 0) return 0;
  
  // Poistetaan DC-offset
  int64_t sum = 0;
  for (int i = 0; i < samples_read; i++) {
    int32_t val = samples[i] >> 14; 
    sum += val;
  }
  int32_t mean = sum / samples_read;

  // RMS laskenta (Root Mean Square) = äänen sähköteho / voimakkuus
  int64_t sum_squares = 0;
  for (int i = 0; i < samples_read; i++) {
    int32_t val = (samples[i] >> 14) - mean;
    sum_squares += (val * val);
  }
  
  return sqrt(sum_squares / samples_read);
}
