#pragma once
#include "pch.h"
#include "TrainingPerformanceGraphs.h"

void GenerateTestXML(const string& filePath);
void AddSessionsToCache(string dataFolder);
vector<TrainingSessionData> ReadTrainingSessionDataFromXML(const string& filePath);
string ParseXMLTag(const string& xml, const string& tag);
vector<pair<string, string>> getTrainingPackNameAndCode(const vector<TrainingSessionData>& sessionCache);
string displayPackSelector(const vector<pair<string, string>>& trainingPacks);

void displayDemo();
void PrintAllCachedSessions();