<p align="right">
  <a href="api.md">English</a> · <strong>简体中文</strong>
</p>

# 发布 API

随附脚本是首选接口。只有在排查故障时才需要阅读这些细节。

## 授权

- `POST /api/agent/device-code` 创建有效期十分钟的浏览器授权请求。
- 打开 `verificationUriComplete`。创作者登录或注册后批准页面显示的代码。
- 轮询 `POST /api/agent/token` 并传入 `device_code`。HTTP 428 表示授权仍在等待中。
- 返回的 bearer token 仅限创作者投稿，30 天后过期，也可以从创作者页面撤销。

## 创作者操作

Bearer 认证使用 `Authorization: Bearer <token>`。

- `GET /api/agent/me` 验证授权。
- `GET /api/agent/projects` 列出创作者最近的项目修订版。
- `POST /api/agent/submissions` 创建项目。
- `POST /api/agent/submissions/{project_id}/resubmit` 提交修订版。

投稿接口接受 multipart 字段 `title_zh`、`description_zh`、可选的 `title_en`、`description_en` 和 `github_url`，以及 `cover` 与 `firmware`。封面必须严格为 3:4 竖向比例。历史兼容字段 `github_url` 接受 GitHub、Gitee、GitLab、Codeberg 或其他公开 HTTPS Git 仓库页面。服务器校验始终是最终依据。

被拒绝的变更不得自动重试。先向创作者展示响应并解决原因。
