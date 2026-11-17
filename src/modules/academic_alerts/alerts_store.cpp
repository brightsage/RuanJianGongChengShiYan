#include "alerts.hpp"

#include <chrono>
#include <ctime>

// ============================================================================
// M4 学业分析与预警 · 数据与领域逻辑
//
// 当前是进程内内存实现，方便实验阶段快速联调。
// 后续接入数据库时，只需要替换本文件里 students()/rules()/alerts() 的读写方式，
// 以及种子数据函数 seed_*()，接口层（alerts_api.cpp）不受影响。
// ============================================================================

namespace platform::academic_alerts {

// ---------------------------------------------------------------------------
// 时间
// ---------------------------------------------------------------------------

std::int64_t now_epoch() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string iso_time(std::int64_t epoch) {
    const std::time_t raw = static_cast<std::time_t>(epoch);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &raw);
#else
    gmtime_r(&raw, &tm);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

// ---------------------------------------------------------------------------
// 种子数据
// ---------------------------------------------------------------------------

namespace {

std::vector<Student> seed_students() {
    return {
        {"2021001", "张伟", "计科2101", "计算机科学与技术", "2021", 58.4, 3, 0.52, 0.72,
         {{"2024-2025-1", 61.2}, {"2024-2025-2", 57.8}, {"2025-2026-1", 58.4}}},
        {"2021002", "李娜", "计科2101", "计算机科学与技术", "2021", 76.9, 1, 0.86, 0.93,
         {{"2024-2025-1", 74.1}, {"2024-2025-2", 75.6}, {"2025-2026-1", 76.9}}},
        {"2021003", "王强", "计科2101", "计算机科学与技术", "2021", 52.1, 4, 0.41, 0.65,
         {{"2024-2025-1", 55.6}, {"2024-2025-2", 50.3}, {"2025-2026-1", 52.1}}},
        {"2021004", "赵敏", "计科2101", "计算机科学与技术", "2021", 68.7, 0, 0.74, 0.88,
         {{"2024-2025-1", 70.2}, {"2024-2025-2", 69.5}, {"2025-2026-1", 68.7}}},
        {"2021005", "陈晨", "计科2102", "计算机科学与技术", "2021", 63.5, 2, 0.68, 0.79,
         {{"2024-2025-1", 66.8}, {"2024-2025-2", 64.1}, {"2025-2026-1", 63.5}}},
        {"2021006", "刘洋", "计科2102", "计算机科学与技术", "2021", 81.3, 0, 0.92, 0.96,
         {{"2024-2025-1", 79.4}, {"2024-2025-2", 80.6}, {"2025-2026-1", 81.3}}},
        {"2021007", "周杰", "计科2102", "计算机科学与技术", "2021", 47.8, 5, 0.33, 0.58,
         {{"2024-2025-1", 52.3}, {"2024-2025-2", 49.1}, {"2025-2026-1", 47.8}}},
        {"2021008", "吴静", "计科2102", "计算机科学与技术", "2021", 72.6, 1, 0.81, 0.91,
         {{"2024-2025-1", 71.9}, {"2024-2025-2", 72.0}, {"2025-2026-1", 72.6}}},
        {"2021009", "孙悦", "软工2101", "软件工程", "2021", 66.2, 1, 0.71, 0.77,
         {{"2024-2025-1", 68.0}, {"2024-2025-2", 67.1}, {"2025-2026-1", 66.2}}},
        {"2021010", "郑凯", "软工2101", "软件工程", "2021", 55.3, 3, 0.48, 0.69,
         {{"2024-2025-1", 58.9}, {"2024-2025-2", 56.4}, {"2025-2026-1", 55.3}}},
        {"2021011", "冯雪", "软工2101", "软件工程", "2021", 88.1, 0, 0.95, 0.98,
         {{"2024-2025-1", 86.2}, {"2024-2025-2", 87.4}, {"2025-2026-1", 88.1}}},
        {"2021012", "何磊", "软工2101", "软件工程", "2021", 61.9, 2, 0.63, 0.75,
         {{"2024-2025-1", 64.0}, {"2024-2025-2", 62.7}, {"2025-2026-1", 61.9}}},
    };
}

std::vector<Rule> seed_rules() {
    return {
        {"R1", "综合成绩不达标", "avg_score", "lt", 60.0, "red", "辅导员、学生本人", true},
        {"R2", "多门课程不及格", "failed_courses", "gte", 2.0, "orange", "辅导员", true},
        {"R3", "出勤率偏低", "attendance", "lt", 0.80, "yellow", "辅导员", true},
        {"R4", "学分完成率偏低", "credit_rate", "lt", 0.60, "orange", "辅导员、院系管理员", true},
    };
}

/// 一条三天前的历史预警，用来演示 UC-AA-06 的「超时提醒」
std::vector<Alert> seed_alerts() {
    Alert alert;
    alert.id = 1;
    alert.student_id = "2021005";
    alert.rule_id = "R2";
    alert.level = "orange";
    alert.status = "open";
    alert.created = now_epoch() - 72 * 3600;
    alert.metric_value = 2.0;
    return {std::move(alert)};
}

std::vector<Student> g_students = seed_students();
std::vector<Rule> g_rules = seed_rules();
std::vector<Alert> g_alerts = seed_alerts();
int g_next_alert_id = 2;

}  // namespace

std::vector<Student>& students() { return g_students; }
std::vector<Rule>& rules() { return g_rules; }
std::vector<Alert>& alerts() { return g_alerts; }
int next_alert_id() { return g_next_alert_id++; }

// ---------------------------------------------------------------------------
// 查询与判定
// ---------------------------------------------------------------------------

Student* find_student(const std::string& id) {
    for (auto& student : g_students)
        if (student.id == id) return &student;
    return nullptr;
}

Alert* find_alert(int id) {
    for (auto& alert : g_alerts)
        if (alert.id == id) return &alert;
    return nullptr;
}

bool is_metric(const std::string& metric) {
    return metric == "avg_score" || metric == "failed_courses" || metric == "credit_rate" ||
           metric == "attendance";
}

bool is_operator(const std::string& op) {
    return op == "lt" || op == "lte" || op == "gt" || op == "gte";
}

bool is_level(const std::string& level) {
    return level == "red" || level == "orange" || level == "yellow";
}

double metric_value(const Student& student, const std::string& metric) {
    if (metric == "avg_score") return student.avg_score;
    if (metric == "failed_courses") return static_cast<double>(student.failed_courses);
    if (metric == "credit_rate") return student.credit_rate;
    if (metric == "attendance") return student.attendance;
    return 0.0;
}

bool hit(const Rule& rule, const Student& student, double* value) {
    const double actual = metric_value(student, rule.metric);
    if (value != nullptr) *value = actual;
    if (rule.op == "lt") return actual < rule.threshold;
    if (rule.op == "lte") return actual <= rule.threshold;
    if (rule.op == "gt") return actual > rule.threshold;
    if (rule.op == "gte") return actual >= rule.threshold;
    return false;
}

bool active_status(const std::string& status) {
    return status == "open" || status == "claimed";
}

int parse_id(const std::string& text) {
    if (text.empty() || text.size() > 9) return -1;
    for (unsigned char c : text)
        if (!std::isdigit(c)) return -1;
    return std::stoi(text);
}

bool overdue(const Alert& alert) {
    return active_status(alert.status) && now_epoch() - alert.created > 48 * 3600;
}

}  // namespace platform::academic_alerts
