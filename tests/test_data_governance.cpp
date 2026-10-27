#include <cassert>
#include <iostream>
#include "platform/app.hpp"
#include "data_governance/types.h"

using namespace platform;
using namespace data_governance;

void test_list_quality_rules() {
    App app = make_app();
    register_data_governance(app);
    
    Request req;
    req.method = "GET";
    req.path = "/api/modules/data-governance/quality-rules";
    
    Response resp = app.handle(req);
    assert(resp.status == 200);
    std::cout << "[PASS] GET /quality-rules" << std::endl;
}

void test_get_student_master() {
    App app = make_app();
    register_data_governance(app);
    
    Request req;
    req.method = "GET";
    req.path = "/api/modules/data-governance/student-master/1001";
    req.path_params["student_id"] = "1001";
    
    Response resp = app.handle(req);
    assert(resp.status == 200);
    std::cout << "[PASS] GET /student-master/:id" << std::endl;
}

void test_recalculate_metrics() {
    App app = make_app();
    register_data_governance(app);
    
    Request req;
    req.method = "POST";
    req.path = "/api/modules/data-governance/recalculate";
    
    Response resp = app.handle(req);
    assert(resp.status == 202);
    std::cout << "[PASS] POST /recalculate" << std::endl;
}

void test_get_quality_report() {
    App app = make_app();
    register_data_governance(app);
    
    Request req;
    req.method = "GET";
    req.path = "/api/modules/data-governance/quality-report";
    
    Response resp = app.handle(req);
    assert(resp.status == 200);
    std::cout << "[PASS] GET /quality-report" << std::endl;
}

int main() {
    std::cout << "Running data governance module tests..." << std::endl;
    test_list_quality_rules();
    test_get_student_master();
    test_recalculate_metrics();
    test_get_quality_report();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
