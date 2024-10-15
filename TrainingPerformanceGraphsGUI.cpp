#include "pch.h"
#include "TrainingPerformanceGraphs.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <set>

using namespace std;

struct TrainingSessionData {
    string trainingPackCode;
    string startTime;
    string endTime;
    int shotAttempts = 0;
    int totalPackShots = -1;
    //TODO: continue updating here
};

vector<TrainingSessionData> ReadTrainingSessionData(const string& filePath);
void RenderLineChart(const vector<TrainingSessionData>& sessions, const string& selectedPack);
void GenerateTestCSV(const string& filePath);

//kinda like main() for our gui
void TrainingPerformanceGraphs::RenderSettings() {
    ImGui::TextUnformatted("Training Performance Graphs");

    //checkbox that will enable demo
    static bool show_demo_window = false;
    ImGui::Checkbox("Show ImPlot Demo", &show_demo_window);
    if (show_demo_window) {
        ImPlot::ShowDemoWindow(&show_demo_window);
    }

    ImGui::Separator();

    //checkbox that will allow us to using randomly generated test data (by not checking we use our data by default)
    static bool show_test_data = false;
    bool checkbox_changed = ImGui::Checkbox("Show Test Data?", &show_test_data);
    string dataFolder = gameWrapper->GetDataFolder().string();
    string filePath;
    if (show_test_data) {
        filePath = dataFolder + "/test_data.csv";
        if (checkbox_changed) {
            GenerateTestCSV(filePath);
        }
    }
    else {
        filePath = dataFolder + "/training_sessions.csv";
    }
    vector<TrainingSessionData> sessions = ReadTrainingSessionData(filePath);
    
    ImGui::Separator();
    
    if (sessions.empty()) {
        ImGui::Text("No data available to display.");
        return;
    }

    //get training pack codes
    set<string> uniquePackCodes;
    for (const auto& session : sessions) {
        uniquePackCodes.insert(session.trainingPackCode);
    }
    vector<string> packOptions(uniquePackCodes.begin(), uniquePackCodes.end());
    static string selectedPack = packOptions.empty() ? "" : packOptions[0];
    
    //drop down
    if (ImGui::BeginCombo("Select Training Pack", selectedPack.c_str())) {
        for (const auto& pack : packOptions) {
            bool isSelected = (selectedPack == pack);
            if (ImGui::Selectable(pack.c_str(), isSelected)) {
                selectedPack = pack;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    
    //graph
    RenderLineChart(sessions, selectedPack);
    ImGui::Separator();
}

//for plugin settings window
string TrainingPerformanceGraphs::GetPluginName() {
    return "TrainingPerformanceGraphs";
}

//read csv file
vector<TrainingSessionData> ReadTrainingSessionData(const string& filePath) {
    vector<TrainingSessionData> sessions;
    
    ifstream file(filePath);

    if (!file.is_open()) {
        LOG("Unable to open file: " + filePath);
        return sessions;
    }

    string line;
    //ignore header
    getline(file, line);

    while (getline(file, line)) {
        stringstream ss(line);
        string item;
        TrainingSessionData session;

        //parse
        getline(ss, session.trainingPackCode, ',');
        getline(ss, session.startTime, ',');
        getline(ss, session.endTime, ',');
        getline(ss, item, ',');
        session.shotAttempts = stoi(item);
        getline(ss, item, ',');
        session.totalPackShots = stoi(item);

        sessions.push_back(session);
    }

    file.close();
    return sessions;
}

//render line and dot chart
void RenderLineChart(const vector<TrainingSessionData>& sessions, const string& selectedPack) {
    //allows selected pack
    vector<TrainingSessionData> filteredSessions;
    for (const auto& session : sessions) {
        if (session.trainingPackCode == selectedPack) {
            filteredSessions.push_back(session);
        }
    }

    //handles if no data
    int num_sessions = static_cast<int>(filteredSessions.size());
    if (num_sessions == 0) {
        ImGui::Text("No data available for this Training Pack.");
        return;
    }

    //prepare data
    vector<float> shotAttempts(num_sessions);
    vector<float> totalPackShots(num_sessions);
    float max_value = 0.0f;

    for (int i = 0; i < num_sessions; ++i) {
        shotAttempts[i] = static_cast<float>(filteredSessions[i].shotAttempts);
        totalPackShots[i] = static_cast<float>(filteredSessions[i].totalPackShots);

        //calculate if max shots is attempts or totalpackshots 
        max_value = max(max_value, max(shotAttempts[i], totalPackShots[i]));
    }

    //create y-axis limit max shot value + 33% padding 
    float y_max_limit = max_value * 1.33f;

    //create x-axis limit adding +1 to each sides (0-11)
    float x_min_limit = 0.0f; // Padding to the left of the first session
    float x_max_limit = max(11.0f, static_cast<float>(num_sessions + 1)); //TODO: expand above 10 training sessions

    //set limits
    ImPlot::SetNextPlotLimits(x_min_limit, x_max_limit, 0, y_max_limit, ImGuiCond_Always);

    if (ImPlot::BeginPlot("Training Pack Performance", "Training Session", "Shots", ImVec2(-1, 0))) {
        vector<float> x_values(num_sessions);
        for (int i = 0; i < num_sessions; ++i) {
            x_values[i] = static_cast<float>(i + 1); //start from 1 for space
        }

        //shot attempt is red circle
        ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Circle);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1, 0, 0, 1)); //red
        ImPlot::PlotLine("Attempts", x_values.data(), shotAttempts.data(), num_sessions);
        ImPlot::PopStyleColor();
        ImPlot::PopStyleVar();

        //total pack shots is blue square
        ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Square);
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0, 0, 1, 1)); //blue
        ImPlot::PlotLine("Total Shots", x_values.data(), totalPackShots.data(), num_sessions);
        ImPlot::PopStyleColor();
        ImPlot::PopStyleVar();

        ImPlot::EndPlot();
    }
}

//generate random test data
void GenerateTestCSV(const string& filePath) {
    ofstream file(filePath);
    if (!file.is_open()) {
        LOG("Unable to create file: " + filePath);
        return;
    }

    file << "trainingPackCode,startTime,endTime,shotAttempts,totalPackShots\n";

    string packCodes[] = { "PACK1", "PACK2", "PACK3", "PACK4", "PACK5", "PACK6", "PACK7", "PACK8", "PACK9", "PACK10" };
    int totalPacks = 10;

    tm date = {};
    date.tm_year = 2024 - 1900; // Year since 1900
    date.tm_mon = 1;            
    date.tm_mday = 1;           

    for (int i = 0; i < 100; ++i) {
        // Cycle through pack codes
        string packCode = packCodes[i % totalPacks];

        // Increment date every 10 entries
        if (i % 10 == 0 && i != 0) {
            date.tm_mday += 1;
        }

        // Format start and end times
        date.tm_hour = 8 + (i % 5) * 2;     // Sessions at different times
        date.tm_min = (i % 2) * 30;         // 0 or 30 minutes
        date.tm_sec = 0;
        time_t startTime = mktime(&date);

        date.tm_min += 30 + (rand() % 16);  // Session length between 30 and 45 minutes
        time_t endTime = mktime(&date);

        // Convert times to strings
        char startStr[20];
        char endStr[20];
        strftime(startStr, sizeof(startStr), "%m/%d/%Y %H:%M:%S", localtime(&startTime));
        strftime(endStr, sizeof(endStr), "%m/%d/%Y %H:%M:%S", localtime(&endTime));

        // Random shot attempts and total shots
        int shotAttempts = 45 + rand() % 30;   // Between 45 and 75
        int totalPackShots = 15 + rand() % 16; // Between 15 and 30

        // Write to CSV
        file << packCode << ','
            << startStr << ','
            << endStr << ','
            << shotAttempts << ','
            << totalPackShots << '\n';
    }

    file.close();
}