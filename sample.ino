#include <M5Core2.h>

//グローバル変数
uint8_t microphonedata0[1024 * 80];     //microphonedata0：録音データを格納するためのバッファ
int data_offset = 0;                   //data_offset：データのオフセットを管理する変数

//DisplayInit関数
void DisplayInit(void) {       // Initialize the display. (ディスプレイの初期化)
    M5.Lcd.fillScreen(WHITE);  // Set the screen background color to white.
    M5.Lcd.setTextColor(
        BLACK);  // Set the text color to black.  
    M5.Lcd.setTextSize(2);  // Set font size to 2.
}

/* M5Core2が起動またはリセットされるとsetUp()関数のプログラムが一度だけ実行される */
void setup() {
    M5.begin(true, true, true, true, kMBusModeOutput,
             true);             // M5CORE2の初期化
    M5.Axp.SetSpkEnable(true);  // スピーカーの有効化
    DisplayInit();              //DisplayInit関数の呼び出し
    M5.Lcd.setTextColor(RED);   //テキストカラーを赤に設定
    M5.Lcd.setCursor(10,
                     10);  // ディスプレイにテキストを表示する際のカーソル位置を設定するための関数
    M5.Lcd.printf("Recorder!");  // ディスプレイに表示
 
    M5.Lcd.setTextColor(BLACK);  //
    M5.Lcd.setCursor(10, 26);
    M5.Lcd.printf("Press Left Button to recording!");
    delay(100);  // delay 100ms.  
}

/* setup()内のプログラムが実行された後、loop()内のプログラムが実行される
loop()関数は、プログラムが繰り返し実行される無限ループ*/
void loop() {
    TouchPoint_t pos =
        M5.Touch.getPressPoint();  // タッチ位置の取得
    if (pos.y > 240) {
        if (pos.x < 109) {
            M5.Axp.SetVibration(true);  // 振動モータをON
            delay(100);
            M5.Axp.SetVibration(false); //// 振動モータをOFF
            data_offset = 0; //録音データを格納するためのバッファのオフセットをリセットする。録音データの格納がバッファの先頭から開始される
            M5.Spk.InitI2SSpeakOrMic(MODE_MIC); //M5Core2のスピーカーをマイクモード (MODE_MIC) に設定。これにより、マイクからの音声データを取得
            size_t byte_read;  //マイクから読み取ったバイト数を格納するための変数を宣言
            while (1) {  //録音プロセスを継続するためのループ処理
                /*i2s_read 関数を使用して、マイクから音声データを読み取る
                  Speak_I2S_NUMBER: I2Sデバイス番号を指定します。
                  (char *)(microphonedata0 + data_offset): データを格納するバッファのアドレスを指定
                  DATA_SIZE: 読み取るデータのサイズを指定
                  &byte_read: 読み取ったデータのバイト数を格納する変数のアドレスを指定
                  (100 / portTICK_RATE_MS): 読み取りタイムアウトを指定します（100ms）*/
                i2s_read(Speak_I2S_NUMBER,  
                         (char *)(microphonedata0 + data_offset), DATA_SIZE,
                         &byte_read, (100 / portTICK_RATE_MS));
                data_offset += 1024;  //データオフセットを更新し、次の読み取り位置を設定
                if (data_offset == 1024 * 80 || M5.Touch.ispressed() != true) //バッファの最大サイズ（1024 * 80）に達するか、タッチが解除されるとループを終了
                    break;
            }
            size_t bytes_written;  //スピーカーに書き込まれたバイト数を格納するための変数
            M5.Spk.InitI2SSpeakOrMic(MODE_SPK);  //M5Core2のスピーカーをスピーカーモード（MODE_SPK）に設定
            /*i2s_write 関数を使用して、録音された音声データをスピーカーに送信
              Speak_I2S_NUMBER: I2Sデバイス番号を指定する。これにより、どのI2Sデバイスにデータを送信するかが決まる
              microphonedata0: 録音データが格納されているバッファのアドレスを指定
              data_offset: 録音データのバイト数を指定します。このサイズのデータがバッファからスピーカーに送信される
              &bytes_written: 実際に書き込まれたバイト数を格納する変数のアドレスを指定します。書き込み操作が終了した後、この変数には書き込まれたバイト数が格納される
              portMAX_DELAY: この引数は、書き込み操作が完了するまで最大限に待機することを指定します。*/
            i2s_write(Speak_I2S_NUMBER, microphonedata0, data_offset,
                      &bytes_written, portMAX_DELAY);
        }
    }
}
