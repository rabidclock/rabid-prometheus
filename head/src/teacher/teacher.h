#pragma once

#include <memory>
#include <string>

namespace prometheus {

// Teacher interface — calls the Gemini API to grade daily logs
// and produce "Lessons" that get injected into context on wake.
class Teacher {
public:
    explicit Teacher(const std::string& api_base_url);
    ~Teacher();

    Teacher(const Teacher&) = delete;
    Teacher& operator=(const Teacher&) = delete;

    // Send a session log to Gemini for evaluation.
    // Returns a distilled "lesson" string.
    std::string grade_log(const std::string& log);

    // Request a "Walled Garden" calming passage (Mr. Rogers content).
    std::string request_calm();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prometheus
