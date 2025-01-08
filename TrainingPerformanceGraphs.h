#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"
#include <fstream>
#include <ctime>
#include <chrono>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <cmath>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

using namespace std;

//used once to hold all tp info
struct TrainingPackInfo {
	string code;
	string name;
	int totalPackShots = 0;
};

//each attempt stores these values
struct AttemptData {
	bool scored = false;
	float goalSpeed = 0.0f;
	float boostUsed = 0.0f;
};

//each shot in a pack will have one vector of attempts
struct ShotData {
	vector<AttemptData> attempts;
};

//struct will hold ONE ENTIRE 'session's data
struct TrainingSessionData {
    string trainingPackCode;
    string trainingPackName;
    string startTime;
    string endTime;
    int totalPackShots = -1;
    vector<vector<AttemptData>> shotData;

    //cached data for scoring rate (not taking or adding this value to xml)
    float scoringRateAverage = 0.0f;
};

class TrainingPerformanceGraphs: public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
	void onLoad() override; 

public:
	//f2 plugin list
	string GetPluginName() override;
	void RenderSettings() override;

private:
	//plugin
	shared_ptr<bool> enabled = make_shared<bool>(true);
	bool loaded = false;

	//start end time
	string sessionStartTime;
	string sessionEndTime;
	string GetCurrentDateTime();

	//struct defined above
	TrainingPackInfo info;
	TrainingPackInfo GetTrainingPackInfo(TrainingEditorWrapper tw);

	//track and handle a shot 'attempt'
	bool isAttempting = false;
	int roundIndex = -1; //active round INDEX
	void HandleAttempt(bool attempting);

	//track and handle boost usage
	bool isBoosting = false;
	float totalBoostUsed = 0.0f;
	chrono::high_resolution_clock::time_point boostStartTime;
	void HandleBoost(bool start);

	//track goal speed
	float speed = 0.0f;
	void GetSpeed(bool attempting);

	//session data
	void SaveSessionDataToXML();

	//vector 'shotData' for each shot in a pack
	vector<ShotData> shotData;
};
