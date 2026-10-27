#include "modules/data_ingestion/sqlite_repository.hpp"
#include "modules/student_profile/sqlite_repository.hpp"
#include "platform/db.hpp"
#include "platform/json.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

void require(bool condition, const std::string& message = "assertion failed") {
    if (!condition) throw std::runtime_error(message);
}

std::string make_temp_db_path() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() /
            ("student_platform_test_" + std::to_string(stamp) + ".db"))
        .string();
}

void remove_database_files(const std::string& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path + "-wal", error);
    std::filesystem::remove(path + "-shm", error);
}

void test_json() {
    using platform::Json;

    const auto value = Json::parse(R"({
        "name": "张三",
        "age": 20,
        "score": 92.5,
        "active": true,
        "tags": ["leadership", "teamwork"],
        "extra": null
    })");
    require(value.is_object(), "json root should be an object");
    require(value.at("name").as_string() == "张三", "json string");
    require(value.at("age").as_int64() == 20, "json integer");
    require(value.at("score").as_double() == 92.5, "json double");
    require(value.at("active").as_bool(), "json bool");
    require(value.at("tags").is_array(), "json array");
    require(value.at("tags").at(1).as_string() == "teamwork", "json nested array");
    require(value.at("extra").is_null(), "json null");

    const auto roundtrip = Json::parse(value.dump());
    require(roundtrip.at("name").as_string() == "张三", "json roundtrip");
    require(roundtrip.at("tags").at(0).as_string() == "leadership", "json array roundtrip");

    require(Json::parse(R"("\u4f60\u597d")").as_string() == "你好", "unicode escape");
    require(Json::parse(R"("line\nquote\"")").as_string() == "line\nquote\"", "string escape");

    Json object = Json::object();
    object["id"] = 7;
    object["name"] = "李四";
    require(object.dump() == R"({"id":7,"name":"李四"})", "object dump");

    bool rejected = false;
    try {
        Json::parse("{");
    } catch (const platform::JsonError&) {
        rejected = true;
    }
    require(rejected, "invalid json should be rejected");
}

void test_ingestion_repository(platform::Database& database) {
    using namespace platform::data_ingestion;

    SqliteIngestionRepository repository(database);

    DataSource source;
    source.name = "教务系统成绩";
    source.kind = "academic";
    source.access_mode = "file";
    source.owner = "教务处";
    source.description = "每学期成绩单样例文件";
    const auto source_id = repository.create_data_source(source);
    require(source_id > 0, "data source id");

    const auto sources = repository.list_data_sources();
    require(sources.size() == 1, "data source count");
    require(sources.front().kind == "academic", "data source kind");
    require(sources.front().created_at.empty() == false, "data source created_at");

    const auto stored_source = repository.find_data_source(source_id);
    require(stored_source.has_value(), "find data source");
    require(stored_source->owner == "教务处", "data source owner");

    FieldMapping mapping;
    mapping.source_field = "xh";
    mapping.standard_field = "student_id";
    mapping.transform_expr = "trim";
    repository.replace_field_mappings(source_id, {mapping});
    const auto mappings = repository.list_field_mappings(source_id);
    require(mappings.size() == 1, "field mapping count");
    require(mappings.front().standard_field == "student_id", "field mapping target");

    ImportTask task;
    task.data_source_id = source_id;
    task.name = "2026 秋季成绩导入";
    task.import_mode = "manual";
    task.validation_rules = R"({"student_id":{"required":true}})";
    const auto task_id = repository.create_import_task(task);
    require(task_id > 0, "import task id");

    const auto stored_task = repository.find_import_task(task_id);
    require(stored_task.has_value(), "find import task");
    require(stored_task->status == "pending", "import task default status");

    const auto run_id = repository.start_import_run(task_id);
    require(run_id > 0, "import run id");

    RawRecord good;
    good.run_id = run_id;
    good.source_row_no = 1;
    good.payload = R"({"student_id":"2023001","score":92})";
    good.valid = true;
    repository.add_raw_record(good);

    RawRecord bad;
    bad.run_id = run_id;
    bad.source_row_no = 2;
    bad.payload = R"({"student_id":"","score":80})";
    bad.valid = false;
    bad.error_reason = "student_id is empty";
    repository.add_raw_record(bad);

    repository.finish_import_run(run_id, "partial", 2, 1, 1);
    const auto run = repository.find_import_run(run_id);
    require(run.has_value(), "find import run");
    require(run->status == "partial", "import run status");
    require(run->total_count == 2 && run->success_count == 1 && run->failure_count == 1,
            "import run counts");
    require(run->finished_at.empty() == false, "import run finished_at");

    const auto all_records = repository.list_raw_records(run_id);
    require(all_records.size() == 2, "raw record count");
    const auto failed_records = repository.list_raw_records(run_id, true);
    require(failed_records.size() == 1, "failed raw record count");
    require(failed_records.front().error_reason == "student_id is empty", "failure reason");

    repository.update_import_task_status(task_id, "partial");
    require(repository.find_import_task(task_id)->status == "partial", "task status update");
}

void test_profile_repository(platform::Database& database) {
    using namespace platform::student_profile;

    SqliteProfileRepository repository(database);

    Student student;
    student.student_id = "2023001";
    student.name = "张三";
    student.major = "软件工程";
    student.class_name = "软件2301";
    student.grade = 2023;
    repository.upsert_student(student);

    student.name = "张三（更新）";
    repository.upsert_student(student);

    const auto stored_student = repository.find_student("2023001");
    require(stored_student.has_value(), "find student");
    require(stored_student->name == "张三（更新）", "upsert student");
    require(repository.list_students().size() == 1, "student count");

    MetricValue metric;
    metric.student_id = "2023001";
    metric.metric_code = "comprehensive_score";
    metric.term = "2026-秋";
    metric.value = 92.0;
    metric.unit = "分";
    metric.source = "academic";
    repository.upsert_metric(metric);

    metric.value = 95.0;
    repository.upsert_metric(metric);

    const auto metrics = repository.list_metrics("2023001", "2026-秋");
    require(metrics.size() == 1, "metric count");
    require(metrics.front().value == 95.0, "metric upsert");
    require(repository.list_metrics("2023001", "2027-春").empty(), "metric term filter");

    Tag tag;
    tag.code = "comprehensive_score_excellent";
    tag.name = "综合成绩优秀";
    tag.dimension = "学业";
    tag.description = "综合成绩位于专业前 20%";
    repository.upsert_tag(tag);

    const auto stored_tag = repository.find_tag(tag.code);
    require(stored_tag.has_value(), "find tag");
    require(stored_tag->dimension == "学业", "tag dimension");

    StudentTag student_tag;
    student_tag.tag_code = tag.code;
    student_tag.evidence = R"({"metric":"comprehensive_score","value":95,"term":"2026-秋"})";
    repository.replace_student_tags("2023001", {student_tag});

    const auto student_tags = repository.list_student_tags("2023001");
    require(student_tags.size() == 1, "student tag count");
    require(student_tags.front().evidence.find("95") != std::string::npos, "tag evidence");

    ProfileSnapshot snapshot;
    snapshot.student_id = "2023001";
    snapshot.term = "2026-秋";
    snapshot.payload = R"({"metrics":{"comprehensive_score":95},"tags":["comprehensive_score_excellent"]})";
    repository.save_snapshot(snapshot);

    const auto stored_snapshot = repository.find_snapshot("2023001", "2026-秋");
    require(stored_snapshot.has_value(), "find snapshot");
    require(stored_snapshot->payload.find("comprehensive_score") != std::string::npos,
            "snapshot payload");
    require(repository.find_snapshot("2023001", "2027-春").has_value() == false,
            "snapshot term filter");
}

void test_storage(const std::string& path) {
    {
        platform::Database database(path);
        test_ingestion_repository(database);
        test_profile_repository(database);
    }
}

}  // namespace

int main() {
    const std::string path = make_temp_db_path();
    try {
        test_json();
        test_storage(path);
    } catch (const std::exception& error) {
        remove_database_files(path);
        std::cerr << "storage tests failed: " << error.what() << '\n';
        return 1;
    }
    remove_database_files(path);
    std::cout << "storage tests passed\n";
    return 0;
}
