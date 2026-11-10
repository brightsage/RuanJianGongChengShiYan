# 数据服务与权限管理

本模块在 `Data-Service-&-Permission-Management` 分支逐步实现。

构建依赖：SQLite3、OpenSSL、json-c 的开发包。Debian/Ubuntu 可安装 `libsqlite3-dev libssl-dev libjson-c-dev pkg-config`。

首次启动设置 `PLATFORM_ADMIN_PASSWORD`（至少 12 个字符），创建用户名为 `admin` 的系统管理员。默认数据库为 `data/platform.sqlite3`，可通过 `PLATFORM_DB` 指定。已有数据库不会因重启重置密码。

请求前缀：`/api/modules/data-services`。登录 `POST /login` 提交 `username,password`，返回 Bearer `access_token` 和 Unix 秒 `expires_at`。令牌只返回一次，数据库仅保存摘要；有效期 1 小时。`GET /me` 获取当前身份，`POST /logout` 撤销当前会话。

系统管理员通过 `GET/POST /users` 和 `PATCH /users/:id` 管理用户，通过 `GET /roles`、`PUT /roles/:code` 管理角色。用户绑定一个角色与组织、班级、学号；学生必须具备这三个身份字段。修改用户会撤销其会话，角色权限变更在下一次请求即时生效。内置 `system_admin` 角色不可修改。

其他模块调用 `app.security()->authorize(request, permission, scope)`，从返回的 `Principal.student_no` 获取学生本人学号。无登录或登录失效返回 401；权限或资源范围不足返回 403。范围支持 `all/college/class/self`，资源范围应由服务端查询真实记录得到。省略 scope 仅做功能权限检查。

就业中心角色为 `career_center`，有 `career.jobs.read/write/publish`、`career.strategy.write` 四个权限，范围为全校。学生为 `student`，拥有 `career.recommendations.read`，范围为本人。

公共接口在 `include/platform/security.hpp`。其他模块可调用 `audited_write(principal, event, callback)`，在 callback 的同一 SQLite 连接内完成业务 SQL。函数在业务操作后写入审计并提交；业务或审计任一失败都会回滚。不得在 callback 内自行提交事务或执行外部副作用。`AuditEvent` 必填 action、resource_type、resource_id，支持 before/after；密码、令牌、学号、联系方式等字段会自动脱敏。`GET /audit-logs` 仅对具有 `audit.read` 权限的账号开放。
