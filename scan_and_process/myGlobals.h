/* Global variable declarations */

#pragma once

#include "myTypes.h"
#include "A3200.h"
#include <opencv2/core.hpp>
#include <vector> 
#include "threadsafeQueue.h"


extern A3200Handle handle;
extern A3200DataCollectConfigHandle DCCHandle;

extern std::vector<Segment> segments;
extern threadsafe_queue <cv::Mat> q_scannedEdges;
extern bool doneScanning;
extern bool positionFlag; 

extern int segmentNum;