# （プロジェクト名未設定）

このリポジトリはテンプレートから作成されました。
Claude Code で `/init-project` を実行して初期化してください。

C++/DxLib で開発するゲームプロジェクトです。Windows 版と、スマホでテストプレイするための Android 版を、同じソースコードからビルドします。

## フォルダ構成

| フォルダ | 内容 |
|---|---|
| `src/common/` | ゲーム本体（両OS共通） |
| `src/windows/` | Windows 固有の部分（`WinMain`、マウス入力） |
| `src/android/` | Android 固有の部分（`android_main`、タッチ入力、画面の拡大表示、内部ストレージ） |
| `windows/` | Visual Studio 2026 のソリューション・プロジェクト |
| `android/` | Gradle＋CMake のプロジェクト（Android Studio で開ける） |
| `assets/` | 画像・音声などの素材（両OSで同じものを使う） |
| `docs/` | 仕様書 |

OSによって違う処理は `src/common/Platform.h` の関数にまとめ、OS別のファイル（`Platform_windows.cpp` / `Platform_android.cpp`）で実装しています。ゲーム本体は `src/common/Game.cpp` の `Update`（入力と時間を進める）と `Draw`（描く）に書きます。

## ファイルを追加するとき

- `src/common/` に `.cpp` / `.h` を追加したら、Visual Studio のプロジェクト（`windows/` の `.vcxproj`）にも追加します。Android 版は `src/common/` と `src/android/` の `.cpp` を自動で集めるので、追加の作業は要りません。
- 各 `.cpp` は、最初の `#include` で `pch.h` を読み込みます。
- 素材は `assets/` に置き、`sound/…` のように `assets/` からの相対パスで読み込みます。

## Windows 版

- `windows/` のソリューション（`.sln`）を Visual Studio 2026 で開いてビルドします。DXライブラリは `D:\DxLib_VC` に置きます。
- デバッグ実行時の作業フォルダは `assets/` です（素材をそこから読み込みます）。

## Android 版

- DXライブラリ Android版 3.25a（`DxLib_Android3_25a.zip`）を展開し、`DxLib_Android` フォルダを `android/DxLib_Android` に置いてから、`android/` を Android Studio で開きます（リポジトリには含めません）。
- NDK は 27.3.13750724（DxLib Android版のライブラリと同じ r27 系）、ABI は arm64-v8a に固定しています。
- 素材は `android/app/build.gradle` の設定で、リポジトリの `assets/` をそのまま apk に入れます。
- 署名はテストプレイ専用のデバッグ鍵 `android/keystore/gametest.jks` を使います（[注意書き](android/keystore/README.md)）。

### 操作（タッチ）

| Windows | Android |
|---|---|
| 左クリック | タップ（指1本） |
| 左ボタンを押したまま動かす | 指でなぞる |
| 右クリック | 2本指でタップ |

`Platform::PollInput()` が、どちらの操作も同じ形（位置・主な操作・2つ目の操作）で返します。

画面は 800×600（`src/common/Config.h`）のまま、端末の画面に縦横比を保って拡大表示します（横向き固定）。

## GitHub Actions

- `Build`（`.github/workflows/build.yml`）：Windows版を Windows ランナーでビルドし、exe と `assets/` の中身を成果物として7日間保存します。
- `Android`（`.github/workflows/android.yml`）：Android版の apk を Linux ランナーでビルドし、GitHub Releases の [android-latest](../../releases/tag/android-latest)（プレリリース）に上書きで置きます。スマホのブラウザからダウンロードしてインストールできます。

どちらも、どのブランチに push しても動きます。
