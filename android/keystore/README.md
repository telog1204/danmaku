# テストプレイ用の署名鍵

**この鍵はテストプレイ専用のデバッグ鍵であり、公開されても問題ない前提でリポジトリに含めている。ストア公開などの本番用途には絶対に使わず、公開時は別の鍵を作成してGitHub Secretsで管理すること。**

- ファイル：`gametest.jks`（PKCS12形式）
- エイリアス：`gametest`
- パスワード：`gametest`（ストアと鍵で共通）
- アルゴリズム：RSA 2048ビット、有効期限 10000日
- 用途：`android/app/build.gradle` のデバッグ署名（`signingConfigs.debug`）。毎回同じ鍵で署名するので、スマホに上書きインストールできる。

作成コマンド（参考）：

```
keytool -genkeypair -keystore gametest.jks -storetype PKCS12 -alias gametest -keyalg RSA -keysize 2048 -validity 10000 -storepass gametest -keypass gametest -dname "CN=Game Test (debug only)"
```
