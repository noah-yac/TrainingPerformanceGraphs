#pragma once
#include "pch.h"
#include "TrainingPerformanceGraphs.h"
#include "TrainingPerformanceGraphsGUI.h"
#include "GraphFunctions.h"

//for individual shot controls, appropriately update variables such that any graph may access the control's state
void HandleIndividualShotControls(int totalShots, const string& selectedPack, const vector<TrainingSessionData>& filteredSessions) {
    //track states
    static bool displayAllShots = false;
    static string lastSelectedPack = "";

    //reset states if the selected pack changes
    if (lastSelectedPack != selectedPack) {
        //LOG("Switching training pack. Resetting shot controls.");
        customShotLines.clear();
        displayAllShots = false;
        lastSelectedPack = selectedPack;
    }

    //spacing
    ImGui::Separator();
    ImGui::Dummy(ImVec2(1, 0));
    ImGui::SameLine();

    //toggle "Display all shots"
    if (ImGui::Button(displayAllShots ? "Remove all shots" : "Add all shots", ImVec2(110.0f, 0.0f))) {
        displayAllShots = !displayAllShots;
        errorMessage.clear();

        if (displayAllShots) {
            //LOG("Displaying all shots.");
            //add all shots to customShotLines
            for (int shotNumber = 1; shotNumber <= totalShots; ++shotNumber) {
                string label = "Shot " + to_string(shotNumber);
                auto it = find_if(customShotLines.begin(), customShotLines.end(),
                    [&label](const auto& line) { return line.first == label; });

                if (it == customShotLines.end()) {
                    customShotLines.emplace_back(label, vector<pair<float, float>>());
                }
            }
        }
        else {
            //LOG("Hiding all shots.");
            //remove all shots from customShotLines
            customShotLines.clear();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add/Remove lines to graph for each available shot number.");
    }

    //spacing and | vert seperation between toggle all and ind shot buttons
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(1, 0));
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(1, 0));
    ImGui::SameLine();

    //individual shot
    if (ImGui::Button("Add shot")) {
        int shotNumber = -1;
        bool validInput = true;
        bool hasData = false;

        try {
            shotNumber = stoi(shotNumberInput);
        }
        catch (...) {
            validInput = false;
        }

        //check for data
        if (validInput && shotNumber > 0 && shotNumber <= totalShots) {
            for (const auto& session : filteredSessions) {
                if (shotNumber - 1 < static_cast<int>(session.shotData.size()) && !session.shotData[shotNumber - 1].empty()) {
                    hasData = true;
                    break;
                }
            }
        }

        //validate shot number
        if (!validInput || shotNumber <= 0 || shotNumber > totalShots) {
            errorMessage = "Invalid value entered. (Valid values: 1-" + to_string(totalShots) + ")";
        }
        else if (!hasData) {
            errorMessage = "No data available for shot " + to_string(shotNumber) + ".";
        }
        else {
            errorMessage.clear();
            //LOG("Added individual shot: #" + to_string(shotNumber));

            //update customShotLines with the selected shot
            string label = "Shot " + to_string(shotNumber);
            auto it = find_if(customShotLines.begin(), customShotLines.end(),
                [&label](const auto& line) { return line.first == label; });

            if (it == customShotLines.end()) {
                customShotLines.emplace_back(label, vector<pair<float, float>>());
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add line to graph for a specific shot number.");
    }
    ImGui::SameLine();
    //input field and buttons
    ImGui::SetNextItemWidth(30.0f); //width for input box
    ImGui::InputText("##ShotNumber", shotNumberInput, IM_ARRAYSIZE(shotNumberInput));
    ImGui::SameLine();
    ImGui::Text("/ %d", totalShots);
   
    //display error message, if any
    if (!errorMessage.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", errorMessage.c_str());
    }
    
    //ImGui::SameLine();
    ImGui::Separator();
}

//Graph 1: Scoring Rate
void RenderScoringRateGraph(const vector<TrainingSessionData>& sessions, const string& selectedPack) {
    static string lastSelectedPack;

    //if the selected pack changes, reset data 
    if (lastSelectedPack != selectedPack) {
        customShotLines.clear();
        lastSelectedPack = selectedPack;
    }

    //filter sessions for the selected training pack
    vector<TrainingSessionData> filteredSessions;
    for (const auto& session : sessions) {
        if (session.trainingPackCode == selectedPack) {
            filteredSessions.push_back(session);
        }
    }

    //get number of sessions
    int numSessions = static_cast<int>(filteredSessions.size());

    //handle if no sessions
    if (numSessions == 0) return;

    //calculate scoring rates
    vector<float> scoringRates(numSessions);
    vector<float> sessionIndices(numSessions);
    for (int i = 0; i < numSessions; ++i) {
        float scored = 0.0f;
        float attempted = 0.0f;

        for (const auto& attempts : filteredSessions[i].shotData) {
            for (const auto& attempt : attempts) {
                attempted += 1;
                if (attempt.scored) {
                    scored += 1;
                }
            }
        }

        scoringRates[i] = (attempted > 0) ? (scored / attempted) * 100.0f : 0.0f;
        sessionIndices[i] = static_cast<float>(i + 1);
    }

    //get total shots from the first session (assuming all sessions have the same number of shots)
    int totalShots = !filteredSessions.empty() ? static_cast<int>(filteredSessions[0].shotData.size()) : 0;

    //call the shot controls function
    HandleIndividualShotControls(totalShots, selectedPack, filteredSessions);

    //update `customShotLines` with data for the selected shots
    for (auto& [label, points] : customShotLines) {
        int shotNumber = stoi(label.substr(5)) - 1; //extract the shot number from the label
        points.clear(); //clear existing points

        for (int sessionIndex = 0; sessionIndex < numSessions; ++sessionIndex) {
            float scored = 0.0f;
            float attempted = 0.0f;

            if (shotNumber < static_cast<int>(filteredSessions[sessionIndex].shotData.size())) {
                for (const auto& attempt : filteredSessions[sessionIndex].shotData[shotNumber]) {
                    attempted += 1;
                    if (attempt.scored) {
                        scored += 1;
                    }
                }
            }

            //add point if there were attempts
            if (attempted > 0) {
                float shotScoringRate = (scored / attempted) * 100.0f;
                points.emplace_back(static_cast<float>(sessionIndex + 1), shotScoringRate);
            }
        }
    }

    //determine y-axis range
    float yMin = -1.0f;
    float yMax = 110.0f;
    float xMin = 0.001f; //start x-axis slightly before the first session
    float xMax = max(11.0f, static_cast<float>(numSessions + 1));

    //set plot limits
    ImPlot::SetNextPlotLimits(xMin, xMax, yMin, yMax, ImGuiCond_Always);

    if (ImPlot::BeginPlot("Scoring Rate: Average Scoring Rate From Selected Training Pack", "Training Session", "Scoring Rate (%)", ImVec2(-1, 0))) {
        if (numSessions == 1) {
            //draw a single white circle for one data point
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); //white color
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Circle); //marker
        }
        else {
            //bold white line for plot session-wide scoring rates
            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); //white color
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.0f); //extra bold line
        }
        ImPlot::PlotLine("Average", sessionIndices.data(), scoringRates.data(), numSessions);
        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();

        //plot custom shot scoring rates
        vector<int> markerStyles = { ImPlotMarker_Circle, ImPlotMarker_Square, ImPlotMarker_Diamond,
                                    ImPlotMarker_Up, ImPlotMarker_Down, ImPlotMarker_Left,
                                    ImPlotMarker_Right, ImPlotMarker_Cross, ImPlotMarker_Plus,
                                    ImPlotMarker_Asterisk };

        int markerIndex = 0; //marker index for overlapping points
        for (const auto& [label, points] : customShotLines) {
            vector<float> xs, ys;
            for (const auto& [x, y] : points) {
                xs.push_back(x);
                ys.push_back(y);
            }

            //alternate marker style for overlapping points
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0, 0, 0, 0));
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, markerStyles[markerIndex % markerStyles.size()]);
            ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
            ImPlot::PopStyleVar();
            ImPlot::PopStyleColor();

            markerIndex++;
        }

        ImPlot::EndPlot();
    }

    ImGui::Separator();
}
//Graph 2: Boost Used
void RenderBoostUsedGraph(const vector<TrainingSessionData>& sessions, const string& selectedPack) {
    //static variable to track the last selected pack
    static string lastSelectedPack;

    //reset data if the selected pack changes
    if (lastSelectedPack != selectedPack) {
        customShotLines.clear();
        lastSelectedPack = selectedPack;
    }

    //filter sessions for the selected training pack
    vector<TrainingSessionData> filteredSessions;
    for (const auto& session : sessions) {
        if (session.trainingPackCode == selectedPack) {
            filteredSessions.push_back(session);
        }
    }

    //get number of sessions
    int numSessions = static_cast<int>(filteredSessions.size());

    //handle if no sessions
    if (numSessions == 0) return;

    //calculate boost used averages
    vector<float> boostAverages(numSessions);
    vector<float> sessionIndices(numSessions);
    for (int i = 0; i < numSessions; ++i) {
        float totalBoostUsed = 0.0f;
        float totalAttempts = 0.0f;

        for (const auto& attempts : filteredSessions[i].shotData) {
            for (const auto& attempt : attempts) {
                totalBoostUsed += attempt.boostUsed;
                totalAttempts += 1;
            }
        }

        boostAverages[i] = (totalAttempts > 0) ? totalBoostUsed / totalAttempts : 0.0f;
        sessionIndices[i] = static_cast<float>(i + 1);
    }

    //update `customShotLines` with data for the selected shots
    for (auto& [label, points] : customShotLines) {
        int shotNumber = stoi(label.substr(5)) - 1; //extract the shot number from the label
        points.clear(); //clear existing points

        for (int sessionIndex = 0; sessionIndex < numSessions; ++sessionIndex) {
            float totalBoostUsed = 0.0f;
            float totalAttempts = 0.0f;

            if (shotNumber < static_cast<int>(filteredSessions[sessionIndex].shotData.size())) {
                for (const auto& attempt : filteredSessions[sessionIndex].shotData[shotNumber]) {
                    totalBoostUsed += attempt.boostUsed;
                    totalAttempts += 1;
                }
            }

            //add point if there were attempts
            if (totalAttempts > 0) {
                float shotBoostAverage = totalBoostUsed / totalAttempts;
                points.emplace_back(static_cast<float>(sessionIndex + 1), shotBoostAverage);
            }
        }
    }

    //determine y-axis range
    float yMin = -3.0f;
    float yMax = 310.0f; //maximum range for boost
    float xMin = 0.001f; //start x-axis slightly before the first session
    float xMax = max(11.0f, static_cast<float>(numSessions + 1));

    //set plot limits
    ImPlot::SetNextPlotLimits(xMin, xMax, yMin, yMax, ImGuiCond_Always);

    if (ImPlot::BeginPlot("Boost Used: Average Boost Used From Selected Training Pack", "Training Session", "Boost Used", ImVec2(-1, 0))) {
        if (numSessions == 1) {
            //draw a single white circle for one data point
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); //white color
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Circle); //marker
        }
        else {
            //bold white line for plot session-wide boost usage rates
            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); //white color
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.0f); //extra bold line
        }
        ImPlot::PlotLine("Average", sessionIndices.data(), boostAverages.data(), numSessions);
        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();

        //plot custom shot boost averages
        vector<int> markerStyles = { ImPlotMarker_Circle, ImPlotMarker_Square, ImPlotMarker_Diamond,
                                    ImPlotMarker_Up, ImPlotMarker_Down, ImPlotMarker_Left,
                                    ImPlotMarker_Right, ImPlotMarker_Cross, ImPlotMarker_Plus,
                                    ImPlotMarker_Asterisk };

        int markerIndex = 0; //marker index for overlapping points
        for (const auto& [label, points] : customShotLines) {
            vector<float> xs, ys;
            for (const auto& [x, y] : points) {
                xs.push_back(x);
                ys.push_back(y);
            }

            //alternate marker style for overlapping points
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0, 0, 0, 0));
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, markerStyles[markerIndex % markerStyles.size()]);
            ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
            ImPlot::PopStyleVar();
            ImPlot::PopStyleColor();

            markerIndex++;
        }

        ImPlot::EndPlot();
    }

    ImGui::Separator();
}
//Graph 3: Goal Speed
void RenderGoalSpeedGraph(const vector<TrainingSessionData>& sessions, const string& selectedPack) {
    //static variable to track the last selected pack
    static string lastSelectedPack;

    //reset data if the selected pack changes
    if (lastSelectedPack != selectedPack) {
        customShotLines.clear();
        lastSelectedPack = selectedPack;
    }

    //filter sessions for the selected training pack
    vector<TrainingSessionData> filteredSessions;
    for (const auto& session : sessions) {
        if (session.trainingPackCode == selectedPack) {
            filteredSessions.push_back(session);
        }
    }

    //get number of sessions
    int numSessions = static_cast<int>(filteredSessions.size());

    //handle if no sessions
    if (numSessions == 0) return;

    //calculate goal speed averages
    vector<float> goalSpeedAverages(numSessions);
    vector<float> sessionIndices(numSessions);
    for (int i = 0; i < numSessions; ++i) {
        float totalGoalSpeed = 0.0f;
        float totalGoals = 0.0f;

        for (const auto& attempts : filteredSessions[i].shotData) {
            for (const auto& attempt : attempts) {
                if (attempt.scored) { //only consider scored attempts for goal speed
                    totalGoalSpeed += attempt.goalSpeed;
                    totalGoals += 1;
                }
            }
        }

        goalSpeedAverages[i] = (totalGoals > 0) ? totalGoalSpeed / totalGoals : 0.0f;
        sessionIndices[i] = static_cast<float>(i + 1);
    }

    //update `customShotLines` with data for the selected shots
    for (auto& [label, points] : customShotLines) {
        int shotNumber = stoi(label.substr(5)) - 1; //extract the shot number from the label
        points.clear(); //clear existing points

        for (int sessionIndex = 0; sessionIndex < numSessions; ++sessionIndex) {
            float totalGoalSpeed = 0.0f;
            float totalGoals = 0.0f;

            if (shotNumber < static_cast<int>(filteredSessions[sessionIndex].shotData.size())) {
                for (const auto& attempt : filteredSessions[sessionIndex].shotData[shotNumber]) {
                    if (attempt.scored) { //only consider scored attempts for goal speed
                        totalGoalSpeed += attempt.goalSpeed;
                        totalGoals += 1;
                    }
                }
            }

            //add point if there were scored attempts
            if (totalGoals > 0) {
                float shotGoalSpeedAverage = totalGoalSpeed / totalGoals;
                points.emplace_back(static_cast<float>(sessionIndex + 1), shotGoalSpeedAverage);
            }
        }
    }

    //determine y-axis range
    float yMin = -1.4f;
    float yMax = 150.0f; //adjusted maximum range for goal speed
    float xMin = 0.001f; //start x-axis slightly before the first session
    float xMax = max(11.0f, static_cast<float>(numSessions + 1));

    //set plot limits
    ImPlot::SetNextPlotLimits(xMin, xMax, yMin, yMax, ImGuiCond_Always);

    if (ImPlot::BeginPlot("Goal Speed: Average Goal Speed From Selected Training Pack", "Training Session", "Goal Speed (km/h)", ImVec2(-1, 0))) {
        if (numSessions == 1) {
            //draw a single white circle for one data point
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); //white color
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Circle); //marker
        }
        else {
            //bold white line for plot session-wide goal speed averages
            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); //white color
            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.0f); //extra bold line
        }
        ImPlot::PlotLine("Average", sessionIndices.data(), goalSpeedAverages.data(), numSessions);
        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();

        //plot custom shot goal speed averages
        vector<int> markerStyles = { ImPlotMarker_Circle, ImPlotMarker_Square, ImPlotMarker_Diamond,
                                    ImPlotMarker_Up, ImPlotMarker_Down, ImPlotMarker_Left,
                                    ImPlotMarker_Right, ImPlotMarker_Cross, ImPlotMarker_Plus,
                                    ImPlotMarker_Asterisk };

        int markerIndex = 0; //marker index for overlapping points
        for (const auto& [label, points] : customShotLines) {
            vector<float> xs, ys;
            for (const auto& [x, y] : points) {
                xs.push_back(x);
                ys.push_back(y);
            }

            //alternate marker style for overlapping points
            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(0, 0, 0, 0));
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, markerStyles[markerIndex % markerStyles.size()]);
            ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()));
            ImPlot::PopStyleVar();
            ImPlot::PopStyleColor();

            markerIndex++;
        }

        ImPlot::EndPlot();
    }

    ImGui::Separator();
}
