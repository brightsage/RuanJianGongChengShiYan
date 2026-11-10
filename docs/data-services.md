# 数据服务与权限管理

本模块在 `Data-Service-&-Permission-Management` 分支逐步实现。

构建依赖：SQLite3、OpenSSL、json-c 的开发包。Debian/Ubuntu 可安装 `libsqlite3-dev libssl-dev libjson-c-dev pkg-config`。

首次启动设置 `PLATFORM_ADMIN_PASSWORD`（至少 12 个字符），创建用户名为 `admin` 的系统管理员。默认数据库为 `data/platform.sqlite3`，可通过 `PLATFORM_DB` 指定。已有数据库不会因重启重置密码。

请求前缀：`/api/modules/data-services`。登录 `POST /login` 提交 `username,password`，返回 Bearer `access_token` 和 Unix 秒 `expires_at`。令牌只返回一次，数据库仅保存摘要；有效期 1 小时。`GET /me` 获取当前身份，`POST /logout` 撤销当前会话。

系统管理员通过 `GET/POST /users` 和 `PATCH /users/:id` 管理用户，通过 `GET /roles`、`PUT /roles/:code` 管理角色。用户绑定一个角色与组织、班级、学号；学生必须具备这三个身份字段。修改用户会撤销其会话，角色权限变更在下一次请求即时生效。内置 `system_admin` 角色不可修改。

其他模块调用 `app.security()->authorize(request, permission, scope)`，从返回的 `Principal.student_no` 获取学生本人学号。无登录或登录失效返回 401；权限或资源范围不足返回 403。范围支持 `all/college/class/self`，资源范围应由服务端查询真实记录得到。省略 scope 仅做功能权限检查。

就业中心角色为 `career_center`，有 `career.jobs.read/write/publish`、`career.strategy.write` 四个权限，范围为全校。学生为 `student`，拥有 `career.recommendations.read`，范围为本人。

公共接口在 `include/platform/security.hpp`。其他模块可调用 `audited_write(principal, event, callback)`，在 callback 的同一 SQLite 连接内完成业务 SQL。函数在业务操作后写入审计并提交；业务或审计任一失败都会回滚。不得在 callback 内自行提交事务或执行外部副作用。`AuditEvent` 必填 action、resource_type、resource_id，支持 before/after；密码、令牌、学号、联系方式等字段会自动脱敏。`GET /audit-logs` 仅对具有 `audit.read` 权限的账号开放。

## 服务发布与授权

数据管理员使用 `POST /services` 发布 `{name,provider_id,version,fields,scope,rate_per_minute}`；`scope` 是 `{type,org_id?,class_id?,student_no?}`。发布后的数据范围、字段、提供者、版本不可修改，需要修改时发布新版本。`PATCH /services/:id` 只允许调整 `active` 与限流值；暂停立即停止授权和令牌签发。`GET /services` 返回与当前账号范围有交集的服务。

院系管理员或第三方账号使用 `POST /applications` 提交 `{service_id,purpose,scope,validity_seconds,quota,rate_per_minute}`。申请范围须同时被账号范围和服务范围覆盖。`GET /applications` 对申请人仅返回本人记录，对审批人返回其范围内的记录。

数据管理员调用 `POST /applications/:id/decision`。批准体为 `{decision:"approve",validity_seconds,quota,rate_per_minute,mask_level:"standard"或"strict",scope?}`，各项只可缩小申请值，不能审批本人申请。拒绝体为 `{decision:"reject",reason}`。重复审批返回 409。

批准后由申请人调用 `POST /applications/:id/token` 取得服务令牌 `svc_...`；令牌与登录令牌分离，只返回一次，再次调用会轮换。`POST /applications/:id/revoke` 允许申请人或审批人撤销授权。授权到期、撤销、账号停用、范围收缩或服务暂停后，不再允许签发有效令牌。令牌和配额均与具体服务版本绑定。
