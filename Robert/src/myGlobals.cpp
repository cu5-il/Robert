#include "myGlobals.h" /* Global variable declarations made available here */

A3200Handle handle = NULL;
A3200DataCollectConfigHandle DCCHandle = NULL;
Extruder extruder;

std::vector<Segment> segments;
threadsafe_queue<bool> q_scanMsg;
threadsafe_queue<edgeMsg> q_edgeMsg;
threadsafe_queue<errsMsg> q_errsMsg;
threadsafe_queue<pathMsg> q_pathMsg;

extern std::string outDir = "./Output/";

std::mutex mut_cmd;