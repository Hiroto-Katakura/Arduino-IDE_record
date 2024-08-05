#include <stdio.h>
#include <M5Core2.h> // M5Core2ライブラリのインクルード
#include <driver/i2s.h> // I2Sドライバのインクルード
#include "VButton.h" // VButtonライブラリのインクルード

#define SAMPLE_RATE 16000 // サンプリングレートを16000Hzに設定
#define CHUNK 1024 // I2Sバッファサイズを1024バイトに設定
#define PERIOD 10 // 録音時間を10秒に設定

#define CONFIG_I2S_LRCK_PIN 0 // I2Sクロックのピン番号を0に設定
#define CONFIG_I2S_DATA_IN_PIN 34 // I2Sデータ入力のピン番号を34に設定

// マイクの初期化関数
void InitMic() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM), // I2Sの動作モードを設定
        .sample_rate = SAMPLE_RATE, // サンプリングレートを設定
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 1サンプルあたりのビット数を設定
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // チャンネルフォーマットを設定
        .communication_format = I2S_COMM_FORMAT_I2S, // 通信フォーマットを設定
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // 割り込みの割り当てフラグを設定
        .dma_buf_count = 2, // DMAバッファの数を設定
        .dma_buf_len = CHUNK // 各DMAバッファの長さを設定
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL); // I2Sドライバのインストール

    i2s_pin_config_t tx_pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE, // I2Sのビットクロックピンを未使用に設定
        .ws_io_num = CONFIG_I2S_LRCK_PIN, // I2Sのワードセレクトピンを設定
        .data_out_num = I2S_PIN_NO_CHANGE, // I2Sのデータ出力ピンを未使用に設定
        .data_in_num = CONFIG_I2S_DATA_IN_PIN // I2Sのデータ入力ピンを設定
    };
    i2s_set_pin(I2S_NUM_0, &tx_pin_config); // I2Sピンの設定
}

// ボタンオブジェクトの宣言
VButton *button;

// ボタンが押されたときのコールバック関数
void button_callback(char *title, bool use_toggle, bool is_toggled) {
    Serial.println("Button Pressed. Starting recording...");
    File f = SD.open("/audio_record.dat", FILE_WRITE); // SDカードにファイルを開く
    if (f) {
        Serial.println("File opened successfully.");
        uint8_t buffer[CHUNK]; // バッファの宣言
        size_t byte_read; // 読み取ったバイト数を格納する変数

        // 録音時間に基づいて必要なチャンク数を計算
        int chunk_max = (int)((SAMPLE_RATE*16*1*PERIOD)/8/CHUNK);
        Serial.printf("Total chunks to record: %d\n", chunk_max);

        for(int i = 0; i < chunk_max; i++) {
            i2s_read(I2S_NUM_0, buffer, CHUNK, &byte_read, (100 / portTICK_RATE_MS)); // I2Sからデータを読み取る
            Serial.printf("Chunk %d read. Bytes read: %d\n", i + 1, byte_read); // シリアルモニタに読み取ったバイト数を表示
            f.write(buffer, CHUNK); // ファイルにバッファの内容を書き込む
            Serial.println("Chunk written to file.");
        }
        f.close(); // ファイルを閉じる
        Serial.println("Recording completed and file closed.");
    } else {
        Serial.println("Failed to open file for writing.");
    }
}

// 初期設定関数
void setup() {
    // スピーカーの初期化を無効にしてM5Core2を初期化
    M5.begin(true, true, true, true, kMBusModeOutput, false);
    Serial.begin(115200); // シリアルモニタの初期化
    M5.Lcd.setBrightness(200); // LCDの明るさを設定
    M5.Lcd.fillScreen(WHITE); // LCDを白で塗りつぶす

    button = new VButton("REC", button_callback); // "REC"という名前のボタンを作成し、コールバック関数を設定
    InitMic(); // マイクの初期化
}

// メインループ関数
void loop() {
    button->loop(); // ボタンの状態を監視
}
