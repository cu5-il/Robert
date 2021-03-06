#include <iostream>
#include <fstream>
#include <iomanip>      // std::setw
#include <vector> 
#include <string>
#include <numeric> // std::accumulate

#include <deque>
#include <filesystem> // std::filesystem::copy, std::filesystem::create_directories
namespace fs = std::filesystem;

#include <opencv2/core.hpp>
#include "opencv2/core/utility.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include "opencv2/core/utils/logger.hpp"

#include "A3200.h"

#include "constants.h"
#include "myTypes.h"
#include "myGlobals.h"
#include "scanning.h"
#include "draw.h"

#include "A3200_functions.h"
#include "thread_functions.h"
#include "raster.h"
#include <ctime>
#include "path.h"
#include "controlCalib.h"
#include "input.h"
#include "multiLayer.h"

std::string datetime(std::string format = "%Y.%m.%d-%H.%M.%S");

int main() {
	// Disable openCV warning in console
	cv::utils::logging::setLogLevel(cv::utils::logging::LogLevel::LOG_LEVEL_SILENT);
	// Create and set the output path
	outDir.append(datetime("%Y.%m.%d") + "/");
	try { fs::create_directories(outDir); }
	catch (std::exception& e) { std::cout << e.what(); }

	//=======================================
	// Connecting to and setting up the A3200
	std::cout << "Connecting to A3200. Initializing if necessary." << std::endl;
	if (!A3200Connect(&handle)) { A3200Error(); }
	// Creating a data collection handle and setting up the data collection
	if (!A3200DataCollectionConfigCreate(handle, &DCCHandle)) { A3200Error(); }
	if (!setupDataCollection(handle, DCCHandle)) { A3200Error(); }
	// Initializing the extruder
	extruder = Extruder(handle, TASK_PRINT);
	//=======================================

	std::thread t_scan, t_process, t_control, t_print;

	// Initialize parameters
	Raster raster;
	double rasterBorder = 2;
	std::vector<std::vector<Path>> path, ctrlPath;

	// defining the material models
	MaterialModel augerModel = MaterialModel('a',
		std::vector<double>{2, 3},
		std::vector<double>{1, 1},
		std::vector<double>{1, 1},
		std::vector<double>{0, 0});

	// setting up the controller
	AugerController controller(augerModel, 0.1, 1.5);

	// setting the print options
	double leadin = 10;
	PrintOptions printOpts(leadin);
	printOpts.extrude = true;
	printOpts.disposal = false;
	printOpts.asyncTheta = 32;

	// Getting user input
	std::string resp, infile;
	char option;
	int lineNum;
	infile = "./Input/printTable.md";

	std::cout << "Select option: (p)rint or (s)can? ";
	std::cin >> option;
	std::cout << "Test #: ";
	std::cin >> lineNum;

	TableInput input(infile, lineNum);
	// copy the table to the output folder
	try { fs::copy(infile, outDir, fs::copy_options::overwrite_existing); }
	catch (std::exception& e) { std::cout << e.what(); }

	// Append the output directory with the print number and create the directory
	outDir.append("/print" + std::to_string(lineNum) + "/");
	try { fs::create_directories(outDir); }
	catch (std::exception& e) { std::cout << e.what(); }

	// make the scaffold
	raster = Raster(input.length, input.width, input.rodSpc, input.rodSpc - .1, rasterBorder);
	raster.offset(cv::Point2d(input.initPos.x, input.initPos.y));
	//MultiLayerScaffold scaffold(input, raster);
	FunGenScaf scaffold(input, raster, augerModel);
	// add a lead out line
	printOpts.leadout = -SCAN_OFFSET_X + 1;
	scaffold.leadout(-SCAN_OFFSET_X);

	path = scaffold.path;
	segments = scaffold.segments;

#ifdef DEBUG_SCANNING
	q_scanMsg.push(true);
	t_CollectScans(raster);
	return 0;
#endif // DEBUG_SCANNING

	cv::Mat imseg = raster.draw(input.startLayer);
	drawSegments(raster.draw(input.startLayer), imseg, segments, raster.origin(), input.startLayer, 3);
	cv::Mat image = cv::Mat::zeros(raster.size(segments.back().layer()), CV_8UC3);
	drawMaterial(image, image, scaffold.segments, scaffold.path, scaffold.segments.back().layer());

	switch (option)
	{
	default:
		return 0;
	case 'p': // PRINTING
		outDir.append("print_");
		printOpts.asyncTheta = 0;
		ctrlPath = path;

		//t_scan = std::thread{ t_CollectScans, raster };
		//t_process = std::thread{ t_GetMatlErrors, raster, path };
		t_print = std::thread{ t_printQueue, path[0][0], printOpts };
		t_control = std::thread{ t_noController, ctrlPath };

		//t_scan.join();
		//t_process.join();
		t_control.join();

		break;

	case 's': // SCANNING
		Raster rasterScan;
		segments.clear();
		path.clear();
		printOpts.extrude = false;
		//std::cout << "(r)otating or (f)ixed scan: ";
		//std::cin >> option;
		option = 'f';
		switch (option)
		{
		default:
			return 0;
		case 'r':
			outDir.append("scanR_");
			rasterScan = raster;
			if (input.startLayer % 2 == 0) { rasterScan.offset(cv::Point2d(0, 1.5)); } // offset path in y direction
			else { rasterScan.offset(cv::Point2d(1.5, 0)); } // offset path in x direction
			input.initPos += cv::Point3d(0, 0, 1); // raise path
			scaffold = FunGenScaf(input, rasterScan, augerModel);
			scaffold.leadout(-SCAN_OFFSET_X);
			segments = scaffold.segments;
			path = scaffold.path;
			ctrlPath = scaffold.path;

			t_scan = std::thread{ t_CollectScans, raster };
			t_process = std::thread{ t_GetMatlErrors, raster, path };
			t_print = std::thread{ t_printQueue, path[0][0], printOpts };
			t_control = std::thread{ t_controller, std::ref(ctrlPath), std::ref(controller) };

			t_scan.join();
			t_process.join();
			t_control.join();
			break;
		case 'f':
			outDir.append("scanF_");
			printOpts.leadin = 0;
			printOpts.leadout = 0;
			printOpts.asyncTheta = 0;

			segments = scaffold.segmentsScan;
			path = scaffold.pathScan;
			ctrlPath = scaffold.pathScan;
			t_scan = std::thread{ t_CollectScans, raster };
			t_print = std::thread{ t_printQueue, path[0][0], printOpts };
			t_control = std::thread{ t_noController, ctrlPath };

			t_scan.join();
			t_control.join();

			// Calculate the errors using the actual raster path used in the print
			segments.clear();
			path.clear();
			path = scaffold.path;
			ctrlPath = scaffold.path;
			segments = scaffold.segments;

			t_GetMatlErrors(raster, path);
			break;
		}

		break;
	}

	// Opening a file to save the results
	std::ofstream outfile;
	outfile.open(std::string(outDir + "pathData.txt").c_str());
	outfile.precision(3);
	// loop through each long segment
	for (int i = 0; i < path.size(); i += 2) {
		// loop through all the waypoints
		for (int j = 0; j < path[i].size(); j++) {
			outfile << std::setw(3) << std::fixed << i << "\t";
			outfile << std::setw(7) << std::fixed << ctrlPath[i][j].x << "\t";
			outfile << std::setw(7) << std::fixed << ctrlPath[i][j].y << "\t";
			outfile << std::setw(6) << std::fixed << ctrlPath[i][j].f << "\t";
			outfile << std::setw(6) << std::fixed << ctrlPath[i][j].e << "\t";
			outfile << std::setw(6) << std::fixed << ctrlPath[i][j].w << "\t";
			if (!segments[i].errWD().empty())
				outfile << std::setw(9) << std::fixed << segments[i].errWD()[j] << "\t";
			if (!segments[i].errCL().empty())
				outfile << std::setw(9) << std::fixed << segments[i].errCL()[j];
			outfile << "\n";
		}
	}
	outfile.close();

	if (option == 'f' || option == 'r')
	{
		// calculate the error norms
		double E2d;
		auto sumsq = [](double a, double b) {
			if (!std::isnan(b)) return a + b * b;
			else return a; };
		outfile.open(std::string(outDir + "errors.txt").c_str());
		outfile.precision(3);
		// loop through each long segment
		for (int i = 0; i < path.size(); i += 2) {
			E2d = sqrt(std::accumulate(segments[i].errWD().begin(), segments[i].errWD().end(), 0.0, sumsq));
			E2d /= input.wayptSpc * (std::count_if(segments[i].errWD().begin(), segments[i].errWD().end(), [](double a) {return !std::isnan(a); }) - 1);
			outfile << std::setw(6) << std::fixed << E2d;
			if (i % 4 == 0) outfile << "\t";
			else outfile << "\n";
		}
		outfile.close();

		// calculate the average width of each segment
		double mean, stdev, meanErr;
		auto sum = [](double a, double b) {
			if (!std::isnan(b)) return a + b;
			else return a; };
		auto variance = [&meanErr](double a, double b) {
			if (!std::isnan(b)) return a + (b - meanErr) * (b - meanErr);
			else return a; };
		outfile.open(std::string(outDir + "widths.txt").c_str());
		outfile.precision(3);
		// loop through each long segment
		for (int i = 0; i < path.size(); i += 2) {
			// calculate the mean
			meanErr = std::accumulate(segments[i].errWD().begin(), segments[i].errWD().end(), 0.0, sum);
			meanErr /= (std::count_if(segments[i].errWD().begin(), segments[i].errWD().end(), [](double a) {return !std::isnan(a); }) - 1);
			mean = path[i][0].w - meanErr;
			// calculate the standard deviation
			stdev = std::accumulate(segments[i].errWD().begin(), segments[i].errWD().end(), 0.0, variance);
			stdev /= (std::count_if(segments[i].errWD().begin(), segments[i].errWD().end(), [](double a) {return !std::isnan(a); }) - 1);
			stdev = sqrt(stdev);
			outfile << std::setw(6) << std::fixed << path[i][0].f << "\t" << path[i][0].e << "\t" << mean << "\t" << stdev << "\n";
		}
		outfile.close();
	}

	t_print.join();


cleanup:
	//A3200 Cleanup
	//=======================================
	// Freeing the resources used by the data collection configuration
	if (NULL != DCCHandle) {
		if (!A3200DataCollectionConfigFree(DCCHandle)) { A3200Error(); }
	}
	// Disconnecting from the A3200
	if (NULL != handle) {
		printf("Disconnecting from the A3200.\n");
		if (!A3200Disconnect(handle)) { A3200Error(); }
	}

#ifdef _DEBUG

#endif
	return 0;
}

cv::Mat translateImg(cv::Mat& img, int offsetx, int offsety) {
	cv::Mat trans_mat = (cv::Mat_<double>(2, 3) << 1, 0, offsetx, 0, 1, offsety);
	cv::warpAffine(img, img, trans_mat, img.size());
	return img;
}

std::string datetime(std::string format) {
	time_t rawtime = time(NULL);
	struct tm timeinfo;
	char buffer[80];

	localtime_s(&timeinfo, &rawtime);
	strftime(buffer, 80, format.c_str(), &timeinfo);
	return std::string(buffer);
}