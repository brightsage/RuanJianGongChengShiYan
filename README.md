# 高校学生成长与职业发展数据中台

这是供六个模块并行开发的 C++ 工程骨架。数据采集与接入、学生画像两个模块已经实现可运行的 HTTP 接口和 SQLite 持久化；其余四个模块仍是注册骨架。需求范围见《实验一报告》。

## 环境与运行

- Windows（Winsock）或 Linux（POSIX socket）
- 支持 C++17 的编译器、CMake 3.21 或更新版本
- Python 3 可选，用于 HTTP 冒烟测试和演示数据脚本
- SQLite 3.54.1 以源码形式内置在 `third_party/sqlite/`，不需要额外安装数据库服务

Linux：

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/linux-debug/platform_server 8090
```

Windows / VS Code：安装 CMake Tools 与 C/C++ 扩展，打开项目后选择 `windows-vs2026` preset，`F7` 构建、`F5` 运行。命令行等价操作：

```powershell
cmake --preset windows-vs2026
cmake --build --preset windows-vs2026
ctest --preset windows-vs2026
.\build\windows-vs2026\Debug\platform_server.exe 8090
```

如果 Visual Studio 报找不到 `cl.exe` 或 `cmake.exe`，打开 Visual Studio Installer → 修改 → 勾选“使用 C++ 的桌面开发”和“C++ CMake tools for Windows”；也可以用任意 MinGW-w64 GCC 13+ 构建：

```powershell
cmake -S . -B build/mingw-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw-debug
ctest --test-dir build/mingw-debug --output-on-failure
```

打开 <http://127.0.0.1:8090/>。服务只监听本机回环地址；端口参数可改，省略时为 8080。测试台列出已经注册的模块和路由，并允许填写方法、路径、请求头和请求体，查看实际 HTTP 响应。页面里的“示例画像”会自动加载张三的画像；也可以点“加载张三的画像”重新加载。点击路由后，路径里的 `:student_id`、`:source_id` 等要换成真实 ID 再发送。它是开发工具，不是最终产品前端。

## 已实现接口

公共接口：

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| GET | `/api/health` | 检查服务是否启动 |
| GET | `/api/modules` | 列出六个模块及各自已注册的路由 |

数据采集与接入（前缀 `/api/modules/data-ingestion`）：

| 方法 | 路径 | 用例 |
| --- | --- | --- |
| POST | `/sources` | UC-DC-01 登记数据源 |
| GET | `/sources`、`/sources/:id` | 查询数据源 |
| PUT | `/sources/:id/mappings` | UC-DC-07 维护字段映射 |
| POST | `/tasks` | UC-DC-02 配置导入任务 |
| PUT | `/tasks/:id/schedule` | UC-DC-06 保存调度配置（暂不启动定时器） |
| POST | `/tasks/:id/run` | UC-DC-03 执行导入，可带 `idempotency_key` |
| GET | `/runs/:id`、`/runs/:id/records?failed=1` | UC-DC-04 查看结果与失败明细 |
| POST | `/runs/:id/retry` | 只重跑失败记录，可带修正后的 `records` |
| POST | `/push` | UC-DC-05 接收外部推送，可带 `idempotency_key` |

学生画像（前缀 `/api/modules/student-profile`）：

| 方法 | 路径 | 用例 |
| --- | --- | --- |
| POST | `/students` | 同步学生读模型（由治理模块调用） |
| GET | `/students?major=&class=` | UC-SP-07 按范围查看学生 |
| GET | `/students/:id?term=` | UC-SP-03 查看个人画像 |
| POST | `/metrics` | 同步指标值（由治理模块调用） |
| POST/GET | `/tags` | UC-SP-01 维护标签体系 |
| POST | `/recalculate` | UC-SP-02 单个学生计算指标、标签与快照 |
| POST | `/recalculate/batch` | UC-SP-02 按学期批量重算（不传 `student_ids` 则全部学生） |
| GET | `/students/:id/tags/:code/evidence?term=` | UC-SP-04 查看标签依据 |
| GET | `/students/:id/compare?peer=&term=` | UC-SP-05 对比画像 |
| GET | `/students/:id/report?term=` | UC-SP-06 HTML 报告（默认页面展示，加 `download=1` 下载） |

未注册的地址返回 404，不提供模拟成功结果。标签规则支持 `>=`、`>`、`<=`、`<`、`==`、`!=`、`between`，例如：

```json
{"metric":"comprehensive_score","operator":">=","value":90}
```

导入任务的校验规则支持 `required`、`types`（number/string/boolean）和 `ranges`（min/max）；字段映射支持 `trim`、`lower`、`upper`、`number` 四种转换。

执行导入或外部推送时可以带 `idempotency_key`：同一个任务重复提交同一个 key 会返回第一次的执行结果，不会重复入库。失败记录用 `POST /runs/:id/retry` 重跑；不带 `records` 时重跑原始失败行，带 `records` 时用修正后的数据。

## 演示数据

先启动服务（例如 8090），再运行：

```powershell
py -3 scripts/seed_demo.py 8090
```

脚本会写入一个数据源、字段映射、导入任务、两名学生、指标、标签和画像快照，并打印画像、报告和对比地址。

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

- `include/platform/json.hpp`：轻量 JSON 解析与输出。
- `include/platform/api.hpp`：查询参数、路径参数、JSON 请求体解析、统一错误响应。
- `include/platform/db.hpp`：SQLite 薄封装，提供 `exec`、`query`、参数绑定和事务。
- 默认数据库文件为 `data/platform.db`，可用环境变量 `PLATFORM_DB_PATH` 覆盖；数据库文件不提交。
- 表名按模块加前缀（数据接入 `ing_`、学生画像 `sp_`），各模块在自己的仓储构造函数里执行 `CREATE TABLE IF NOT EXISTS`。
- 分层为“路由（module.cpp）→ 业务（service.cpp）→ 仓储（repository）→ SQLite”，详细说明见 [`docs/模块框架说明.md`](docs/模块框架说明.md)。

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

新增业务接口时，同时添加能验证其行为的测试。`tests/app_test.cpp` 演示应用层测试，`tests/module_api_test.cpp` 覆盖两个模块的完整业务流，`tests/http_smoke.py` 验证真实 HTTP 边界。

## 当前边界

- 数据治理、学业预警、职业推荐、数据服务与权限四个模块尚未实现。
- 调度配置只保存到数据库，没有后台定时器；鉴权、脱敏和 PDF 报告留待后续迭代。
- 标签和画像快照都按学期保存；旧库启动时会自动迁移 `sp_student_tag.term` 和 `ing_import_run.idempotency_key` 两列。
- 这个轻量服务只用于本地联调：单线程、每个连接只处理一个请求、请求体上限 1 MiB，不支持分块传输、TLS 或生产部署。涉及通用接口变更时，先在主分支协商，避免六个分支分别修改核心文件。
