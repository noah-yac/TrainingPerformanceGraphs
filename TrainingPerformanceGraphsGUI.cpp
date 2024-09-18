#include "pch.h"
#include "TrainingPerformanceGraphs.h"

using namespace std;

// Plugin Settings Window code here
string TrainingPerformanceGraphs::GetPluginName() {
	return "TrainingPerformanceGraphs";
}

// This will show up f2 -> plugins -> TrainingPerformanceGraphs
void TrainingPerformanceGraphs::RenderSettings() {
    ImGui::TextUnformatted("Training Performance Graphs");

    //checkbox that will enable demo
    static bool show_demo_window = true;
    ImGui::Checkbox("Show ImPlot Demo", &show_demo_window);

    //when true display demo
    if (show_demo_window) {
        ImPlot::ShowDemoWindow(&show_demo_window);
    }

    ImGui::Separator();
    ImGui::Text("Other stuff to be implemented");
}