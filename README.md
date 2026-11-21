# 高校学生成长与职业发展数据中台

六个模块均已注册并提供 HTTP 接口，全部使用 MySQL 持久化。需求范围见《实验一报告》。

## 环境与运行

- Windows（Winsock）、Linux 或 macOS（POSIX socket）
- 支持 C++17 的编译器、CMake 3.21 或更新版本
- Python 3 可选，用于 HTTP 冒烟测试
- MySQL 8.4（客户端头文件/库 + 一个可连接的 MySQL 实例；SQLite 已从构建中移除）
- 平台身份认证与审计使用仓库内置的 `platform/crypto.cpp`，不需要 OpenSSL
- 默认连接 `mysql://platform:PlatformPass123!@127.0.0.1:3307/student_platform`，可用 `PLATFORM_DB_URL` 或 `PLATFORM_DB_HOST/PORT/USER/PASSWORD/NAME` 覆盖
- 换电脑或新环境从零部署（数据库、环境变量、构建、数据迁移）见 [docs/换机部署.md](docs/换机部署.md)

Linux / macOS：

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/platform_server 8080
```

Windows / VS Code：安装 CMake Tools 与 C/C++ 扩展，打开项目后选择 `windows-vs2026` preset，`F7` 构建、`F5` 运行。命令行等价操作：

```powershell
cmake --preset windows-vs2026
cmake --build --preset windows-vs2026
ctest --preset windows-vs2026
.\build\windows-vs2026\Debug\platform_server.exe 8080
```

打开 <http://127.0.0.1:8080/>。服务只监听本机回环地址；端口参数可改，省略时为 8080。测试台列出已经注册的模块和路由，并允许填写方法、路径、请求头和请求体，查看实际 HTTP 响应。它是开发工具，不是最终产品前端。

开发/演示页面：

- `/workspace`：统一业务工作台，覆盖数据接入、数据治理、学生画像、学业预警；支持登录、分页、规则启停、主数据浏览和预警筛选/处置
- `/career`：职业发展推荐，启动方式见 [职业发展推荐模块](docs/职业发展推荐模块.md)
- `/data-services`：数据服务与权限管理，说明见 [数据服务与权限管理](docs/data-services.md)

组内接口约定见 [职业推荐联调说明](docs/职业推荐联调说明.md)

首次启动正式模式前设置至少 12 字符的 `PLATFORM_ADMIN_PASSWORD`，用于创建 `admin` 账号。权限管理与账号创建见 [数据服务与权限管理](docs/data-services.md)

一条命令跑通 M1→M6 的端到端演示见 [scripts/e2e_demo.py](scripts/e2e_demo.py)：先启动服务，再执行 `py -3 scripts/e2e_demo.py 8090`。

现在可用的公共接口：

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| GET | `/api/health` | 检查服务是否启动 |
| GET | `/api/modules` | 列出六个模块及各自已注册的路由 |

未注册的地址返回 404，不提供模拟成功结果。

## 模块边界

| 模块 | 目录 | 路由前缀 |
| --- | --- | --- |
| 数据采集与接入 | `src/modules/data_ingestion/` | `/api/modules/data-ingestion/` |
| 数据治理与标准化 | `src/modules/data_governance/` | `/api/modules/data-governance/` |
| 学生画像 | `src/modules/student_profile/` | `/api/modules/student-profile/` |
| 学业分析与预警 | `src/modules/academic_alerts/` | `/api/modules/academic-alerts/` |
| 职业发展推荐 | `src/modules/career_recommendation/` | `/api/modules/career-recommendation/` |
| 数据服务与权限管理 | `src/modules/data_services/` | `/api/modules/data-services/` |

每个 `module.cpp` 是本模块的注册入口。新增源文件时，把它列入 `CMakeLists.txt` 的 `platform_core`。公共请求、响应和路由接口在 `include/platform/app.hpp`，注册入口声明在 `include/platform/modules.hpp`，组合顺序在 `src/app.cpp`。前端资源在 `web/`；组员的业务代码主要写在各自模块目录中。

## 数据库与共享工具

- `include/platform/json.hpp`：轻量 JSON 解析与输出，模块用它处理请求体和响应体。
- `include/platform/db.hpp`：MySQL 8.x 薄封装，提供连接、`exec`、`query`、参数绑定和事务。
- 默认连接为 `mysql://platform:PlatformPass123!@127.0.0.1:3307/student_platform`；优先读 `PLATFORM_DB_URL`，否则读 `PLATFORM_DB_HOST/PORT/USER/PASSWORD/NAME`。`PLATFORM_TEST_DB=1` 时每个测试进程使用 `_test_<pid>_<timestamp>` 独立库，同进程内的所有模块共用一个库。
- 表名按模块加前缀（数据接入 `ing_`、数据治理 `gov_`、学生画像 `sp_`、学业预警 `aa_`、职业推荐 `cd_`），各模块在启动/仓储初始化时执行 `CREATE TABLE IF NOT EXISTS`。
- 六个模块均已使用 MySQL。
- MySQL 初始化、建库和测试账号见 [`docs/模块框架说明.md`](docs/模块框架说明.md) 的数据库章节。

## 添加一个接口

以学生画像模块为例，在它的 `register_student_profile` 函数内登记路由：

```cpp
app.add_module_route("student-profile", "GET", "/:student_id",
    [](const platform::Request& request) {
        const auto& id = request.path_params.at("student_id");
        return platform::Response{200, "text/plain; charset=utf-8", id};
    });
```

随后请求 `GET /api/modules/student-profile/123`，测试台会自动列出该路由。动态路径段写作 `:参数名`。`Request` 提供 `method`、`path`、小写键名的 `headers`、原始 `body`、原始 `query` 和 `path_params`；`Response` 提供状态码、内容类型、响应体和可选响应头。路由注册遇到未知模块或重复的“方法 + 路径”会报错。

新增业务接口时，同时添加能验证其行为的测试。`tests/app_test.cpp` 演示了直接调用应用层的测试；`tests/http_smoke.py` 验证实际 HTTP 边界。建议先写一个可运行的接口和对应测试，再继续下一个接口。

## 当前边界

这个轻量服务只用于本地联调：单线程、每个连接只处理一个请求、请求体上限 1 MiB，不支持分块传输、TLS 或生产部署。路由采用固定路径和 `:参数名`；查询串与请求体仅保留原文，具体格式解析由模块自行决定。M5/M6 已经接入统一鉴权和审计，M1/M2/M3/M4 的权限接入是本轮收口任务之一；M2/M4 仍为内存实现。运行前需先启动 MySQL 8.4 并建好 `student_platform` 库与账号。涉及通用接口变更时，先在主分支协商，避免六个分支分别修改核心文件。
