/**
 * 智能课程规划助手 —— 基于先修关系与兴趣标签的个性化课表生成系统
 * 
 * 功能：
 *   - 加载课程体系（含先修关系）
 *   - 加载用户已修课程与兴趣
 *   - 自动生成满足约束的推荐课表
 * 
 * 前沿要素：知识图谱 + 推荐系统 + AI规划
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

using namespace std;

// ================== 数据结构 ==================

struct Course {
    string id;
    string name;
    int credits;
    vector<string> prereqs;
    vector<string> tags;
};

struct UserProgress {
    string userId;
    set<string> completed;
    vector<string> interests;
    int semester;
};

// ================== 全局变量 ==================
vector<Course> g_allCourses;
map<string, Course> g_courseMap; // 快速ID查找
UserProgress g_user;
const int MAX_CREDITS = 22; // 每学期最多学分

// ================== 工具函数 ==================

// 去除字符串首尾空格
string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == string::npos) return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

// 按分隔符拆分字符串（支持逗号或空格）
vector<string> split(const string& s, char delimiter = ',') {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        token = trim(token);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// ================== 文件加载 ==================

void loadCoursesFromFile() {
    ifstream file("courses.txt");
    if (!file.is_open()) {
        cerr << "错误：无法打开 courses.txt\n";
        // 提供内置课程确保可运行
        g_allCourses = {
            {"CS101", "程序设计基础", 4, {}, {"AI", "编程"}},
            {"CS102", "数据结构", 3, {"CS101"}, {"AI", "算法"}},
            {"CS103", "计算机组成", 3, {"CS101"}, {"系统", "硬件"}},
            {"CS201", "操作系统", 3, {"CS102", "CS103"}, {"系统"}},
            {"CS202", "机器学习导论", 3, {"CS102", "MATH201"}, {"AI", "数据科学"}},
            {"MATH201", "概率统计", 3, {}, {"数学", "AI"}},
            {"MATH202", "线性代数", 3, {}, {"数学"}}
        };
        for (auto& c : g_allCourses) {
            g_courseMap[c.id] = c;
        }
        cout << "使用内置课程数据。\n";
        return;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // 复制原始行用于处理
        string originalLine = line;
        
        // 提取ID
        size_t space1 = line.find(' ');
        if (space1 == string::npos) continue;
        string id = line.substr(0, space1);
        line = line.substr(space1 + 1);
        
        // 提取学分（从行尾开始找）
        size_t space2 = line.rfind(' ');
        if (space2 == string::npos) continue;
        string tagStr = line.substr(space2 + 1);
        line = line.substr(0, space2);
        
        // 提取先修课程
        size_t space3 = line.rfind(' ');
        if (space3 == string::npos) continue;
        string prereqStr = line.substr(space3 + 1);
        line = line.substr(0, space3);
        
        // 提取学分
        size_t space4 = line.rfind(' ');
        if (space4 == string::npos) continue;
        string creditStr = line.substr(space4 + 1);
        
        // 提取课程名称
        string name = line.substr(0, space4);
        
        // 转换学分
        int credits = stoi(creditStr);
        
        // 创建课程对象
        Course c;
        c.id = id;
        c.name = name;
        c.credits = credits;
        if (!prereqStr.empty() && prereqStr != " ") {
            c.prereqs = split(prereqStr, ',');
        }
        if (!tagStr.empty()) {
            c.tags = split(tagStr, ',');
        }
        
        g_allCourses.push_back(c);
        g_courseMap[c.id] = c;
    }
    file.close();
    cout << "成功加载 " << g_allCourses.size() << " 门课程。\n";
}

void loadUserProgress() {
    ifstream file("user_progress.txt");
    if (!file.is_open()) {
        cout << "未找到 user_progress.txt，使用默认用户配置。\n";
        g_user.userId = "U1001";
        g_user.completed = {"CS101", "MATH201"};
        g_user.interests = {"AI", "数据科学"};
        g_user.semester = 3;
        return;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        if (g_user.userId.empty()) {
            g_user.userId = trim(line);
        } else if (line.find("已修课程:") != string::npos) {
            string part = line.substr(line.find(':') + 1);
            vector<string> ids = split(part, ',');
            for (string id : ids) {
                g_user.completed.insert(id);
            }
        } else if (line.find("兴趣标签:") != string::npos) {
            string part = line.substr(line.find(':') + 1);
            g_user.interests = split(part, ',');
        } else if (line.find("当前学期:") != string::npos) {
            g_user.semester = stoi(line.substr(line.find(':') + 1));
        }
    }
    file.close();
    cout << "加载用户 [" << g_user.userId << "] 进度完成。\n";
}

// ================== 核心逻辑：先修检查 ==================

bool hasAllPrerequisites(const Course& course) {
    for (const string& pre : course.prereqs) {
        if (g_user.completed.find(pre) == g_user.completed.end()) {
            return false;
        }
    }
    return true;
}

// ================== 核心逻辑：兴趣匹配评分 ==================

double calculateInterestScore(const Course& course) {
    if (g_user.interests.empty()) return 0.0;
    int matchCount = 0;
    for (const string& tag : course.tags) {
        for (const string& interest : g_user.interests) {
            if (tag == interest) {
                matchCount++;
                break;
            }
        }
    }
    return static_cast<double>(matchCount) / g_user.interests.size();
}

// ================== 核心逻辑：回溯生成课表组合 ==================

void backtrack(
    int index,
    const vector<Course>& candidates,
    int currentCredits,
    vector<Course>& currentPlan,
    vector<vector<Course>>& allPlans
) {
    // 超过学分上限
    if (currentCredits > MAX_CREDITS) {
        return;
    }

    // 到达末尾，保存方案
    if (index == static_cast<int>(candidates.size())) {
        allPlans.push_back(currentPlan);
        return;
    }

    // 选择1：不选当前课程
    backtrack(index + 1, candidates, currentCredits, currentPlan, allPlans);

    // 选择2：选当前课程（如果学分允许）
    if (currentCredits + candidates[index].credits <= MAX_CREDITS) {
        currentPlan.push_back(candidates[index]);
        backtrack(index + 1, candidates, currentCredits + candidates[index].credits, currentPlan, allPlans);
        currentPlan.pop_back(); // 回溯
    }
}

vector<vector<Course>> generateAllValidSchedules(const vector<Course>& available) {
    vector<vector<Course>> allPlans;
    vector<Course> current;
    backtrack(0, available, 0, current, allPlans);
    return allPlans;
}

// ================== 核心逻辑：推荐排序 ==================

double calculatePlanScore(const vector<Course>& plan) {
    if (plan.empty()) return 0.0;
    double totalInterest = 0.0;
    int totalCredits = 0;
    for (const auto& c : plan) {
        totalInterest += calculateInterestScore(c);
        totalCredits += c.credits;
    }
    double creditUtilization = static_cast<double>(totalCredits) / MAX_CREDITS;
    // 综合得分 = 兴趣分均值 * 0.7 + 学分利用率 * 0.3
    return (totalInterest / plan.size()) * 0.7 + creditUtilization * 0.3;
}

vector<vector<Course>> getTopRecommendations(int topN = 3) {
    // Step 1: 筛选可修课程
    vector<Course> available;
    for (const auto& c : g_allCourses) {
        if (g_user.completed.count(c.id)) continue; // 已修
        if (hasAllPrerequisites(c)) {
            available.push_back(c);
        }
    }

    if (available.empty()) {
        return {};
    }

    // Step 2: 生成所有合法组合
    vector<vector<Course>> allPlans = generateAllValidSchedules(available);

    if (allPlans.empty()) {
        return {};
    }

    // Step 3: 按综合得分排序
    sort(allPlans.begin(), allPlans.end(), [&](const vector<Course>& a, const vector<Course>& b) {
        return calculatePlanScore(a) > calculatePlanScore(b);
    });

    // Step 4: 返回前 topN
    if (allPlans.size() > (size_t)topN) {
        allPlans.resize(topN);
    }
    return allPlans;
}

// ================== 辅助显示函数 ==================

void displayCourseList(const vector<Course>& courses) {
    if (courses.empty()) {
        cout << "无课程。\n";
        return;
    }
    cout << left << setw(10) << "课程ID"
         << setw(20) << "课程名称"
         << setw(8) << "学分"
         << "领域标签\n";
    cout << string(55, '-') << "\n";
    for (const auto& c : courses) {
        cout << left << setw(10) << c.id
             << setw(20) << c.name
             << setw(8) << c.credits;
        for (size_t i = 0; i < c.tags.size(); ++i) {
            if (i > 0) cout << ", ";
            cout << c.tags[i];
        }
        cout << "\n";
    }
}

void displayRecommendations() {
    cout << "\n=== 🤖 AI课表推荐 ===\n";
    auto recommendations = getTopRecommendations(3);

    if (recommendations.empty()) {
        cout << "暂无可推荐课表。可能原因：\n";
        cout << "- 所有课程已修完\n";
        cout << "- 无满足先修条件的课程\n";
        return;
    }

    for (size_t i = 0; i < recommendations.size(); ++i) {
        const auto& plan = recommendations[i];
        int totalCredits = 0;
        double avgInterest = 0.0;
        for (const auto& c : plan) {
            totalCredits += c.credits;
            avgInterest += calculateInterestScore(c);
        }
        avgInterest /= plan.size();

        cout << "\n【推荐方案 #" << (i+1) << "】\n";
        cout << "总学分: " << totalCredits << "/" << MAX_CREDITS 
             << " | 兴趣匹配度: " << fixed << setprecision(2) << avgInterest << "\n";
        displayCourseList(plan);
    }
}

void showMainMenu() {
    cout << "\n============================\n";
    cout << "   智能课程规划助手\n";
    cout << "   用户: " << g_user.userId << "\n";
    cout << "============================\n";
    cout << "1. 查看可修课程\n";
    cout << "2. 获取AI课表推荐\n";
    cout << "3. 显示所有课程\n";
    cout << "4. 显示我的进度\n";
    cout << "0. 退出\n";
    cout << "请选择: ";
}

void displayMyProgress() {
    cout << "\n=== 我的学习进度 ===\n";
    cout << "已修课程:\n";
    for (const string& id : g_user.completed) {
        auto it = g_courseMap.find(id);
        if (it != g_courseMap.end()) {
            cout << "  - " << it->second.name << " (" << id << ")\n";
        } else {
            cout << "  - " << id << " (未知课程)\n";
        }
    }
    cout << "兴趣标签: ";
    for (size_t i = 0; i < g_user.interests.size(); ++i) {
        if (i > 0) cout << ", ";
        cout << g_user.interests[i];
    }
    cout << "\n";
}

// ================== 主函数 ==================

int main() {
    cout << "欢迎使用 智能课程规划助手！\n";
    cout << "本系统融合知识图谱、推荐系统与AI规划思想...\n";

    // 加载数据
    loadCoursesFromFile();
    loadUserProgress();

    int choice;
    do {
        showMainMenu();
        cin >> choice;

        switch (choice) {
            case 1: {
                vector<Course> available;
                for (const auto& c : g_allCourses) {
                    if (g_user.completed.count(c.id)) continue;
                    if (hasAllPrerequisites(c)) {
                        available.push_back(c);
                    }
                }
                cout << "\n=== 可修课程列表 ===\n";
                displayCourseList(available);
                break;
            }
            case 2:
                displayRecommendations();
                break;
            case 3:
                cout << "\n=== 所有课程 ===\n";
                displayCourseList(g_allCourses);
                break;
            case 4:
                displayMyProgress();
                break;
            case 0:
                cout << "感谢使用！祝学业顺利！\n";
                break;
            default:
                cout << "无效选项，请重试。\n";
        }
    } while (choice != 0);

    return 0;
}
