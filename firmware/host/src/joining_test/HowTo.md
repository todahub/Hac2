# HowTo: 輪唱結合テストの実行手順と配線

このドキュメントは、ホスト機（`joining_test.ino`）、クライアント機（[client.ino](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/client.ino)）、および音波生成プログラム（[gakuhusaisei.pde](file:///c:/Users/yuzu6/source/repos/Hac2/processing/src/single_test/gakuhusaisei.pde)）を用いた輪唱結合テストの配線と実行手順について記述しています。

---

## 1. 概要と動作仕様

このテストでは、起動と同時にホスト機が自動的にビートパルスを発信します（再生開始・停止ボタンは使用しません）。

### 送信IDのシーケンス（クライアント台数 $N$ のとき）
* **ID 1 〜 $N-1$**: 各IDを **8回** ずつ送信し、次のIDへ移行します。
* **ID $N$**: 以後は ID $N$ を維持します。
* **停止ボタン押下後**: ホストは電圧出力とビート生成を停止します。

> [!NOTE]
> テスト版スケッチでは、クライアント台数定数は `NUM_CLIENTS = 2` に設定されています。
> そのため、通常時は **「ID 1 を8回送信 ➔ ID 2 を8回送信 ➔ ID 2 を維持」** という動作になります。停止ボタンを押すとホストは電圧出力を止め、DAC は 0V に戻ります。本番環境（4台）では定数を `4` に変更してください。

---

## 2. 配線手順

### 使用する主要パーツ
* ホスト機: Arduino UNO R4 WiFi × 1
* クライアント機: Arduino × 2 （テスト対象）
* 可変抵抗（ポテンショメータ）10kΩ × 1
* クライアント機ごとのID検出・比較器回路（コンパレータなど）
* PC（Processing実行用）

### 配線図
```text
  [ホスト (UNO R4)]
    5V  ----------------- 可変抵抗 端子A
    A1  ----------------- 可変抵抗 ワイパー (BPM調整)
    GND ----------------- 可変抵抗 端子B & 全体共通GND
    DAC (A0) -----------+ (出力電圧バス)
                        |
                        +---> [Client 1 比較器入力] ===(デジタル出力)===> Client 1 D1 (PIN_SYNC_IN)
                        |
                        +---> [Client 2 比較器入力] ===(デジタル出力)===> Client 2 D1 (PIN_SYNC_IN)
```

1. **共通電源とGND**: ホスト機とすべてのクライアント機の GND を必ず共通化してください。
2. **可変抵抗**:
   * 端子の片側を `5V`、もう片側を `GND`、中央のワイパーピンをホスト機の `A1` に接続します。
3. **アナログバス配線**:
   * ホスト機の `DAC` ピン（Arduino UNO R4 WiFiではアナログピン `A0` に相当）を出力バスとして各クライアントのID検出回路（比較器等）の入力に接続します。
4. **クライアント入力**:
   * 各クライアント機の [config.h](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/config.h) に定義された `PIN_SYNC_IN`（デフォルトは `1`）に、対応する比較器のデジタル出力を接続します。
5. **タクトスイッチ**:
   * START スイッチはホスト機の `D4` と `GND` の間に接続します。
   * STOP スイッチはホスト機の `D5` と `GND` の間に接続します。
   * ホスト側スケッチは `INPUT_PULLUP` を使うため、外付けのプルアップ抵抗やプルダウン抵抗は不要です。
   * 未押下時は `HIGH`、押下時は `LOW` になります。

---

## 3. テストの準備と実行手順

### ステップ1: クライアント機の準備
1. クライアント機 1 に [client.ino](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/client.ino) を書き込みます。
   * [config.h](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/config.h) の `CLIENT_ID` を `1` に設定します。
2. クライアント機 2 に同様に [client.ino](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/client.ino) を書き込みます。
   * [config.h](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/config.h) の `CLIENT_ID` を `2` に設定します。

### ステップ2: ホスト機の書き込み
1. ホスト機（Arduino UNO R4 WiFi）に [joining_test.ino](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/host/src/joining_test/joining_test.ino) を書き込みます。
2. 書き込み完了と同時に、自動的にシリアルモニター（115200 bps）にログが出力され、DACからパルスが出力され始めます。
3. 可変抵抗を回して、BPM が変化すること、および発信されるIDが「1（8回）➔ 2（8回）➔ 3（8回）➔ 4（8回）➔ 4...」と推移することを確認します。

### ステップ3: Processingの実行
1. PCにクライアント機をUSB接続します。
2. Processing を起動し、[gakuhusaisei.pde](file:///c:/Users/yuzu6/source/repos/Hac2/processing/src/single_test/gakuhusaisei.pde) を開きます。
3. スケッチ内のシリアルポート番号（`Serial.list()[3]` など）を、接続したクライアント機のポート番号に合わせて実行します。

---

## 4. 期待されるテスト結果の検証

1. **ホスト機**: 可変抵抗を回すとBPMがリアルタイムで変化し、設定された周期でID電圧が切り替わります。
2. **クライアント 1 (`CLIENT_ID = 1`)**:
   * 起動遅延 `ROUND_DELAY_BEATS` が `(1 - 1) * 4 = 0` であるため、ホストがID 1 のパルスを送出した瞬間に即座に同期してLED（Pin 13）が点滅し、演奏（シリアル送信）を開始します。
3. **クライアント 2 (`CLIENT_ID = 2`)**:
   * 起動遅延 `ROUND_DELAY_BEATS` が `(2 - 1) * 4 = 4` に設定されているため、ホストがID 2 のパルスを送信し始めてから **4拍遅れて** LEDが点滅を開始し、演奏（シリアル送信）を開始します。
4. **PC (Processing)**:
   * クライアント機が拍を検知するたびに `Serial.write(0x03)` が送信され、Processing側で受信されます。

> [!CAUTION]
> ### 開発用メモ（シリアルデータの整合性について）
> * クライアントの [client.ino](file:///c:/Users/yuzu6/source/repos/Hac2/firmware/client/client.ino) は、拍ごとにバイナリデータ `0x03` を送信する仕様となっています。
> * 一方、[gakuhusaisei.pde](file:///c:/Users/yuzu6/source/repos/Hac2/processing/src/single_test/gakuhusaisei.pde) は、カンマ区切りの文字列 `"freq,beat,vel\n"` を受信する仕様となっています。
> * そのため、本テストにおいて Processing で実際に音階のある音をトリガーして鳴らす場合は、Processing側の `serialEvent()` 内で `0x03` バイトを検知して特定の周波数で `playNote(freq)` を鳴らすようにロジックを調整するか、あるいはクライアント側の `onBeat()` で `"freq,beat,vel\n"` 形式の文字列を出力するように適宜変更してください。
