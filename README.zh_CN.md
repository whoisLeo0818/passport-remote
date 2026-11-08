[English](README.md) · **简体中文**

# Passport Remote

Passport Remote 是适用于 FoloToy AI Passport 的固件，目标硬件为 ESP32-C3、8 MB Flash。通过设备的三个按键，可使用个人名片、蓝牙音乐遥控、照片查看和天气功能。

## 软件功能

- **个人名片**显示头像、名字、星座、属相、今日标签和天气摘要。设备开机直接进入名片。
- **音乐遥控**以 `Passport Remote` 与安卓手机配对。短按上／下换歌，确定播放或暂停，长按上／下调音量。已用 QQ 音乐验证。
- **照片查看**支持最多 6 张照片。上／下键翻图，确定键进入或退出全屏，图片保持原始比例。
- **天气查看**支持深圳和武汉，可手动刷新，也可每 30／60／120／240 分钟更新。名片与天气页共用数据。每次开机首次联网后校时，离线时有缓存则显示缓存。
- **系统设置**提供息屏时长、配置热点与外部 Wi-Fi 的独立开关、蓝牙配对管理。地铁码和公交码页面为界面占位。

长按确定打开或返回功能菜单。用上／下键选择条目，按确定进入。

## 本地管理

在系统设置开启 **Hotspot**，使用「Hotspot 信息」显示的密码连接 `Passport WIFI`，然后用浏览器打开 `http://192.168.4.1`。网页可编辑名片、添加或替换照片，并配置外部 Wi-Fi 和天气。外部 Wi-Fi 凭据在网页中输入。

## 源码分工

以下文件位于 [main/](main/)。

| 文件 | 作用 |
| --- | --- |
| `main.c` | 启动板级服务和应用循环，分发按键事件并刷新屏幕。 |
| `passport_model.c / .h` | 定义页面、按键动作、息屏时间和应用状态转换。 |
| `passport_ui.c / .h` | 绘制 LVGL 界面，显示名片、天气、连接状态和遥控反馈。 |
| `passport_text.h` | 集中定义设备文案和默认名片资料。 |
| `passport_ble.c / .h` | 管理蓝牙配对、重连及 HID 媒体指令。 |
| `passport_wifi.c / .h` | 管理配置热点、外部 Wi-Fi、网络扫描及本地 HTTP 接口。 |
| `passport_web.html` | 提供手机和电脑使用的名片编辑、照片管理及联网设置页面。 |
| `passport_assets.c / .h` | 将名片和照片写入 Flash，并在校验通过后切换到新数据。 |
| `passport_transfer.c / .h` | 校验上传格式、图像尺寸、数据长度和校验和。 |
| `passport_store.c / .h` | 将应用设置保存到 NVS。 |
| `passport_weather.c / .h` | 负责联网校时、HTTPS 天气请求、刷新和缓存。 |
| `passport_weather_model.c / .h` | 校验天气数据，将城市和天气代码转换成显示文本。 |
| `passport_font_inventory.h` | 记录设备界面使用的字形。 |
| `passport_photos.h` | 声明内置示例照片。 |
| `CMakeLists.txt` | 选择参与构建的源码，嵌入网页，并在构建时读取本地热点密码。 |

[components/bsp/](components/bsp/) 提供显示、按键、电池、音频和 I²C 驱动。[assets/](assets/) 存放字体和示例照片。[tests/](tests/) 包含主机检查。[tools/](tools/) 包含验证、打包和资源生成脚本。`demo_*` 和 `ui_pixel*` 是上游参考代码。实际编入固件的源码见 [main/CMakeLists.txt](main/CMakeLists.txt)。

## 构建

使用 ESP-IDF 5.5.3 并激活环境。在本地设置 `PASSPORT_WIFI_PASSWORD`，密码为 8–63 个字符，可用字母、数字及 `@._!+-`。在 Bash 中运行：

```bash
read -rs -p 'Hotspot password: ' PASSPORT_WIFI_PASSWORD
export PASSPORT_WIFI_PASSWORD
./tools/validate.sh
unset PASSPORT_WIFI_PASSWORD
```

校验后的合并固件位于 `build/FoloToy-AI-Passport-full.bin`，刷写地址为 `0x0`。更新已有资料的设备前，请查看[分区与刷写数据说明](docs/development/engineering/firmware-layout.zh_CN.md)。

更多信息见[应用指南和验收记录](docs/development/passport-card-remote.zh_CN.md)。硬件与基础固件来自 [FoloToy AI Passport](https://gitee.com/FoloToy/ai-passport)。许可见 [LICENSE](LICENSE)。
