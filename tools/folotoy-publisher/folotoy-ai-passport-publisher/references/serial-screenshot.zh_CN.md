<p align="right">
  <a href="serial-screenshot.md">English</a> · <strong>简体中文</strong>
</p>

# 串口屏幕捕获协议

`capture-screen` 获取运行中的显示帧缓冲，作为以玩法为主的封面证据。它不会拍摄外壳，也不要求封面出现外壳。

命令默认以 115200 波特打开创作者选择的串口并写入以下 ASCII 行：

```text
FAP_SCREENSHOT_V1\n
```

命令必须是只读观察操作，不得重启、刷写、擦除、修改设置或暴露设备凭据。

## 设备响应

固件紧接着返回一行 ASCII 头和声明的二进制载荷：

```text
FAP_SCREENSHOT_V1 <width> <height> <encoding> <byte_length>\n
<binary payload>
```

支持的编码：

- `RGB565LE`：按行排列的小端 RGB565 像素，`byte_length` 必须等于 `width * height * 2`。
- `PNG`：完整 PNG，尺寸必须与 `width` 和 `height` 匹配。

宽度和高度都必须为 1–4096 像素，载荷不得超过 10 MiB。成功后发布脚本保存 PNG，不修改连接中的设备。

## 可用性和强制校验

通过本工具发布的固件必须实现该协议。成功捕获会写入 PNG 及相邻的 `.fap-capture.json` 回执，回执包含屏幕哈希、尺寸、端口、协议和捕获时间。`validate` 与 `submit` 要求匹配的回执，并拒绝超过 24 小时的捕获。只会输出日志的现有固件必须先加入该协议，才能通过本工具发布。

将捕获画面用于公开封面前，必须检查其中没有设备 KEY、密码、个人数据、令牌或其他秘密。不要把含敏感信息的捕获画面用于图像生成或公开发布。
