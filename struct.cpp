// 一门课程
struct Course {
    string id;                // 如 "CS101"
    string name;              // 如 "程序设计基础"
    int credits;              // 学分
    vector<string> prereqs;   // 先修课程ID列表
    vector<string> tags;      // 领域标签，如 {"AI", "编程"}
};

// 用户状态
struct UserProgress {
    string userId;
    set<string> completed;        // 已修课程ID集合
    vector<string> interests;     // 兴趣标签
    int currentSemester;          // 当前学期（用于扩展，本版未用）
};
