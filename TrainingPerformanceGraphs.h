#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

using namespace std;


class TrainingPerformanceGraphs: public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
	//Boilerplate
	void onLoad() override;

public:
	//get name for f2 plugin list 
	string GetPluginName() override;

	//renders settings from f2 plugin list
	void RenderSettings() override;

private:
	//Plugin enabled
	shared_ptr<bool> enabled = make_shared<bool>(true);
	//Plugin loaded
	bool loaded = false;

	//
	
	//Returns Training Pack Code string
	string GetTrainingPackCode(TrainingEditorWrapper tw);
	string tpCode;

	//, 
	
	//Returns DateTime
	string GetCurrentDateTime();
	string sessionStartTime;

	//,

	string sessionEndTime;

	//, 
	
	int shotAttempts = 0;

	//,
	
	int totalPackShots = -1;

	//session data
	void SaveSessionDataToCSV();
};
