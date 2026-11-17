# 数据服务与权限管理

本模块覆盖实验报告的 UC-DS-01～UC-DS-08，管理页为 `/data-services`。

构建依赖：SQLite3、OpenSSL、json-c 的开发包。Debian/Ubuntu 可安装 `libsqlite3-dev libssl-dev libjson-c-dev pkg-config`。

首次启动设置 `PLATFORM_ADMIN_PASSWORD`（至少 12 个字符），创建用户名为 `admin` 的系统管理员。默认数据库为 `data/platform.sqlite3`，可通过 `PLATFORM_DB` 指定。已有数据库不会因重启重置密码。

请求前缀：`/api/modules/data-services`。登录 `POST /login` 提交 `username,password`，返回 Bearer `access_token` 和 Unix 秒 `expires_at`。令牌只返回一次，数据库仅保存摘要；有效期 1 小时。`GET /me` 获取当前身份，`POST /logout` 撤销当前会话。

系统管理员通过 `GET/POST /users` 和 `PATCH /users/:id` 管理用户，通过 `GET /roles`、`PUT /roles/:code` 管理角色。用户绑定一个角色与组织、班级、学号；学生必须具备这三个身份字段。修改用户会撤销其会话，角色权限变更在下一次请求即时生效。内置 `system_admin` 角色不可修改。

用户列表中的学号被遮盖，`GET /me` 仅向本人提供完整身份；其他模块在服务端使用 `Principal.student_no`。系统级用户、角色、脱敏和审计管理要求全校范围，不能给院级账号授予这些权限后越过数据范围。用户管理者可读取角色和权限清单，以便分配角色。

其他模块调用 `app.security()->authorize(request, permission, scope)`，从返回的 `Principal.student_no` 获取学生本人学号。无登录或登录失效返回 401；权限或资源范围不足返回 403。范围支持 `all/college/class/self`，资源范围应由服务端查询真实记录得到。省略 scope 仅做功能权限检查。

就业中心角色为 `career_center`，有 `career.jobs.read/write/publish`、`career.strategy.write` 四个权限，范围为全校。学生为 `student`，拥有 `career.recommendations.read`，范围为本人。

公共接口在 `include/platform/security.hpp`。其他模块可调用 `audited_write(principal, event, callback)`，在 callback 的同一 SQLite 连接内完成业务 SQL。函数在业务操作后写入审计并提交；业务或审计任一失败都会回滚。不得在 callback 内自行提交事务或执行外部副作用。`AuditEvent` 必填 action、resource_type、resource_id，支持 before/after；密码、令牌、学号、联系方式等字段会自动脱敏。`GET /audit-logs` 仅对具有 `audit.read` 权限的账号开放。

## 服务发布与授权

数据管理员使用 `POST /services` 发布 `{name,provider_id,version,fields,scope,rate_per_minute}`；`scope` 是 `{type,org_id?,class_id?,student_no?}`。发布后的数据范围、字段、提供者、版本不可修改，需要修改时发布新版本。`PATCH /services/:id` 只允许调整 `active` 与限流值；暂停立即停止授权和令牌签发。`GET /services` 返回与当前账号范围有交集的服务。

院系管理员或第三方账号使用 `POST /applications` 提交 `{service_id,purpose,scope,validity_seconds,quota,rate_per_minute}`。申请范围须同时被账号范围和服务范围覆盖。`GET /applications` 对申请人仅返回本人记录，对审批人返回其范围内的记录。

数据管理员调用 `POST /applications/:id/decision`。批准体为 `{decision:"approve",validity_seconds,quota,rate_per_minute,mask_level:"standard"或"strict",scope?}`，各项只可缩小申请值，不能审批本人申请。拒绝体为 `{decision:"reject",reason}`。重复审批返回 409。

批准后由申请人调用 `POST /applications/:id/token` 取得服务令牌 `svc_...`；令牌与登录令牌分离，只返回一次，再次调用会轮换。`POST /applications/:id/revoke` 允许申请人或审批人撤销授权。授权到期、撤销、账号停用、范围收缩或服务暂停后，不再允许签发有效令牌。令牌和配额均与具体服务版本绑定。

## 提供者与受控调用

其他模块通过 `app.data_services()->register_provider(id, fields, handler)` 接入只读数据提供者，声明允许发布的字段。头文件为 `include/platform/data_services.hpp`。应用先初始化数据服务模块，然后初始化其余模块，所以各模块注册函数中即可调用此入口。`GET /providers` 供数据管理员查看当前注册的提供者。

handler 接收 `Query {caller,scope,fields,parameters}`，返回记录数组。有范围限制时，记录必须携带真实 `org_id`，班级范围还需 `class_id`，本人范围还需 `student_no`。提供者先按 scope 查询，网关返回前再次筛选范围，并且只保留已发布字段。调用者不能在请求体中指定授权范围。提供者应只读且不产生外部副作用。

第三方使用服务令牌请求 `POST /services/:id/invoke`，请求体 `{parameters:{...}}`。返回 `{service_id,version,records,request_id}`。网关重新检查账号、权限、范围、服务状态和令牌期限，按授权总额度、授权每分钟限流、服务全局每分钟限流共同限制调用。成功响应才消耗额度；失败也留调用审计。未注册提供者返回 503，提供者字段与发布配置不符返回 409，绝不返回模拟业务数据。

系统管理员通过 `GET /masking-rules`、`PUT /masking-rules/:field` 配置 `{algorithm:"redact"|"partial"|"hash"}`。默认覆盖学号、身份证、姓名、成绩、电话和邮件等字段，递归处理嵌套结构。hash 使用数据库内持久密钥的 HMAC-SHA256；strict 授权将所有配置为敏感的字段完全遮盖。密码、令牌、密钥字段始终遮盖。新增敏感字段时须同时配置脱敏规则。

`GET /call-logs` 仅供系统管理员审计，可按 `service_id/actor_id/status` 过滤。`GET /audit-logs` 可按 `actor_id/action/resource_type/outcome` 过滤。两者支持 Unix 秒 `from/to` 与 `limit/offset` 分页（默认 100，最多 200）。日志不包含令牌、请求参数或原始数据，调用日志与操作审计通过 request_id 关联。成功调用的额度扣减、调用日志与操作审计在同一事务内；审计失败时回滚并返回 500。

## 模块对接示例

职业推荐模块可以在注册路由时捕获公共权限服务：

```cpp
#include "platform/security.hpp"

auto security = app.security();
app.add_module_route("career-recommendation", "GET", "/me",
    [security](const platform::Request& request) {
        auto actor = security->authorize(request, "career.recommendations.read");
        // 用 actor.student_no 查询本人推荐，不能使用前端传入的学号替代。
        // 在这里调用职业推荐模块自己的业务函数。
        return platform::Response{200, "application/json", "{}"};
    });
```

以上仅是接入示例，仓库不注册这条职业推荐路由。新增模块权限时，先调用 `app.security()->register_permission("module.action", "权限说明")`，管理员随后可从 `GET /permissions` 清单把该权限分配给角色。

脱敏请求与内部身份示例：

```http
GET /api/modules/career-recommendation/me HTTP/1.1
Authorization: Bearer usr_<redacted>
```

```json
{"user_id":"u_<redacted>","role":"student","student_no":"2026****","scope":{"type":"self","org_id":"college-a","class_id":"class-a","student_no":"2026****"}}
```

上面 JSON 仅展示 Principal 的部分字段；服务端真实学号由认证服务从账号库读取。未登录或会话过期统一返回 `401 {"error":"unauthorized","message":"..."}`，响应含 `WWW-Authenticate: Bearer`；已登录但权限/范围不足返回 `403 {"error":"forbidden","message":"..."}`。`App::handle` 会把公共异常转换成 HTTP 响应，职业推荐模块无需重复实现令牌解析和错误转换。

就业中心可管理全校岗位和全局策略，角色与权限映射：

| 角色 | 权限 | 范围 |
| --- | --- | --- |
| system_admin | users.manage、roles.manage、masking.manage、audit.read | 全校 |
| data_admin | services.publish、grants.approve、services.read | 全校 |
| college_admin / third_party | grants.apply、services.read | 本学院 |
| career_center | career.jobs.read、career.jobs.write、career.jobs.publish、career.strategy.write | 全校 |
| student | career.recommendations.read | 本人 |
| counselor | 由对应业务模块注册并分配 | 本班 |

创建/编辑岗位使用 `career.jobs.write`，发布/下架/重新发布使用 `career.jobs.publish`，修改全局策略使用 `career.strategy.write`。岗位的 `owner_org_id` 可记录来源，不限制就业中心的全校管理权。

上述六类写操作统一调用：

```cpp
platform::security::AuditEvent event{"career.job.update", "job", job_id};
event.before = before;  // platform::Json，建议只放发生变化的字段
event.after = after;
security->audited_write(actor, event, [&](sqlite3* db) {
    // 在 db 上执行岗位 SQL，检查返回值并在失败时抛异常。
});
```

建议动作名：`career.job.create/update/publish/unpublish/republish`、`career.strategy.update`。`actor` 必须先经过 authorize。业务与审计共享该连接的一笔事务；外部数据库或文件操作不在此事务内。业务失败自动回滚，并尝试记录 failure；审计存储故障时不返回业务成功。不要将令牌、密码和整份学生对象放入审计快照。

## 验证与当前限制

`ctest --test-dir build --output-on-failure` 运行应用回归、身份权限、服务授权、网关脱敏和两个真实 HTTP 测试。测试数据只存在于内存或临时目录，不写默认数据库。覆盖了权限与范围拒绝、过期/停用/退出、服务版本绑定、批准额度约束、轮换与撤销、递归脱敏、授权和服务两级限流、SQLite 审计失败回滚，以及进程重启后的持久化。

浏览器仅在内存保存登录令牌，刷新后需重新登录。服务令牌只在签发时显示，管理页支持直接带令牌调用服务。底层 HTTP 服务仍是本地单线程开发服务器，没有 TLS、分块传输或跨域部署支持。日志当前持久保存并支持分页筛选，自动归档与留存期清理未实现。数据提供者在其他模块接入前为空，不包含预置学生业务数据。
