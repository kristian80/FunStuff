#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

// OS X: we use this to convert our file path.
#if APL
#include <Carbon/Carbon.h>
#endif






/**************************************************************************************************************
 * Global Variables
 **************************************************************************************************************/

XPLMDataRef enable_random_failures = NULL;
XPLMDataRef set_mtbf = NULL;

XPLMFlightLoopID beforeLoop = 0;
XPLMFlightLoopID afterLoop = 0;
XPLMFlightLoopID slowLoop = 0;

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



	return 1;
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

	

	enable_random_failures = XPLMFindDataRef("sim/operation/failures/enable_random_falures");
	set_mtbf = XPLMFindDataRef("sim/operation/failures/mean_time_between_failure_hrs");




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

	char buf[2048];
	sprintf(buf, "GS: BeforeLoop = %u.\n", (unsigned int)beforeLoop);

	XPLMDebugString(buf);

	XPLMScheduleFlightLoop(beforeLoop, -1, 0);
	XPLMScheduleFlightLoop(afterLoop, -1, 0);
	XPLMScheduleFlightLoop(slowLoop, 1, 0);

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
	XPLMDebugString("GS: Stop.\n");

}

PLUGIN_API int XPluginEnable(void)
{
	XPLMDebugString("GS: Enable.\n");
	//XPLMScheduleFlightLoop(beforeLoop, 1, 0);
	return 1;
}

PLUGIN_API void XPluginDisable(void)
{
	XPLMDebugString("GS: Stop.\n");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int msg, void * p)
{
	XPLMDebugString("FunStuff: Receive Message.\n");

	XPLMSetDataf(set_mtbf, 10000);
	XPLMSetDatai(enable_random_failures, 1);
}

