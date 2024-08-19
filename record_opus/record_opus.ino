#include <stdio.h>
#include <M5Core2.h>
#include <driver/i2s.h>
#include "VButton.h"
#include <SD.h>
#include <opus.h>

#define SAMPLE_RATE 16000
#define CHUNK 1024
#define PERIOD 10

#define CONFIG_I2S_LRCK_PIN 0
#define CONFIG_I2S_DATA_IN_PIN 34

OpusEncoder *opusEncoder;
uint8_t *opusBuffer;
int opusBufferSize;

VButton *button;

void InitMic() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = CHUNK
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

    i2s_pin_config_t tx_pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = CONFIG_I2S_LRCK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = CONFIG_I2S_DATA_IN_PIN
    };
    i2s_set_pin(I2S_NUM_0, &tx_pin_config);
}

void InitOpus() {
    int error;
    opusEncoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK) {
        Serial.printf("Failed to create Opus encoder: %d\n", error);
        return;
    }
    // ビットレート設定のエラーチェック
    error = opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(16000));
    if (error != OPUS_OK) {
        Serial.printf("Failed to set bitrate: %d\n", error);
        return;
    }
    
    opusBufferSize = CHUNK; 
    opusBuffer = (uint8_t*)malloc(opusBufferSize);
}

void button_callback(char *title, bool use_toggle, bool is_toggled) {
    File f = SD.open("/audio_record.opus", FILE_WRITE);
    if (!f) {
        Serial.println("Failed to open file for writing.");
        return;
    }

    uint8_t buffer[CHUNK];
    size_t byte_read;

    int chunk_max = (int)((SAMPLE_RATE * 16 * 1 * PERIOD) / 8 / CHUNK);

    for (int i = 0; i < chunk_max; i++) {
        esp_err_t result = i2s_read(I2S_NUM_0, buffer, CHUNK, &byte_read, (100 / portTICK_RATE_MS));
        if (result != ESP_OK || byte_read == 0) {
            Serial.println("I2S read failed or no data read.");
            continue;
        }
        Serial.printf(".");

        // 16ビットPCMデータをキャスト
        const opus_int16 *pcmData = reinterpret_cast<const opus_int16 *>(buffer);
        int numSamples = byte_read / sizeof(opus_int16); // サンプル数の計算
        Serial.printf("Number of samples (numSamples): %d\n", numSamples);


        // PCMデータの最初の数値を出力（デバッグ用）
        Serial.printf("PCM Data (16-bit): %d\n", pcmData[0]);

        int opusBytes = opus_encode(opusEncoder, pcmData, numSamples, opusBuffer, opusBufferSize);
        if (opusBytes < 0) {
            Serial.printf("Opus encoding failed: %d\n", opusBytes);
        } else {
            f.write(opusBuffer, opusBytes);
        }
    }
    f.close();
    Serial.println("Recording completed and file closed.");
}

void setup() {
    M5.begin(true, true, true, true, kMBusModeOutput, false);
    Serial.begin(115200);

    if (!SD.begin()) {
        Serial.println("SD card initialization failed!");
        return;
    }

    M5.Lcd.setBrightness(200);
    M5.Lcd.fillScreen(WHITE);

    button = new VButton("REC", button_callback);
    InitMic();
    InitOpus();
}

void loop() {
    button->loop();
}
