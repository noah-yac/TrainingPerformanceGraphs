#include "pch.h"
#include "TrainingPerformanceGraphs.h"
#include "IMGUI/implot.h"

using namespace std;

// Plugin Settings Window code here
string TrainingPerformanceGraphs::GetPluginName() {
	return "TrainingPerformanceGraphs";
}

// This will show up f2 -> plugins -> TrainingPerformanceGraphs
void TrainingPerformanceGraphs::RenderSettings() {
	ImGui::TextUnformatted("Hello world!");
	//further GUI implemented here
}

