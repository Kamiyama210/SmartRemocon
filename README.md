# エアコン自動制御装置

**室温を監視して赤外線でエアコンを自動制御する**システム。  
MQTT経由でPCアプリから遠隔操作・監視も可能。   

```mermaid
flowchart LR
    PCアプリ <--> |"MQTT"| マイコン
    マイコン --> |"赤外線"| エアコン
```
## 開発環境

| 項目 | 内容 |
|---|---|
| マイコン | Raspberry Pi Pico 2 W |
| SDK | Pico SDK 2.3.1 |
| RTOS | FreeRTOS-Kernel |
| ネットワーク | lwIP |
| 温度センサ | BME280 |
| 言語 | C |

## ディレクトリ構成

```
src/
├── main.c              エントリポイント
├── apps/               アプリ層
│   ├── network/        Wi-FiとMQTT関連
│   ├── control/        エアコン制御関連
│   └── wdt_monitor.c   ウォッチドッグ
├── modules/            モジュール層
├── utils/              Util層
└── wrappers/           ラッパー層
```

## レイヤ構成図

```mermaid
flowchart TD
    App層 --> Module層
    App層 --> Util層
    Module層 --> Util層
    Module層 --> Wrapper層
    Util層 --> Wrapper層
    Wrapper層 --> SDK
```

## ハードウェア構成

![回路図](docs/images/circuit.svg)

## タスク構成図

```mermaid
flowchart LR
    subgraph LWIP["lwIP"]
        CB["lwIP/cyw43 コールバック"]
    end

    subgraph RTOS["FreeRTOS"]
        ST["Startup<br>スタートアップ"]
        NET["Network<br>MQTT＆WIFI制御"]
        CTRL["Control<br>センサ読取＆エアコン制御"]
        WDT["WDT_Monitor<br>タスク監視"]
    end

    subgraph HW["ハードウェア"]
        BME["BME280(温度センサ)"]
        IR["赤外線LED"]
    end

    subgraph SERVER["サーバ"]
        MQTT["MQTT"]
    end

    CTRL <--> NET
    MQTT <--> NET
    CTRL --> BME
    CTRL --> IR
    ST -.->|生成| NET
    ST -.->|生成| CTRL
    WDT -.->|監視| NET
    WDT -.->|監視| CTRL
    CB --> NET
```

## 状態遷移図(ネットワークタスク)

```mermaid
stateDiagram-v2
    [*] --> WIFI_INIT

    WIFI_INIT --> WIFI_WAIT: 接続開始
    WIFI_INIT --> MQTT_INIT: 既に接続済み
    WIFI_WAIT --> MQTT_INIT: WIFI接続完了
    MQTT_INIT --> MQTT_WAIT: 接続開始
    MQTT_WAIT --> MQTT_PROCESS: MQTT接続完了
    MQTT_PROCESS --> MQTT_PROCESS

    WIFI_INIT --> TEARDOWN: 開始失敗
    WIFI_WAIT --> TEARDOWN: 失敗
    MQTT_INIT --> TEARDOWN: 開始失敗/Wi-Fi断
    MQTT_WAIT --> TEARDOWN: 失敗
    MQTT_PROCESS --> TEARDOWN: 切断/送信失敗3回

    TEARDOWN --> RETRY_DELAY: mqtt_stop / wifi_stop
    RETRY_DELAY --> WIFI_INIT: 待機終了
    RETRY_DELAY --> MQTT_INIT: 待機終了(Wi-Fi生存時)
```
**補足**
- すべての異常系をTEARDOWNへ通し、開始終了は常に対にする。
- リトライ時間は指数バックオフ（上限60秒）。
   
## 状態遷移図(制御タスク)

```mermaid
stateDiagram-v2
    [*] --> INIT

    INIT --> IDLE: 既に適温
    INIT --> SETTLING: 運転開始

    SETTLING --> STEADY: 10分経過かつ室温が安定
    STEADY --> SETTLING: 室温不安定

    IDLE --> SETTLING: 適温から外れた

    INIT --> FAULT: センサ異常
    SETTLING --> FAULT: センサ異常
    STEADY --> FAULT: センサ異常
    IDLE --> FAULT: センサ異常
    OFF --> FAULT: センサ異常
    FAULT --> INIT: センサ復帰
    FAULT --> OFF: センサ復帰(異常の前か異常中に停止操作を受けていた場合)

    INIT --> OFF: 停止操作
    SETTLING --> OFF: 停止操作
    STEADY --> OFF: 停止操作
    IDLE --> OFF: 停止操作
    OFF --> SETTLING: 運転操作
```
**補足**
- IDLEは起動時に既に適温だったときの待機で、室温が外れれば自動で運転を始める。
- OFFはPCからの停止操作による停止で、次の運転操作を受けるまで自動では運転しない。
- FAULT以外の状態で運転操作を受けるとSETTLINGに移る。
- FAULTの間は運転操作は受け付けない。
- OFFは再起動すると解除され、INITから始まる。
