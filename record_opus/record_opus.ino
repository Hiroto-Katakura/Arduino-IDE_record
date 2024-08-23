#include <stdio.h>  // 標準入出力ライブラリをインクルード
#include <M5Core2.h>  // M5Core2のライブラリをインクルード
#include <driver/i2s.h>  // I2S（インターICサウンド）ドライバをインクルード
#include "VButton.h"  // ボタン管理ライブラリをインクルード
#include <SD.h>  // SDカードライブラリをインクルード
#include <opus.h>  // Opusコーデックライブラリをインクルード

#define SAMPLE_RATE 8000  // サンプルレートを8kHzに設定
#define FRAME_SIZE 20  // 2.5msに相当するフレームサイズ（20サンプル）を定義
#define PERIOD 10  // 録音期間の設定（秒）

#define CONFIG_I2S_LRCK_PIN 0  // I2Sのワードセレクト（LRCK）ピン番号を0に設定
#define CONFIG_I2S_DATA_IN_PIN 34  // I2Sのデータ入力ピン番号を34に設定

OpusEncoder *opusEncoder;  // Opusエンコーダのポインタを宣言
uint8_t *opusBuffer;  // Opusエンコーディングの結果を格納するバッファを宣言
int opusBufferSize;  // Opusバッファのサイズを保持する変数を宣言

VButton *button;  // ボタン管理用のVButtonオブジェクトを宣言

void InitMic() {  // マイクの初期化関数
    i2s_config_t i2s_config = {  // I2Sの設定構造体を初期化
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),  // I2Sをマスター・受信・PDMモードで動作設定
        .sample_rate = SAMPLE_RATE,  // サンプルレートを設定
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // サンプルのビット深度を16ビットに設定
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  // 右チャンネルのみを使用するフォーマットに設定
        .communication_format = I2S_COMM_FORMAT_I2S,  // I2S通信フォーマットに設定
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,  // 割り込みフラグをレベル1に設定
        .dma_buf_count = 8,  // DMAバッファの数を8に設定
        .dma_buf_len = FRAME_SIZE  // DMAバッファの長さをフレームサイズ（240サンプル）に設定
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);  // I2Sドライバをインストール

    i2s_pin_config_t tx_pin_config = {  // I2Sのピン設定構造体を初期化
        .bck_io_num = I2S_PIN_NO_CHANGE,  // BCKピンは変更しない
        .ws_io_num = CONFIG_I2S_LRCK_PIN,  // LRCKピンを設定
        .data_out_num = I2S_PIN_NO_CHANGE,  // データ出力ピンは変更しない
        .data_in_num = CONFIG_I2S_DATA_IN_PIN  // データ入力ピンを設定
    };
    i2s_set_pin(I2S_NUM_0, &tx_pin_config);  // I2Sのピン設定を適用
}

void InitOpus() {  // Opusエンコーダの初期化関数
    int error;  // エラーチェック用の変数を宣言
    opusEncoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_AUDIO, &error);  // Opusエンコーダを作成
    if (error != OPUS_OK) {  // エラーチェック
        Serial.printf("Failed to create Opus encoder: %d\n", error);  // エラーがあればメッセージを出力
        return;  // エラー時は関数を終了
    }

    error = opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(16000));  // Opusのビットレートを設定
    if (error != OPUS_OK) {  // エラーチェック
        Serial.printf("Failed to set bitrate: %d\n", error);  // エラーがあればメッセージを出力
        return;  // エラー時は関数を終了
    }
    
    opusBufferSize = FRAME_SIZE * 2;  // Opusバッファサイズを設定（フレームサイズに応じて2倍のサイズ）
    opusBuffer = (uint8_t*)malloc(opusBufferSize);  // バッファメモリを確保
}

void button_callback(char *title, bool use_toggle, bool is_toggled) {  // ボタン押下時のコールバック関数
    File f = SD.open("/audio_record.opus", FILE_WRITE);  // SDカードにファイルを作成
    if (!f) {  // ファイル作成のエラーチェック
        Serial.println("Failed to open file for writing.");  // エラーがあればメッセージを出力
        return;  // エラー時は関数を終了
    }

    uint8_t buffer[FRAME_SIZE * 2];  // PCMデータ格納用バッファをフレームサイズに基づいて確保
    size_t byte_read;  // 読み取ったバイト数を格納する変数

    int chunk_max = (int)((SAMPLE_RATE * 16 * 1 * PERIOD) / 8 / FRAME_SIZE);  // 録音するチャンクの最大数を計算

    for (int i = 0; i < chunk_max; i++) {  // チャンクごとに録音とエンコードを繰り返す
        esp_err_t result = i2s_read(I2S_NUM_0, buffer, FRAME_SIZE * 2, &byte_read, (100 / portTICK_RATE_MS));  // I2Sからデータを読み取る
        if (result != ESP_OK || byte_read == 0) {  // 読み取りエラーチェック
            Serial.println("I2S read failed or no data read.");  // エラーがあればメッセージを出力
            continue;  // エラー時は次のループへ進む
        }
        Serial.printf(".");  // 成功した場合に進捗を表示

        const opus_int16 *pcmData = reinterpret_cast<const opus_int16 *>(buffer);  // PCMデータを16ビット形式にキャスト
        int opusBytes = opus_encode(opusEncoder, pcmData, FRAME_SIZE, opusBuffer, opusBufferSize);  // PCMデータをOpus形式にエンコード
        if (opusBytes < 0) {  // エンコードエラーチェック
            Serial.printf("Opus encoding failed: %d\n", opusBytes);  // エラーがあればメッセージを出力
        } else {
            f.write(opusBuffer, opusBytes);  // 成功した場合、エンコードデータをファイルに書き込む
        }
    }
    f.close();  // ファイルを閉じる
    Serial.println("Recording completed and file closed.");  // 録音終了を通知
}

void setup() {  // 初期設定関数
    M5.begin(true, true, true, true, kMBusModeOutput, false);  // M5Core2の初期化
    Serial.begin(115200);  // シリアル通信を開始

    if (!SD.begin()) {  // SDカードの初期化
        Serial.println("SD card initialization failed!");  // 初期化失敗時のエラーメッセージ
        return;  // エラー時は関数を終了
    }

    M5.Lcd.setBrightness(200);  // LCDの明るさを設定
    M5.Lcd.fillScreen(WHITE);  // LCD画面を白で塗りつぶす

    button = new VButton("REC", button_callback);  // ボタンオブジェクトを作成し、コールバック関数を設定
    InitMic();  // マイクの初期化を実行
    InitOpus();  // Opusエンコーダの初期化を実行
}

void loop() {  // メインループ関数
    button->loop();  // ボタンの状態を監視し、押された場合の処理を行う
}
