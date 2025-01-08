#include "pch.h"
#include "TrainingPerformanceGraphs.h"
#include "TrainingPerformanceGraphsGUI.h"
#include "GraphFunctions.h"

//initialization
bool isCacheLoaded = false;
string currentFilePath = "";
vector<TrainingSessionData> sessionsCache;

//functions=========================================================================================================================
//for 'Plugins' left panel list
string TrainingPerformanceGraphs::GetPluginName() { return "TrainingPerformanceGraphs";}
//GUI main()
void TrainingPerformanceGraphs::RenderSettings() {
    //ImGui::TextUnformatted("Training Performance Graphs");

    //display implot demo for debugging/testing
    //displayDemo();

    //static flag to ensure the data loading function is only called once
    static bool dataLoadedOnce = false;

    if (!dataLoadedOnce) {
        string dataFolder = gameWrapper->GetDataFolder().string();
        
        //generate test data file
        GenerateTestXML(dataFolder + "\\test_data.xml");
        LOG("Test data generated.");

        //append user xml data then test xml data to sessionCache
        AddSessionsToCache(dataFolder);
        dataLoadedOnce = true;
    }

    //get list of all tpcodes user has trained, display dropdown to user
    vector<pair<string, string>> trainingPacks = getTrainingPackNameAndCode(sessionsCache);
    string selectedPack = displayPackSelector(trainingPacks);

    //Graph 1: Scoring Rate
    RenderScoringRateGraph(sessionsCache, selectedPack);

    //Graph 2: Boost Used
    RenderBoostUsedGraph(sessionsCache, selectedPack);

    //Graph 3: Goal Speed
    RenderGoalSpeedGraph(sessionsCache, selectedPack);
}

//generate randomized test XML file
void GenerateTestXML(const string& filePath) {
    srand(static_cast<unsigned int>(time(nullptr)));

    ofstream file(filePath);
    if (!file.is_open()) {
        LOG("Unable to create file: " + filePath);
        return;
    }

    file << "<root>\n    <sessions>\n";
    int totalPacks = 5; //number of to be generated packs
    string packCodes[] = { "AAAA-BBBB-0000-1111", "AAAA-BBBB-0000-2222", "AAAA-BBBB-0000-3333", "AAAA-BBBB-0000-4444", "AAAA-BBBB-0000-5555" };

    float scoringBase = 0.4f;       //starting scoring probability (40%)
    float scoringGrowth = 1.02f;    //growth rate for scoring
    float goalSpeedBase = 40.0f;    //starting goal speed
    float goalSpeedGrowth = 1.02f;  //growth rate for goal speed
    float boostUsageBase = 250.0f;  //starting boost used
    float boostUsageDecay = 0.98f;  //decay rate for boost usage (improves over time)

    for (int packIndex = 0; packIndex < totalPacks; ++packIndex) {
        int sessionsForPack = 5 + (rand() % 96);  //random between 5 and 100 sessions per pack

        for (int sessionIndex = 0; sessionIndex < sessionsForPack; ++sessionIndex) {
            file << "        <session>\n"
                << "            <startTime>01/01/2024 08:00:00</startTime>\n"
                << "            <endTime>01/01/2024 08:30:00</endTime>\n"
                << "            <trainingPack>\n"
                << "                <code>" << packCodes[packIndex] << "</code>\n"
                << "                <name>Generated Example Pack " << (packIndex + 1) << "</name>\n"
                << "                <totalPackShots>10</totalPackShots>\n"
                << "            </trainingPack>\n"
                << "            <shotData>\n";

            int shots = 10; //set number of shots
            for (int shotIndex = 0; shotIndex < shots; ++shotIndex) {
                file << "                <shot id=\"" << shotIndex << "\">\n";
                int attempts = 2 + (rand() % 3); //random attempts between 2 and 4 per shot

                //get current exponential growth/decay
                float currentScoringChance = min(static_cast<float>(scoringBase * pow(scoringGrowth, sessionIndex)), 0.95f); //will approach 95% accuracy
                float currentGoalSpeed = min(static_cast<float>(goalSpeedBase * pow(goalSpeedGrowth, sessionIndex) + static_cast<float>((rand() % 50 - 25))), 140.0f); //will approach 140kph (+/-25 variance)
                float currentBoostUsed = max(static_cast<float>(boostUsageBase * pow(boostUsageDecay, sessionIndex) + static_cast<float>((rand() % 100 - 50))), 10.0f); //will approach 10 boost used (+/-50 variance)

                for (int attemptIndex = 0; attemptIndex < attempts; ++attemptIndex) {
                    //Randomly calculate per shot exponential growth/decay with variance
                    bool scored = (rand() % 100) < (currentScoringChance * 100);
                    float goalSpeed = scored ? min(currentGoalSpeed + static_cast<float>(rand() % 50 - 25), 140.0f) : 0.0f;
                    float boostUsed = max(currentBoostUsed + static_cast<float>(rand() % 100 - 50), 0.0f);

                    file << "                    <attempt id=\"" << attemptIndex
                        << "\" scored=\"" << (scored ? "true" : "false")
                        << "\" goalSpeed=\"" << fixed << setprecision(4) << goalSpeed
                        << "\" boostUsed=\"" << fixed << setprecision(4) << boostUsed << "\" />\n";
                }
                file << "                </shot>\n";
            }

            file << "            </shotData>\n"
                << "        </session>\n";
        }
    }

    file << "    </sessions>\n</root>\n";

    file.close();
}
//Add user and test data to session cache
void AddSessionsToCache(string dataFolder) {
    sessionsCache.clear();
    isCacheLoaded = false;

    //user data -> sessionsCache
    try {
        vector<TrainingSessionData> userSessions = ReadTrainingSessionDataFromXML(dataFolder + "\\training_sessions.xml");
        sessionsCache.insert(sessionsCache.end(), userSessions.begin(), userSessions.end());
        LOG("User training data loaded successfully.");
    }
    catch (const exception& e) { LOG("Error loading user training data: " + string(e.what())); }

    //test data -> sessionsCache
    try {
        vector<TrainingSessionData> testSessions = ReadTrainingSessionDataFromXML(dataFolder + "\\test_data.xml");
        sessionsCache.insert(sessionsCache.end(), testSessions.begin(), testSessions.end());
        LOG("Test training data loaded successfully.");
    }
    catch (const exception& e) { LOG("Error loading test training data: " + string(e.what())); }

    isCacheLoaded = true;
    //PrintAllCachedSessions();

    ImGui::Separator();
}
//reads training data from passed in file path returning list of sessions
vector<TrainingSessionData> ReadTrainingSessionDataFromXML(const string& filePath) {
    vector<TrainingSessionData> sessions;

    ifstream file(filePath);
    if (!file.is_open()) {
        throw runtime_error("Unable to open file: " + filePath);
    }

    //read file as a string
    stringstream buffer;
    buffer << file.rdbuf();
    string xml = buffer.str();
    file.close();

    //find start and end sessions blocks of XML
    size_t sessionsStart = xml.find("<sessions>");
    size_t sessionsEnd = xml.find("</sessions>");
    if (sessionsStart == string::npos || sessionsEnd == string::npos) {
        throw runtime_error("Error: <sessions> block not found in XML.");
    }

    //note of start and end blocks
    string sessionsBlock = xml.substr(sessionsStart + 10, sessionsEnd - (sessionsStart + 10));

    
    size_t sessionPos = 0;
    while ((sessionPos = sessionsBlock.find("<session>", sessionPos)) != string::npos) {
        size_t sessionEnd = sessionsBlock.find("</session>", sessionPos);
        if (sessionEnd == string::npos) {
            LOG("Error: Malformed <session> block.");
            break;
        }

        //extract session data
        string sessionData = sessionsBlock.substr(sessionPos, sessionEnd - sessionPos + 9);

        //store TrainingSessionData as session
        TrainingSessionData session;

        //start and end times
        session.startTime = ParseXMLTag(sessionData, "startTime");
        session.endTime = ParseXMLTag(sessionData, "endTime");

        //pack data
        string trainingPackData = ParseXMLTag(sessionData, "trainingPack");
        session.trainingPackCode = ParseXMLTag(trainingPackData, "code");
        session.trainingPackName = ParseXMLTag(trainingPackData, "name");
        session.totalPackShots = stoi(ParseXMLTag(trainingPackData, "totalPackShots"));

        //shot data
        string shotDataBlock = ParseXMLTag(sessionData, "shotData");
        size_t shotPos = 0;
        while ((shotPos = shotDataBlock.find("<shot", shotPos)) != string::npos) {
            size_t shotEnd = shotDataBlock.find("</shot>", shotPos);
            if (shotEnd == string::npos) {
                LOG("Error: Malformed <shot> block.");
                break;
            }

            string shotData = shotDataBlock.substr(shotPos, shotEnd - shotPos + 7);
            vector<AttemptData> attempts;

            size_t attemptPos = 0;
            while ((attemptPos = shotData.find("<attempt", attemptPos)) != string::npos) {
                size_t attemptEnd = shotData.find("/>", attemptPos);
                if (attemptEnd == string::npos) {
                    LOG("Error: Malformed <attempt> block.");
                    break;
                }

                string attemptData = shotData.substr(attemptPos, attemptEnd - attemptPos + 2);
                AttemptData attempt;
                size_t scoredPos = attemptData.find("scored=\"true\"");
                attempt.scored = (scoredPos != string::npos);

                size_t goalSpeedPos = attemptData.find("goalSpeed=\"");
                if (goalSpeedPos != string::npos) {
                    goalSpeedPos += 11;
                    size_t goalSpeedEnd = attemptData.find("\"", goalSpeedPos);
                    attempt.goalSpeed = stof(attemptData.substr(goalSpeedPos, goalSpeedEnd - goalSpeedPos));
                }

                size_t boostUsedPos = attemptData.find("boostUsed=\"");
                if (boostUsedPos != string::npos) {
                    boostUsedPos += 11;
                    size_t boostUsedEnd = attemptData.find("\"", boostUsedPos);
                    attempt.boostUsed = stof(attemptData.substr(boostUsedPos, boostUsedEnd - boostUsedPos));
                }

                attempts.push_back(attempt);
                attemptPos = attemptEnd + 2;
            }

            session.shotData.push_back(attempts);
            shotPos = shotEnd + 7;
        }

        sessions.push_back(session);
        sessionPos = sessionEnd + 9;
    }

    return sessions;
}
//helper function for ReadTrainingSessionDataFromXML for parsing our xml tags easier
string ParseXMLTag(const string& xml, const string& tag) {
    //get start and end indexes
    size_t start = xml.find("<" + tag + ">"); //'<' from <tagname>tagname
    size_t end = xml.find("</" + tag + ">"); //'<' from </tagname>

    //if couldnt get start or end indexes
    if (start == string::npos || end == string::npos) {
        LOG("Error: Tag <" + tag + "> not found.");
        return "";
    }

    //from the start point, add the length of the tag and the two angle brackets <>
    start += tag.length() + 2;

    //in xml from start point, until incremented pointer x times
    return xml.substr(start, end - start);
}
//helper that prepares pack data for displayPackSelector
vector<pair<string, string>> getTrainingPackNameAndCode(const vector<TrainingSessionData>& sessionCache) {
    vector<pair<string, string>> trainingPacks;
    set<string> seenCodes;

    for (const auto& session : sessionCache) {
        //ensure code doesn't already exist
        if (seenCodes.find(session.trainingPackCode) == seenCodes.end()) {
            trainingPacks.emplace_back(session.trainingPackCode, session.trainingPackName);
            seenCodes.insert(session.trainingPackCode);
        }
    }

    return trainingPacks;
}
//display dropdown with name and code
string displayPackSelector(const vector<pair<string, string>>& trainingPacks) {
    //automatically try to get code and name
    static string selectedPackCode = trainingPacks.empty() ? "" : trainingPacks[0].first;
    static string selectedPackName = trainingPacks.empty() ? "" : trainingPacks[0].second;
    string selectedDisplay = selectedPackName + " (" + selectedPackCode + ")";

    //dropdown
    ImGui::Text("Select Training Pack:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(350.0f);
    if (ImGui::BeginCombo("##TrainingPackDropdown", selectedDisplay.c_str())) {
        for (const auto& [packCode, packName] : trainingPacks) {
            //combine here
            string displayName = packName + " (" + packCode + ")";
            bool isSelected = (selectedPackCode == packCode);

            if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                selectedPackCode = packCode;
                selectedPackName = packName;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus(); //set as selected item from dropdown
            }
        }
        ImGui::EndCombo();
    }

    return selectedPackCode;
}

//unused debugging functions==========================================================================================================
//displays implot demo
void displayDemo() {
    static bool show_demo_window = false;
    ImGui::Checkbox("Show ImPlot Demo", &show_demo_window);
    if (show_demo_window) {
        ImPlot::ShowDemoWindow(&show_demo_window);
    }
    ImGui::Separator();
}
//debugging function
void PrintAllCachedSessions() {
    //loop through every session
    for (const auto& session : sessionsCache) {
        //print top-level members
        LOG("Start Time: " + session.startTime);
        LOG("End Time: " + session.endTime);
        LOG("Training Pack Code: " + session.trainingPackCode);
        LOG("Training Pack Name: " + session.trainingPackName);
        LOG("Total Pack Shots: " + to_string(session.totalPackShots));

        //print all shotData from attempt vector
        for (size_t i = 0; i < session.shotData.size(); i++) {
            //print shotData's shot #
            LOG("Shot " + to_string(i) + ":");

            //print shotData for shot #
            for (const auto& attempt : session.shotData[i]) {
                //shotData includes Attempt (t/f), goal speed, boost used
                LOG("  Attempt - Scored: " + string(attempt.scored ? "true" : "false") +
                    ", Goal Speed: " + to_string(attempt.goalSpeed) +
                    ", Boost Used: " + to_string(attempt.boostUsed));
            }
        }
    }
}