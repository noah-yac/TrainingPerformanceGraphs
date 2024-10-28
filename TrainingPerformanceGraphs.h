#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"
#include <fstream>
#include <ctime>
#include <chrono>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

using namespace std;

struct TrainingPackInfo {
	string code;
	string name;
	int totalPackShots = 0;
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
	int shotAttempts = 0;
	int shotsMade = 0;
	int roundNumber = -1; //active round INDEX
	void HandleAttempt(bool attempting);

	//track and handle boost usage
	bool isBoosting = false;
	float totalBoostUsed = 0.0f;
	chrono::high_resolution_clock::time_point boostStartTime;
	void HandleBoost(bool start);

	//session data
	void SaveSessionDataToCSV();
};
