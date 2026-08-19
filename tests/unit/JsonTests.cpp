#include "TestHarness.hpp"

#include "services/api/TinyJson.hpp"

TEST_CASE("tiny JSON parses nested API fields and escaped Unicode") {
    clockin::TinyJson json(
        R"({"success":true,"message":"ok","user":{"name":"Jos\u00e9"},"dates":["2026-08-18","2026-08-19"]})");
    clockin::TinyJson::ObjectRange root;
    clockin::TinyJson::ObjectRange user;
    bool success = false;
    std::string name;
    std::vector<std::string> dates;
    CHECK(json.RootObject(root));
    CHECK(json.GetBool(root, "success", success));
    CHECK(success);
    CHECK(json.GetObject(root, "user", user));
    CHECK(json.GetString(user, "name", name));
    CHECK(name == "José");
    CHECK(json.GetStringArray(root, "dates", dates));
    CHECK(dates.size() == 2);
}

TEST_CASE("tiny JSON rejects malformed input") {
    clockin::TinyJson json("{\"success\": tru");
    clockin::TinyJson::ObjectRange root;
    CHECK(!json.RootObject(root));
}
