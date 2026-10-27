#include "data_governance/student_master.h"
#include "data_governance/types.h"
#include "data_governance/response_utils.h"

namespace data_governance {

platform::Response handle_get_student_master(const platform::Request& req) {
    std::string student_id_str = req.path_params.at("student_id");
    int student_id = std::stoi(student_id_str);
    
    for (const auto& student : mock_student_master) {
        if (student.student_id == student_id) {
            json data;
            data["student_id"] = student.student_id;
            data["student_no"] = student.student_no;
            data["name"] = student.name;
            data["major"] = student.major;
            data["class"] = student.class_name;
            data["status"] = student.status;
            return success_response(data);
        }
    }
    
    return error_response(404, "学生主数据不存在");
}

} // namespace data_governance
