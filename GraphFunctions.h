#pragma once
#include "pch.h"
#include "TrainingPerformanceGraphs.h"
#include "TrainingPerformanceGraphsGUI.h"

//static variables for adding points onto graph
static char shotNumberInput[4] = "";
static string errorMessage;
static vector<pair<string, vector<pair<float, float>>>> customShotLines;

//for individual shot controls, appropriately update variables such that any graph may access the control's state
void HandleIndividualShotControls(int totalShots, const string& selectedPack, const vector<TrainingSessionData>& filteredSessions);

//graphing functions
void RenderScoringRateGraph(const vector<TrainingSessionData>& sessions, const string& selectedPack);
void RenderBoostUsedGraph(const vector<TrainingSessionData>& sessions, const string& selectedPack);
void RenderGoalSpeedGraph(const vector<TrainingSessionData>& sessions, const string& selectedPack);
