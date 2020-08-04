#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <string>

#define _USE_MATH_DEFINES
#include <math.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "XPLMPlugin.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMMenus.h"
#include "XPLMUtilities.h"
#include "XPWidgets.h"
#include "XPStandardWidgets.h"
#include "XPLMScenery.h"
#include "XPLMPlanes.h"

// OS X: we use this to convert our file path.
#if APL
#include <Carbon/Carbon.h>
#endif

/* ToDo

Get Aircraft Type from xfse_alias.txt or look how FSE gets the aircraft anme

Get the Fuel Consumption and Average Speed
Calculate current fuel consumption
Add Method to toogle fuel adaption (fuel drain until at FSE gph)


*/


/**************************************************************************************************************
 * Definitions
 **************************************************************************************************************/

const std::string default_distance = "No Data";

struct HRM_Airport
{
	std::string icao = "";
	std::string name = "";
	double latitude = 0;
	double longitude = 0;

};

struct FSE_Aircraft
{
	std::string name = "";
	double cruise = 0;
	double gph = 0;
};

const double fse_fuel_weight = 2.72;

double calc_distance_nm(double lat1, double long1, double lat2, double long2);
void ReadFSEAirports();
HRM_Airport* GetClosestFSEAirport(double lat, double lon);



/**************************************************************************************************************
 * Global Variables
 **************************************************************************************************************/

XPLMWindowID WindowId = 0;

XPLMDataRef enable_random_failures = NULL;
XPLMDataRef set_mtbf = NULL;

XPLMDataRef m_d_latitude = NULL;;
XPLMDataRef m_d_longitude = NULL;;
XPLMDataRef m_f_heading = NULL;;

XPLMDataRef m_fse_dr_is_connected = NULL;
XPLMDataRef m_fse_dr_is_flying = NULL;
XPLMDataRef m_fse_dr_can_end_flight = NULL;

XPLMDataRef m_i_paused = NULL;;
XPLMDataRef m_i_sim_ground_speed = NULL;;
XPLMDataRef m_i_sim_time_speed = NULL;;

XPLMDataRef m_dr_f_fuel_total = NULL;;
XPLMDataRef m_dr_f_fuel_tanks = NULL;;

int m_fse_li_is_connected = 0;
int m_fse_li_is_flying = 0;
int m_fse_li_can_end_flight = 0;


int m_fse_li_is_flying_old = 0;
int m_fse_li_can_end_flight_old = 0;

int m_li_paused = 0;
int m_li_sim_ground_speed = 0;
int m_li_sim_time_speed = 0;


double m_ld_latitude;
double m_ld_longitude;
float m_lf_heading;


std::string m_config_path = "";
std::string m_ds = "";
std::vector<HRM_Airport*> m_fse_airports;


HRM_Airport* p_apt_departure = NULL;
HRM_Airport* p_apt_arrival = NULL;
std::string fse_avg_speed = default_distance;
float fse_airport_distance = 0;
float start_time = 0;
float stop_time = 0;

float sim_time = 0;


XPLMFlightLoopID beforeLoop = 0;
XPLMFlightLoopID afterLoop = 0;
XPLMFlightLoopID slowLoop = 0;

int mtbf_set_value = 10000;
float slow_flight_loop_time;

std::string fse_aircraft_name = "";
std::vector<FSE_Aircraft*> m_fse_aircraft;

FSE_Aircraft* p_current_fse_aircraft = NULL;
float fuel_start_level = 0;
float fse_nominal_flight_time = 0;

std::string fse_aircraft_name_str = "";
std::string fse_aircraft_cruise_str = "";
std::string fse_gph_nominal_str = "";
std::string fse_gph_actual_str = "";

float fse_gph_actual = 0;
float fse_cruise_actual = 0;

XPLMCommandRef cmdAdjustFuel = NULL;

void GetFSEAircraftData(void)
{
	try
	{
		p_current_fse_aircraft = NULL;
		fse_aircraft_name = "";

		char buffer[2048];
		char buffer2[2048];
		XPLMGetNthAircraftModel(0, buffer, buffer2);
		std::string ds = XPLMGetDirectorySeparator();

		std::string aircraft_directory = buffer2;

		std::size_t found = aircraft_directory.find_last_of(ds);

		if (found != std::string::npos)
		{
			

			aircraft_directory = aircraft_directory.substr(0, found + 1);
			std::string aircraft_ini_path = aircraft_directory + "xfse_alias.txt";

			fse_aircraft_name = aircraft_directory.substr(0, found);

			std::size_t found = fse_aircraft_name.find_last_of(ds);
			fse_aircraft_name = fse_aircraft_name.substr(found + 1, aircraft_directory.size() - (found + 1));

			std::ifstream ini_file;

			ini_file.open(aircraft_ini_path, std::ios::in);
			if (ini_file.is_open() == true)
			{
				std::string line;
				if (std::getline(ini_file, line))
				{
					fse_aircraft_name = line;
				}
			}
		}

		if (fse_aircraft_name.compare("") != 0)
		{
			for (auto p_aircraft : m_fse_aircraft)
			{
				if (fse_aircraft_name.compare(p_aircraft->name) == 0)
				{
					p_current_fse_aircraft = p_aircraft;
				}
			}
		}
	}
	catch (...)
	{
		XPLMDebugString("Read aircraft ini file crashed");
	}




}

void ReadFSEAircraft()
{
	XPLMDebugString("Starting loading acf.txt");

	std::string fse_line;
	std::ifstream fse_file(m_config_path + "acf.txt");
	std::string fse_delimiter = ",";
	size_t pos = 0;

	try
	{

		if (fse_file.is_open())
		{

			while (!fse_file.eof())
			{
				getline(fse_file, fse_line);

				//Name, AddtlCrew, Seats, Cruise, Fuel, FuelType, GPH, Payload (0fuel), price

				std::string name = "";
				std::string crew = "";
				std::string seats = "";
				std::string cruise = "";
				std::string fuel = "";
				std::string fuel_type = "";
				std::string gph = "";
				std::string payload = "";
				std::string price = "";

				double gph_double = 0;
				double cruise_double = 0;

				// Name
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					name = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}

				// Crew
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					crew = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}

				// Seats
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					seats = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}
				// Cruise
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					cruise = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}
				// Fuel
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					fuel = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}
				// Fuel Type
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					fuel_type = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}
				// GPH
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					gph = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}
				// Payload
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					payload = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}
				// Price
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					price = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}

				if (gph.compare("") != 0)
				{
					gph_double = std::stof(gph);
				}

				if (cruise.compare("") != 0)
				{
					cruise_double = std::stof(cruise);
				}


				if (name.compare("") != 0)
				{

					FSE_Aircraft* p_aircraft = new FSE_Aircraft();
					p_aircraft->name = name;
					p_aircraft->gph = gph_double;
					p_aircraft->cruise = cruise_double;


					m_fse_aircraft.push_back(p_aircraft);
				}
			}

			XPLMDebugString("Finished loading acf.tx");
			fse_file.close();
		}
		else
		{
			XPLMDebugString("Could not find acf.tx");
		}
	}
	catch (...)
	{
		XPLMDebugString("Error reading acf.txt");
	}
}




PLUGIN_API float GSFlightLoopCallbackBefore(float elapsedMe, float elapsedSim, int counter, void * refcon)
{

	return -1;
}

PLUGIN_API float GSFlightLoopCallbackAfter(float elapsedMe, float elapsedSim, int counter, void * refcon)
{



	return -1;
}

PLUGIN_API float GSFlightLoopCallbackSlow(float elapsedMe, float elapsedSim, int counter, void * refcon)
{
	m_ld_latitude = XPLMGetDatad(m_d_latitude);
	m_ld_longitude = XPLMGetDatad(m_d_longitude);
	m_lf_heading = XPLMGetDataf(m_f_heading);

	m_li_paused = XPLMGetDatai(m_i_paused);
	m_li_sim_ground_speed = XPLMGetDatai(m_i_sim_ground_speed);
	m_li_sim_time_speed = XPLMGetDatai(m_i_sim_time_speed);

	if (m_li_paused == 0) sim_time += elapsedMe * m_li_sim_ground_speed * m_li_sim_time_speed;

	if (m_fse_dr_is_connected == NULL) m_fse_dr_is_connected = XPLMFindDataRef("fse/status/connected");
	if (m_fse_dr_is_flying == NULL) m_fse_dr_is_flying = XPLMFindDataRef("fse/status/flying");
	if (m_fse_dr_can_end_flight == NULL) m_fse_dr_can_end_flight = XPLMFindDataRef("fse/status/canendflight");


	if (m_fse_dr_is_connected != NULL)		m_fse_li_is_connected = XPLMGetDatai(m_fse_dr_is_connected);
	if (m_fse_dr_is_flying != NULL)			m_fse_li_is_flying = XPLMGetDatai(m_fse_dr_is_flying);
	if (m_fse_dr_can_end_flight != NULL)	m_fse_li_can_end_flight = XPLMGetDatai(m_fse_dr_can_end_flight);



	if ((m_fse_li_is_flying == 1) && (m_fse_li_is_flying_old == 0))
	{
		p_apt_departure = GetClosestFSEAirport(m_ld_latitude, m_ld_longitude);
		start_time = sim_time;

		GetFSEAircraftData();
		fuel_start_level = XPLMGetDataf(m_dr_f_fuel_total);
	}
	else if ((m_fse_li_is_flying == 1) && (m_fse_li_can_end_flight_old == 0) && (m_fse_li_can_end_flight == 1))
	{
		p_apt_arrival = GetClosestFSEAirport(m_ld_latitude, m_ld_longitude);

		if ((p_apt_departure != NULL) && (p_apt_arrival != NULL))
		{
			fse_airport_distance = calc_distance_nm(p_apt_departure->latitude, p_apt_departure->longitude, p_apt_arrival->latitude, p_apt_arrival->longitude);

			fse_nominal_flight_time = 0;

			if ((p_current_fse_aircraft != NULL) && (p_current_fse_aircraft->cruise > 0))
			{
				fse_nominal_flight_time = fse_airport_distance / p_current_fse_aircraft->cruise;
			}
		}

	}
	else if (m_fse_li_is_flying == 0)
	{
		p_apt_departure = NULL;
		p_apt_arrival = NULL;
		fse_avg_speed = default_distance;
		fse_airport_distance = 0;
		fuel_start_level = 0;
		p_current_fse_aircraft = NULL;
		fse_nominal_flight_time = 0;
		fse_gph_actual = 0;
		fse_cruise_actual = 0;
		fse_nominal_flight_time = 0;

		fse_aircraft_name_str = "No Aircraft";
		fse_gph_nominal_str = "No Aircraft";
		fse_gph_actual_str = "No GPH";
		fse_aircraft_cruise_str = "No Aircraft";
	}

	if ((m_fse_li_is_flying == 1) && (p_apt_departure != NULL) && (p_apt_arrival != NULL))
	{
		if (fse_airport_distance <= 0.0f)
		{
			fse_avg_speed = "Zero Distance";
		}
		else if ((sim_time - start_time) <= 0.0f)
		{
			fse_avg_speed = "Zero Time";
		}
		else
		{
			fse_cruise_actual = fse_airport_distance / ((sim_time - start_time) / 3600.0f);

			fse_avg_speed = std::to_string((int)fse_cruise_actual) + "kt";
		}

		if (p_current_fse_aircraft != NULL)
		{
			fse_aircraft_cruise_str = std::to_string(p_current_fse_aircraft->cruise) + "kt";;
			fse_gph_nominal_str = std::to_string(p_current_fse_aircraft->gph) + "gph";;
			fse_aircraft_name_str = p_current_fse_aircraft->name;


			if (fse_nominal_flight_time > 0)
			{
				float fuel_act = XPLMGetDataf(m_dr_f_fuel_total);
				fse_gph_actual = (fuel_start_level - fuel_act) / (fse_nominal_flight_time * fse_fuel_weight);
				fse_gph_actual_str = std::to_string((int)fse_gph_actual) + "gph";
			}
			else
			{
				fse_gph_actual_str = "No GPH";
			}
		}

	}



	m_fse_li_is_flying_old = m_fse_li_is_flying;
	m_fse_li_can_end_flight_old = m_fse_li_can_end_flight;
	return 1;
}

int AdjustFuelCallback(XPLMCommandRef cmd, XPLMCommandPhase phase, void* refcon)
{
	if ((fuel_start_level > 0) && (fse_nominal_flight_time > 0) && (p_current_fse_aircraft != NULL))
	{
		float fuel_act = XPLMGetDataf(m_dr_f_fuel_total);
		float fuel_current_consumption = fuel_start_level - fuel_act;
		float fuel_max_level = fse_nominal_flight_time * p_current_fse_aircraft->gph * fse_fuel_weight;

		float fuel_adjust = fuel_max_level - fuel_current_consumption;

		

		if ((fuel_adjust) > 0)
		{
			float fuel_tanks[9];

			XPLMGetDatavf(m_dr_f_fuel_tanks, fuel_tanks, 0, 9);

			for (int index = 0; index < 9; index++)
			{
				float fuel_change = 0;
				if (fuel_tanks[index] < fuel_adjust)
					fuel_change = fuel_tanks[index];
				else
					fuel_change = fuel_adjust;

				fuel_tanks[index] -= fuel_change;
				fuel_adjust -= fuel_change;

				if (fuel_tanks[index] < 0)
					fuel_tanks[index] = 0;

				if (fuel_adjust < 0)
					fuel_adjust = 0;
			}
			
			XPLMSetDatavf(m_dr_f_fuel_tanks, fuel_tanks, 0, 9);


		}

	}
	return 1;
}

HRM_Airport* GetClosestFSEAirport(double lat, double lon)
{
	float distance_min = 1000000;
	HRM_Airport* p_closest_airport = NULL;

	for (auto p_airport : m_fse_airports)
	{
		double distance = abs(calc_distance_nm(lat, lon, p_airport->latitude, p_airport->longitude));

		if (distance <= distance_min)
		{
			p_closest_airport = p_airport;
			distance_min = distance;
		}
	}

	return p_closest_airport;
}

void MyDrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon)
{
	int		left, top, right, bottom;
	float	color[] = { 1.0, 1.0, 1.0 }; 	/* RGB White */

	float	color_red[] = { 1.0, 0.0, 0.0 }; 	/* RGB Red */
	float	color_green[] = { 0.0, 1.0, 0.0 }; 	/* RGB Green */
	float	color_yellow[] = { 1.0, 1.0, 0.0 }; 	/* RGB Yellow */

	/* First we get the location of the window passed in to us. */
	XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);

	XPLMDrawTranslucentDarkBox(left, top, right, bottom);
	//if (show_output > 0)
	{
		char buf[2048];

		float* p_color = color;

		sprintf(buf, "FSE ACF: %s", fse_aircraft_name_str.c_str());
		XPLMDrawString(p_color, left + 5, top - 10, buf, 0, xplmFont_Basic);

		sprintf(buf, "FSE AVG NOM: %s", fse_aircraft_cruise_str.c_str());
		XPLMDrawString(p_color, left + 5, top - 20, buf, 0, xplmFont_Basic);

		p_color = color_red;
		
		if (p_current_fse_aircraft != NULL)
		{
			if (fse_cruise_actual < (p_current_fse_aircraft->cruise))
				p_color = color_green;
			else if (fse_cruise_actual < (p_current_fse_aircraft->cruise * 1.5))
				p_color = color_yellow;
		}

		sprintf(buf, "FSE AVG CUR: %s", fse_avg_speed.c_str());
		XPLMDrawString(p_color, left + 5, top - 30, buf, 0, xplmFont_Basic);

		p_color = color;

		sprintf(buf, "FSE GPH NOM: %s", fse_gph_nominal_str.c_str());
		XPLMDrawString(p_color, left + 5, top - 40, buf, 0, xplmFont_Basic);

		p_color = color_red;

		if (p_current_fse_aircraft != NULL)
		{
			if (fse_gph_actual > (p_current_fse_aircraft->gph))
				p_color = color_green;
			else if (fse_gph_actual > (p_current_fse_aircraft->gph / 2))
				p_color = color_yellow;
		}

		sprintf(buf, "FSE GPH CUR: %s", fse_gph_actual_str.c_str());
		XPLMDrawString(p_color, left + 5, top - 50, buf, 0, xplmFont_Basic);
	}
}

void SaveConfig()
{
	XPLMDebugString("FunStuff: Writing config file");
	boost::property_tree::ptree pt;

	pt.put("FunStuff.MTBF", mtbf_set_value);

	boost::property_tree::ini_parser::write_ini(m_config_path + "FunStuff.ini", pt);
}

void ReadConfig()
{
	XPLMDebugString("FunStuff: Reading config file");
	boost::property_tree::ptree pt;
	try
	{
		boost::property_tree::ini_parser::read_ini(m_config_path + "FunStuff.ini", pt);
	}
	catch (...)
	{
		XPLMDebugString("FunStuff: Could not read config file");
		return;
	}

	try { mtbf_set_value = pt.get<int>("FunStuff.MTBF"); }
	catch (...) { XPLMDebugString("Ini File: Entry not found."); }

}



void MyHandleKeyCallback(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags, char inVirtualKey, void * inRefcon, int losingFocus)
{
}

int MyHandleMouseClickCallback(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse, void *inRefcon)
{
	return 1;
}

// Mac specific: this converts file paths from HFS (which we get from the SDK) to Unix (which the OS wants).
// See this for more info:
//
// http://www.xsquawkbox.net/xpsdk/mediawiki/FilePathsAndMacho

#if APL
int ConvertPath(const char * inPath, char * outPath, int outPathMaxLen) {

	CFStringRef inStr = CFStringCreateWithCString(kCFAllocatorDefault, inPath, kCFStringEncodingMacRoman);
	if (inStr == NULL)
		return -1;
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLHFSPathStyle, 0);
	CFStringRef outStr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	if (!CFStringGetCString(outStr, outPath, outPathMaxLen, kCFURLPOSIXPathStyle))
		return -1;
	CFRelease(outStr);
	CFRelease(url);
	CFRelease(inStr);
	return 0;
}
#endif



// Initialization code.

static float InitPlugin(float elapsed, float elapsed_sim, int counter, void * ref)
{
	XPLMDebugString("FunStuff: Initializing.\n");

	m_ds = XPLMGetDirectorySeparator();

	char buffer[2048];
	XPLMGetSystemPath(buffer);
	std::string m_system_path = buffer;


	m_config_path = m_system_path + "Resources" + m_ds + "plugins" + m_ds + "FunStuff" + m_ds;

	ReadConfig();

	enable_random_failures = XPLMFindDataRef("sim/operation/failures/enable_random_falures");
	set_mtbf = XPLMFindDataRef("sim/operation/failures/mean_time_between_failure_hrs");

	m_d_latitude = XPLMFindDataRef("sim/flightmodel/position/latitude");
	m_d_longitude = XPLMFindDataRef("sim/flightmodel/position/longitude");
	m_f_heading = XPLMFindDataRef("sim/flightmodel/position/true_psi");

	m_i_paused = XPLMFindDataRef("sim/time/paused");
	m_i_sim_ground_speed = XPLMFindDataRef("sim/time/ground_speed");
	m_i_sim_time_speed = XPLMFindDataRef("sim/time/sim_speed");

	m_dr_f_fuel_total = XPLMFindDataRef("sim/flightmodel/weight/m_fuel_total");

	m_dr_f_fuel_tanks = XPLMFindDataRef("sim/flightmodel/weight/m_fuel");

	


	XPLMCreateFlightLoop_t *flightloop = new XPLMCreateFlightLoop_t();
	flightloop->callbackFunc = GSFlightLoopCallbackBefore;
	flightloop->phase = 0;
	flightloop->refcon = NULL;
	beforeLoop = XPLMCreateFlightLoop(flightloop);

	flightloop = new XPLMCreateFlightLoop_t();
	flightloop->callbackFunc = GSFlightLoopCallbackAfter;
	flightloop->phase = 1;
	flightloop->refcon = NULL;
	afterLoop = XPLMCreateFlightLoop(flightloop);

	flightloop = new XPLMCreateFlightLoop_t();
	flightloop->callbackFunc = GSFlightLoopCallbackSlow;
	flightloop->phase = 1;
	flightloop->refcon = NULL;
	slowLoop = XPLMCreateFlightLoop(flightloop);

	cmdAdjustFuel = XPLMCreateCommand("FunStuff/Adjust_Fuel", "Adjust Fuel Weight");
	XPLMRegisterCommandHandler(cmdAdjustFuel, AdjustFuelCallback, 0, 0);

	char buf[2048];
	sprintf(buf, "GS: BeforeLoop = %u.\n", (unsigned int)beforeLoop);

	XPLMDebugString(buf);

	ReadFSEAirports();
	ReadFSEAircraft();

	XPLMScheduleFlightLoop(beforeLoop, -1, 0);
	XPLMScheduleFlightLoop(afterLoop, -1, 0);
	XPLMScheduleFlightLoop(slowLoop, 1, 0);

	WindowId = XPLMCreateWindow(10, 200, 500, 500, 1, MyDrawWindowCallback, MyHandleKeyCallback, MyHandleMouseClickCallback, 0);

	

	return 0.0f;
}

PLUGIN_API int XPluginStart(char * name, char * sig, char * desc)
{
	XPLMDebugString("GS: Startup.\n");

	strcpy(name, "FunStuff");
	strcpy(sig, "k80.FunStuff");
	strcpy(desc, "FunStuff Plugin");

	

	XPLMRegisterFlightLoopCallback(InitPlugin, -1.0, NULL);

	/**/

	return 1;
}

PLUGIN_API void XPluginStop(void)
{
	XPLMDebugString("FunStuff: Stop.\n");
	SaveConfig();

}

PLUGIN_API int XPluginEnable(void)
{
	XPLMDebugString("FunStuff: Enable.\n");
	//XPLMScheduleFlightLoop(beforeLoop, 1, 0);
	return 1;
}

PLUGIN_API void XPluginDisable(void)
{
	XPLMDebugString("FunStuff: Stop.\n");
	SaveConfig();
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int msg, void * p)
{
	XPLMDebugString("FunStuff: Receive Message.\n");

	XPLMSetDataf(set_mtbf, mtbf_set_value);
	XPLMSetDatai(enable_random_failures, 1);
}



void ReadFSEAirports()
{
	XPLMDebugString("Starting loading icaodata.csv");

	std::string fse_line;
	std::ifstream fse_file(m_config_path + "icaodata.csv");
	std::string fse_delimiter = ",";
	size_t pos = 0;

	try
	{

		if (fse_file.is_open())
		{

			while (!fse_file.eof())
			{
				getline(fse_file, fse_line);


				std::string apt_icao = "";
				std::string apt_lat = "";
				std::string apt_long = "";
				std::string apt_name = "";
				std::string apt_type = "";
				std::string apt_size = "";
				double apt_longitude = 0;
				double apt_latitude = 0;
				int pos_long = 0;
				int pos_lat = 0;

				//ICAO
				if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
				{
					apt_icao = fse_line.substr(0, pos);
					fse_line.erase(0, pos + fse_delimiter.length());
				}

				// Check for header line
				if ((apt_icao.compare("icao") != 0) && (pos != std::string::npos))
				{
					// Get Latitude
					if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
					{
						apt_lat = fse_line.substr(0, pos);
						apt_latitude = std::stof(apt_lat);
						pos_lat = 90 + ((int)apt_latitude);
						fse_line.erase(0, pos + fse_delimiter.length());
					}

					// Get Longitude
					if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
					{
						apt_long = fse_line.substr(0, pos);
						apt_longitude = std::stof(apt_long);
						pos_long = 180 + ((int)apt_longitude);
						fse_line.erase(0, pos + fse_delimiter.length());
					}

					// Get Type
					if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
					{
						apt_type = fse_line.substr(0, pos);
						fse_line.erase(0, pos + fse_delimiter.length());
					}

					// Get Size
					if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
					{
						apt_size = fse_line.substr(0, pos);
						fse_line.erase(0, pos + fse_delimiter.length());
					}

					// Get Name
					if ((pos = fse_line.find(fse_delimiter)) != std::string::npos)
					{
						apt_name = fse_line.substr(0, pos);
						fse_line.erase(0, pos + fse_delimiter.length());
					}

					HRM_Airport* p_airport = new HRM_Airport();

					p_airport->icao = apt_icao;
					p_airport->name = apt_name;
					p_airport->longitude = apt_longitude;
					p_airport->latitude = apt_latitude;

					m_fse_airports.push_back(p_airport);
				}


			}




			XPLMDebugString("Finished loading icaodata.csv");
			fse_file.close();
		}
		else
		{
			XPLMDebugString("Could not find icaodata.csv");
		}
	}
	catch (...)
	{
		XPLMDebugString("Error reading icaodata.csv");
	}
}

double calc_distance_nm(double lat1, double long1, double lat2, double long2)
{
	lat1 = lat1 * M_PI / 180;
	long1 = long1 * M_PI / 180;
	lat2 = lat2 * M_PI / 180;
	long2 = long2 * M_PI / 180;

	double rEarth = 6372.797;

	double dlat = lat2 - lat1;
	double dlong = long2 - long1;

	double x1 = sin(dlat / 2);
	double x2 = cos(lat1);
	double x3 = cos(lat2);
	double x4 = sin(dlong / 2);

	double x5 = x1 * x1;
	double x6 = x2 * x3 * x4 * x4;

	double temp1 = x5 + x6;

	double y1 = sqrt(temp1);
	double y2 = sqrt(1.0 - temp1);

	double temp2 = 2 * atan2(y1, y2);

	double rangeKm = temp2 * rEarth;

	double CalcRange = rangeKm * 0.539957;

	return CalcRange;
}

