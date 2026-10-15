# W2 作業｜第八組

本資料夾是第二週「原型實作與功能整合」的繳交版本。系統以 AMB82-MINI 與 JXF37 鏡頭完成即時影像、人物／書本／人臉辨識、頭部姿態判斷、番茄鐘網頁、統計與 LED 輸出。

## 資料夾內容

```text
W2作業_第八組/
├─ Ameba82_RTSP_VLC/             # 可直接用 Arduino IDE 開啟的原型程式
│  ├─ Ameba82_RTSP_VLC.ino
│  ├─ dashboard_html.h
│  └─ wifi_secrets.example.h
├─ docs/
│  ├─ 第二週_原型實作與功能整合_進度說明_第八組.docx
│  ├─ 第二週_原型實作與功能整合_進度說明_第八組.pdf
│  ├─ TEST_PLAN.md
│  ├─ VIDEO_SCRIPT.md
│  └─ VLC_RTSP_TEST.md
└─ 繳交前檢查.md
```

## 第二週完成項目

- FHD H.264 RTSP 串流，可用 VLC 觀看。
- 網頁取得 640 × 360 的最新 JPEG 畫面，降低長時間播放造成的延遲累積。
- YOLOv4-tiny 辨識人物與書本。
- SCRFD 偵測人臉與五個特徵點，估算低頭／抬頭。
- 連續維持指定秒數後才切換姿態，並加入數值平滑與進入／離開雙門檻。
- 網頁可設定專注、休息與姿態確認時間，並提供開始、暫停及重設。
- 統計專心、不專心、離席、姿態不明時間與分心事件次數。
- 低頭專心時點亮板上 LED；抬頭、無人或姿態不明時熄滅。
- 辨識結果逾時清除與 Wi-Fi 背景重連。
- 不需要 SD 卡或外部網頁伺服器。

## 硬體與開發環境

- 開發板：AMB82-MINI（AmebaPro2 / RTL8735B）
- 鏡頭：JXF37
- 已使用的 Ameba Arduino 版本：4.0.9
- Arduino IDE 設定：
  - Board：`AMB82-MINI`
  - Camera Options：`JXF37`
  - Camera FCS Mode：`Disable`
  - Serial Monitor：`115200 baud`

## 編譯與燒錄

1. 複製 `Ameba82_RTSP_VLC/wifi_secrets.example.h`，並改名為 `wifi_secrets.h`。
2. 在新檔案中填入 Wi-Fi 名稱與密碼。`wifi_secrets.h` 已列入 `.gitignore`，不要上傳真實密碼。
3. 用 Arduino IDE 開啟 `Ameba82_RTSP_VLC/Ameba82_RTSP_VLC.ino`。
4. 選好板子與鏡頭設定後編譯、進入燒錄模式並上傳。
5. 按 Reset，從序列監控視窗確認網頁與 VLC 網址。

## 目前實機網址

本版配合 iPhone 個人熱點固定使用：

- 儀表板：`http://172.20.10.4/`
- 最新網頁影像：`http://172.20.10.4:81/snapshot`
- VLC：`rtsp://172.20.10.4:554`

若更換網路，請調整 `.ino` 內的 `boardIp`、`hotspotDns`、`hotspotGateway` 與 `hotspotSubnet`，並以序列監控顯示的位址為準。

## 判斷方式與限制

目前以人臉五個特徵點的相對位置推估頭部俯仰，低頭暫定為專心、抬頭暫定為不專心。這不是眼球追蹤；側臉、遮擋、光線、鏡頭高度及個人坐姿皆可能影響結果。書本辨識只提供輔助資訊，不是專心判斷的唯一條件。

## 繳交連結

- GitHub：上傳後填入
- YouTube 操作影片：上傳後填入

上傳與錄影完成後，請把兩個連結補入 Word 報告，再輸出最終 PDF。詳細步驟見 `繳交前檢查.md`。
