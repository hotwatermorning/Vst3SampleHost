## Vst3SampleHost

[![Build Status](https://dev.azure.com/vst3host-dev/vst3host-dev/_apis/build/status/hotwatermorning.Vst3SampleHost?branchName=master)](https://dev.azure.com/vst3host-dev/vst3host-dev/_build/latest?definitionId=1&branchName=master)

このプロジェクトは、VST3 プラグインをホストするサンプルホストアプリケーションのプロジェクトです。

![ScreenShot](./misc/ScreenShot.png)

## ダウンロード

最新のビルド済みパッケージは、このプロジェクトのリリースページからダウンロードできます。

* https://github.com/hotwatermorning/Vst3SampleHost/releases

#### 免責事項
@hotwatermorning は、このソフトウェアの使用によって生じたいかなる損害に対しても責任を負いません。

## 機能

* VST3 プラグインをロードできます。
* PC キーボードや Vst3SampleHost 上の鍵盤 UI でオーディオ信号や MIDI 信号を生成し、プラグインでそのデータを処理できます。
    * PC キーボードの A, W, S, ..., L, P が、鍵盤のド、レ♭、レ、...、（オクターブ上の）レ、（オクターブ上の）ミ♭に対応します。
    * Z と X キーでオクターブを変更できます
    * 読み込んだプラグインがインストゥルメントプラグインの場合は MIDI 信号を生成して、インストゥルメントプラグインで音を鳴らせます。
    * 読み込んだプラグインがエフェクトプラグインの場合は、オーディオ信号を生成して、エフェクトプラグインで音を加工できます。
* マイク入力をプラグインに送信して、エフェクト処理できます。

## ビルド方法

（詳細なビルド方法は [wiki](https://github.com/hotwatermorning/Vst3SampleHost/wiki/%E3%83%93%E3%83%AB%E3%83%89%E6%96%B9%E6%B3%95) にも記載しています）

Vst3SampleHost は、以下の環境でビルドできます。

* macOS 10.13.4 & Xcode 10.1
* Windows 10 & Visual Studio 2017
* Windows 10 & Visual Studio 2019

### 必須要件

* Java JRE (or JDK) version 8 or later (Gradleを使用するため)
* Git 2.8.1 or later
* CMake 3.14.1 or later
* Xcode 10.1 or later
* Visual Studio 2017 or later

### macOS 環境でのビルドコマンド

```sh
cd ./gradle

./gradlew build_all [-Pconfig=Debug]
# `config` プロパティはデフォルトで `Debug` が指定されます。リリースビルド時は、 `-Pconfig=Release` を指定します。

open ../build_debug/Debug/Vst3SampleHost/Vst3SampleHost.app
```

### Windows 環境でのビルドコマンド

```bat
cd .\gradle

gradlew build_all [-Pconfig=Debug] [-Pmsvc_version="..."]
: `config` プロパティはデフォルトで `Debug` が指定されます。リリースビルド時は、 `-Pconfig=Release` を指定します。
:
: `msvc_version` プロパティには、`"Visual Studio 16 2019"` または `"Visual Studio 15 2017"` を指定できます。
: デフォルトでは `"Visual Studio 16 2019"` が使用されます。
: そのため、Visual Studio 2017のみをインストールしている環境ではこのオプションに明示的に `"Visual Studio 15 2017"` を指定してください。
:
: 非英語ロケール環境でのビルド時に文字化けが発生する場合は、 `-Dfile.encoding=UTF-8` オプションを追加してください。

start ..\build_debug\Debug\Vst3SampleHost\Vst3SampleHost.exe
```

### TIPS

* サブモジュールのビルドが完了していて Vst3SampleHost 自体の再ビルドのみが必要な場合は、以下のようにコマンドを実行することで、不要なサブモジュールの再ビルドをスキップして、プロジェクトファイルの再生成と Vst3SampleHost の再ビルドを実行できます。

```sh
./gradlew prepare_project build_app [-Pconfig=Debug]
```

## ライセンスと依存ライブラリ

Vst3SampleHost のソースコードは MIT License で公開します。配布するバイナリにはさらに Proprietary Steinberg VST3 License と Steinberg ASIO SDK License が適用されます。

Vst3SampleHost は以下のライブラリに依存しています。

* [wxWidgets](http://www.wxwidgets.org/)
* [PortAudio](http://www.portaudio.com/)
* [VST3 SDK](https://www.steinberg.net/en/company/developers.html)
* [ASIO SDK](https://www.steinberg.net/en/company/developers.html)
* [RtMidi](https://github.com/thestk/rtmidi)
* [Catch2](https://github.com/catchorg/Catch2)

## 商標について

<img src="./misc/VST_Compatible_Logo_Steinberg_with_TM_negative.png" width="20%" height="20%" alt="VST Compatible Logo"></img>

<img src="./misc/ASIO-compatible-logo-Steinberg-TM-BW.jpg" width="20%" height="20%" alt="ASIO Compatible Logo"></img>

## 連絡先

hotwatermorning@gmail.com

https://twitter.com/hotwatermorning

