//-------------------------------------------------------------------------------------------------
/**
 * @file gnss.c
 *
 * Tool to debug/monitor GNSS device.
 *
 * Copyright (C) Sierra Wireless Inc.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved
 */
//-------------------------------------------------------------------------------------------------


#include "legato.h"
#include "interfaces.h"

//-------------------------------------------------------------------------------------------------
/**
 * Default time(in second) for 3D fixing after starting gnss device.
 */
//-------------------------------------------------------------------------------------------------
#define DEFAULT_3D_FIX_TIME   60


//-------------------------------------------------------------------------------------------------
/**
 * Default watch period(in second) to get positioning information.
 */
//-------------------------------------------------------------------------------------------------
#define DEFAULT_WATCH_PERIOD   10*60


//-------------------------------------------------------------------------------------------------
/**
 * Max characters for constellations name.
 */
//-------------------------------------------------------------------------------------------------
#define CONSTELLATIONS_NAME_LEN     256


//-------------------------------------------------------------------------------------------------
/**
 * Base 10
 */
//-------------------------------------------------------------------------------------------------
#define BASE10 10

//-------------------------------------------------------------------------------------------------
/**
 * Different type of constellation.
 * {@
 */
//-------------------------------------------------------------------------------------------------
#define CONSTELLATION_GPS           0x1
#define CONSTELLATION_GLONASS       0x2
#define CONSTELLATION_BEIDOU        0x4
#define CONSTELLATION_GALILEO       0x8
#define CONSTELLATION_SBAS          0x10  //TelSDK supported this constellation
#define CONSTELLATION_QZSS          0x20
#define CONSTELLATION_NAVIC         0x40
// @}

//-------------------------------------------------------------------------------------------------
/**
 * Position handler reference.
 */
//-------------------------------------------------------------------------------------------------
static le_gnss_PositionHandlerRef_t PositionHandlerRef;


//-------------------------------------------------------------------------------------------------
/**
 * Array to store get parameter name.
 */
//-------------------------------------------------------------------------------------------------
static char ParamsName[128] = "";

//-------------------------------------------------------------------------------------------------
/**
 * Print the help text to stdout.
 */
//-------------------------------------------------------------------------------------------------
void PrintGnssHelp
(
    void
)
{
    puts("\n\t\tNAME:\n"
         "\t\t\tgnss - Used to access different functionality of gnss\n\n"
         "\t\tSYNOPSIS:\n"
         "\t\t\tgnss help\n"
         "\t\t\tgnss <enable/disable>\n"
         "\t\t\tgnss <start/stop>\n"
         "\t\t\tgnss restart <RestartType>\n"
         "\t\t\tgnss fix [FixTime in seconds]\n"
         "\t\t\tgnss get <parameter>\n"
         "\t\t\tgnss get posInfo\n"
         "\t\t\tgnss set constellation <ConstellationType>\n"
         "\t\t\tgnss set agpsMode <ModeType>\n"
         "\t\t\tgnss set acqRate <acqRate in milliseconds>\n"
         "\t\t\tgnss set nmeaSentences <nmeaMask>\n"
         "\t\t\tgnss set minElevation <minElevation in degrees>\n"
         "\t\t\tgnss set startMode <StartMode>\n"
         "\t\t\t\t  be as follows:\n"
         "\t\t\t\t\t- 0 ---> HOT\n"
         "\t\t\t\t\t- 1 ---> WARM\n"
         "\t\t\t\t\t- 2 ---> COLD\n"
         "\t\t\t\t\t- 3 ---> FACTORY\n"
         "\t\t\t\t\t- 4 ---> UNKNOWN\n"
         "\t\t\tgnss watch [WatchPeriod in seconds]\n\n"
         "\t\tDESCRIPTION:\n"
         "\t\t\tgnss help\n"
         "\t\t\t\t- Print this help message and exit\n\n"
         "\t\t\tgnss <enable/disable>\n"
         "\t\t\t\t- Enable/disable gnss device\n\n"
         "\t\t\tgnss <start/stop>\n"
         "\t\t\t\t- Start/stop gnss device\n\n"
         "\t\t\tgnss startType <EngineType>\n"
         "\t\t\t\t- Set GNSS device with the specified engine type. Type can be as follows:\n"
         "\t\t\t\t\t- 0 ---> FUSED\n"
         "\t\t\t\t\t- 1 ---> SPE\n"
         "\t\t\t\t\t- 2 ---> PPE\n"
         "\t\t\t\t\t- 3 ---> VPE\n\n"
         "\t\t\tgnss ConfigLevArm <forwardOffset(double)> <sidewaysOffset(double)> <upOffset(double)> <levArmType>\n"
         "\t\t\t\t- Configure Lever Arm Parameters to support QDR.levArmType can be as follows:\n"
         "\t\t\t\t\t- GNSS_TO_VRP->1\n"
         "\t\t\t\t\t- DR_IMU_TO_GNSS->2\n"
         "\t\t\t\t\t- VPE_IMU_TO_GNSS->3\n\n"
         "\t\t\tgnss ConfigDR <rollOffset(double)> <yawOffset(double)> <pitchOffset(double)> <offsetUnc(double)>\n"
         "\t\t\t\t     <speedFactor(double)> <speedFactorUnc(double)> <gyroFactor(double)> <gyroFactorUnc(double>\n"
         "\t\t\t\t- Configure Dead Reckoning Engine Parameters to support QDR.\n\n"
         "\t\t\tgnss set configEng <EngineType> <EngineState>\n"
         "\t\t\t\t  Engine type be as follows:\n"
         "\t\t\t\t\t- 0 ---> UNKNOWN\n"
         "\t\t\t\t\t- 1 ---> SPE(Currently not supported)\n"
         "\t\t\t\t\t- 2 ---> PPE(Currently not supported)\n"
         "\t\t\t\t\t- 3 ---> DRE(supported only)\n"
         "\t\t\t\t\t- 4 ---> VPE(Currently not supported)\n"
         "\t\t\t\t  Engine state be as follows:\n"
         "\t\t\t\t\t- 0 ---> UNKNOWN\n"
         "\t\t\t\t\t- 1 ---> SUSPEND\n"
         "\t\t\t\t\t- 2 ---> RUNNING\n"
         "\t\t\tgnss set robustloc <enable> <enabled911>\n"
         "\t\t\t\t- Configuring robust location information be as follows:\n"
         "\t\t\t\t\t-  enable->1 for enabling  enable->0 for disabling\n"
         "\t\t\t\t\t-  enable911->1 for enabling enable911->0 for disabling\n"
         "\t\t\tgnss set secondBandConst <constellation type>"
         "\t\t\t\t  constellation type be as follows:\n"
         "\t\t\t\t\t- -1 ---> UNKNOWN\n"
         "\t\t\t\t\t-  1 ---> GPS\n"
         "\t\t\t\t\t-  2 ---> GALILEO\n"
         "\t\t\t\t\t-  4 ---> SBAS\n"
         "\t\t\t\t\t-  8 ---> COMPAS(Not supported)\n"
         "\t\t\t\t\t-  16 ---> GLONASS\n"
         "\t\t\t\t\t-  32 ---> BDS\n"
         "\t\t\t\t\t-  64 ---> QZSS\n"
         "\t\t\t\t\t-  128 ---> NAVIC\n"
         "\t\t\tgnss restart <RestartType>\n"
         "\t\t\t\t- Restart gnss device. Allowed when device in 'active' state. Restart type can\n"
         "\t\t\t\t  be as follows:\n"
         "\t\t\t\t\t- hot\n"
         "\t\t\t\t\t- warm\n"
         "\t\t\t\t\t- cold\n"
         "\t\t\t\t\t- factory\n"
         "\t\t\t\tTo know more about these restart types, please look at: \n"
         "\t\t\t\t           https://docs.legato.io/latest/c_gnss.html\n\n"
         "\t\t\tgnss fix [FixTime in seconds]\n"
         "\t\t\t\t- Loop for certain time for first position fix. Here, FixTime is optional.\n"
         "\t\t\t\t  Default time(60s) will be used if not specified\n\n"
         "\t\t\tgnss supportedNmeaSentences --> Supported NMEA sentences (bit mask)\n\n"
         "\t\t\tgnss supportedConstellations --> Supported Constellations (bit mask)\n\n"
         "\t\t\tgnss configDefSecBand --> Configure default secondary band constellations\n\n"
         "\t\t\tgnss get <parameter>\n"
         "\t\t\t\t- Used to get different gnss parameter.\n"
         "\t\t\t\t  Follows parameters and their descriptions :\n"
         "\t\t\t\t\t- ttff          --> Time to First Fix (milliseconds)\n"
         "\t\t\t\t\t- acqRate       --> Acquisition Rate (unit milliseconds)\n"
         "\t\t\t\t\t- agpsMode      --> Agps Mode\n"
         "\t\t\t\t\t- nmeaSentences --> Enabled NMEA sentences (bit mask)\n"
         "\t\t\t\t\t- minElevation  --> Minimum elevation in degrees\n"
         "\t\t\t\t\t- constellation --> GNSS constellation\n"
         "\t\t\t\t\t- secondBandConst --> Secondary band Constellations\n"
         "\t\t\t\t\t- robustloc     --> Robust location information\n"
         "\t\t\t\t\t- magDev        --> Magnitude deviation\n"
         "\t\t\t\t\t- elliUnc       --> Elliptical Uncertainity\n"
         "\t\t\t\t\t- posState      --> Position fix state(no fix, 2D, 3D etc)\n"
         "\t\t\t\t\t- loc2d         --> 2D location (latitude, longitude, horizontal accuracy)\n"
         "\t\t\t\t\t- alt           --> Altitude (Altitude, Vertical accuracy)\n"
         "\t\t\t\t\t- altOnWgs84    --> Altitude with respect to the WGS-84 ellipsoid\n"
         "\t\t\t\t\t- loc3d         --> 3D location (latitude, longitude, altitude,\n"
         "\t\t\t\t\t                horizontal accuracy, vertical accuracy)\n"
         "\t\t\t\t\t- gpsTime       --> Get last updated gps time\n"
         "\t\t\t\t\t- time          --> Time of the last updated location\n"
         "\t\t\t\t\t- epochTime     --> Epoch time of the last updated location\n"
         "\t\t\t\t\t- timeAcc       --> Time accuracy in milliseconds\n"
         "\t\t\t\t\t- LeapSeconds   --> Current and next leap seconds\n"
         "\t\t\t\t\t- GpsLeapSeconds --> UTC leap seconds in advance in seconds\n"
         "\t\t\t\t\t- date          --> Date of the last updated location\n"
         "\t\t\t\t\t- hSpeed        --> Horizontal speed(Horizontal Speed, Horizontal\n"
         "\t\t\t\t\t                    Speed accuracy)\n"
         "\t\t\t\t\t- vSpeed        --> Vertical speed(Vertical Speed, Vertical Speed accuracy)\n"
         "\t\t\t\t\t- motion        --> Motion data (Horizontal Speed, Horizontal Speed accuracy,\n"
         "\t\t\t\t\t                    Vertical Speed, Vertical Speed accuracy)\n"
         "\t\t\t\t\t- direction     --> Direction indication\n"
         "\t\t\t\t\t- satInfo       --> Satellites Vehicle information\n"
         "\t\t\t\t\t- satStat       --> Satellites Vehicle status\n"
         "\t\t\t\t\t- dop           --> Dilution of Precision for the fixed position. Displayed\n"
         "\t\t\t\t\t-               in all resolutions: (0 to 3 digits after the decimal point) \n"
         "\t\t\t\t\t- posInfo       --> Get all current position info of the device\n"
         "\t\t\t\t\t- status        --> Get gnss device's current status\n\n"
         "\t\t\tgnss set constellation <ConstellationType>\n"
         "\t\t\t\t- Used to set constellation. Allowed when device in 'ready/Active' state. May require\n"
         "\t\t\t\t  platform reboot, please look platform documentation for details.\n"
         "\t\t\t\t  ConstellationType can be as follows:\n"
         "\t\t\t\t\t- 1 ---> GPS(Not Supported)\n"
         "\t\t\t\t\t- 2 ---> GLONASS\n"
         "\t\t\t\t\t- 4 ---> BEIDOU\n"
         "\t\t\t\t\t- 8 ---> GALILEO\n"
         "\t\t\t\t\t- 16 --> SBAS\n"
         "\t\t\t\t\t- 32 --> QZSS\n"
         "\t\t\t\t\t- 64 --> NAVIC\n"
         "\t\t\t\tPlease use sum of the values to set multiple constellation, e.g.\n"
         "\t\t\t\t10 for GLONASS+GALILEO, 46 for GLONASS+BEIDOU+GALILEO+QZSS\n\n"
         "\t\t\tgnss set agpsMode <ModeType>\n"
         "\t\t\t\t- Used to set agps mode. ModeType can be as follows:\n"
         "\t\t\t\t\t- alone -----> Standalone agps mode\n"
         "\t\t\t\t\t- msBase ----> MS-based agps mode\n"
         "\t\t\t\t\t- msAssist --> MS-assisted agps mode\n\n"
         "\t\t\tgnss set acqRate <acqRate in milliseconds>\n"
         "\t\t\t\t- Used to set acquisition rate.\n"
         "\t\t\t\t  Please note that it is available when the device is 'ready' state.\n\n"
         "\t\t\tgnss set nmeaSentences <nmeaMask>\n"
         "\t\t\t\t- Used to set the enabled NMEA sentences. \n"
         "\t\t\t\t  Bit mask should be set with hexadecimal values, e.g. 7FFF\n\n"
         "\t\t\t\t- Used to set nmea sentences. Allowed when device in 'ready' state. May require\n"
         "\t\t\t\t  platform reboot, please look platform documentation for details.\n"
         "\t\t\t\t  nmeaMask can be as follows (the values are in hexadecimal):\n"
         "\t\t\t\t\t- 1 ------> GPGGA\n"
         "\t\t\t\t\t- 2 ------> GARMC\n"
         "\t\t\t\t\t- 4 ------> GNGSA\n"
         "\t\t\t\t\t- 8 ------> GPVTG\n"
         "\t\t\t\t\t- 10 -----> GPGNS\n"
         "\t\t\t\t\t- 20 -----> GPDTM\n"
         "\t\t\t\t\t- 40 -----> GPGSV\n"
         "\t\t\t\t\t- 80 -----> GLGSV\n"
         "\t\t\t\t\t- 100 ----> GAGSV\n"
         "\t\t\t\t\t- 200 ----> GQGSV\n"
         "\t\t\t\t\t- 400 ----> GBGSV\n"
         "\t\t\t\t\t- 800 ----> GIGSV\n"
         "\t\t\t\t\t- FFFFFFFF ---> ALL\n"
         "\t\t\tgnss set minElevation <minElevation in degrees>\n"
         "\t\t\t\t- Used to set the minimum elevation in degrees [range 0..90].\n\n"
         "\t\t\tgnss watch [WatchPeriod in seconds]\n"
         "\t\t\t\t- Used to monitor all gnss information(position, speed, satellites used etc).\n"
         "\t\t\t\t  Here, WatchPeriod is optional. Default time(600s) will be used if not\n"
         "\t\t\t\t  specified\n\n"
         "\tPlease note, some commands require gnss device to be in specific state\n"
         "\t(and platform reboot) to produce valid result. Please look :\n"
         "\thttps://docs.legato.io/latest/howToGNSS.html,\n"
         "\thttps://docs.legato.io/latest/c_gnss.html and platform documentation for more\n"
         "\tdetails.\n"
         );
}


//-------------------------------------------------------------------------------------------------
/**
 * This function enables gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int Enable
(
    void
)
{
    le_result_t result = LE_FAULT;

    result = le_gnss_Enable();

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_DUPLICATE:
            printf("The GNSS device is already enabled\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not initialized\n");
            break;
        case LE_FAULT:
            printf("Failed to enable GNSS device\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}



//-------------------------------------------------------------------------------------------------
/**
 * This function disables gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int Disable
(
    void
)
{
    le_result_t result = LE_FAULT;

    result = le_gnss_Disable();

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_DUPLICATE:
            printf("The GNSS device is already disabled\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not initialized or started. Please see log for details\n");
            break;
        case LE_FAULT:
            printf("Failed to disable GNSS device\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function starts gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int Start
(
    void
)
{
    le_result_t result = LE_FAULT;

    result = le_gnss_Start();

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_DUPLICATE:
            printf("The GNSS device is already started\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is disabled or not initialized. See logs for details\n");
            break;
        case LE_FAULT:
            printf("Failed to start GNSS device. See logs for details\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * This function starts the GNSS device in the specified engine type.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int StartEngineType
(
    const char* startTypePtr           ///< [IN] Start type
)
{
    char *end;
    uint32_t type = strtoul(startTypePtr, &end, BASE10);

    if ('\0' != end[0])
    {
        printf("Bad type : %s\n", startTypePtr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_SetEngineType(type);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to start the specified Engine type\n");
            break;
        case LE_BAD_PARAMETER:
            printf("Bad parameter to set SetEngineType\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not ready state\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * This function configures Lever Arm Parameters.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int ConfigureLevArm
(
    taf_gnss_LeverArmParams_t* LeverArmParams         ///< [IN] Lever Arm Parameters
)
{

    le_result_t result = le_gnss_SetLeverArmConfig(LeverArmParams);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to configure Lever Arm parameters\n");
            break;
        case LE_BAD_PARAMETER:
            printf("Bad parameter to configure Lever Arm parameters\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in Ready State\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }
    //free the LeverArmParams memory
    le_mem_Release(LeverArmParams);
    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * This function configures Dead Reckoning Engine Parameters.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int ConfigureDeadReckoning
(
    taf_gnss_DrParams_t* DrParams         ///< [IN] Lever Arm Parameters
)
{

    le_result_t result = le_gnss_SetDRConfig(DrParams);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to configure Dead Reckoning Engine parameters\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in Ready State\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }
    //free the DrParams memory
    le_mem_Release(DrParams);
    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function stops gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int Stop
(
    void
)
{
    le_result_t result = LE_FAULT;

    result = le_gnss_Stop();

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_DUPLICATE:
            printf("The GNSS device is already stopped\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not initialized or disabled. See logs for details\n");
            break;
        case LE_FAULT:
            printf("Failed to stop GNSS device. See logs for details\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function restarts gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int Restart
(
    const char* restartTypePtr      ///< [IN] Type of restart, i.e. hot/warm/cold/factory etc
)
{
    le_result_t result = LE_FAULT;

    if (strcmp(restartTypePtr, "cold") == 0)
    {
        printf("Doing cold restart...\n");
        result = le_gnss_ForceColdRestart();
    }
    else if (strcmp(restartTypePtr, "warm") == 0)
    {
        printf("Doing warm restart...\n");
        result = le_gnss_ForceWarmRestart();
    }
    else if (strcmp(restartTypePtr, "hot") == 0)
    {
        printf("Doing hot restart...\n");
        result = le_gnss_ForceHotRestart();
    }
    else if (strcmp(restartTypePtr, "factory") == 0)
    {
        printf("Doing factory restart...\n");
        result = le_gnss_ForceFactoryRestart();
    }
    else
    {
        printf("Invalid parameter: %s\n", restartTypePtr);
        return EXIT_FAILURE;
    }

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not enabled or not started. See logs for details\n");
            break;
        case LE_FAULT:
            printf("Failed to do '%s' restart. See logs for details\n", restartTypePtr);
            break;
        case LE_UNSUPPORTED:
            printf("Not Supported!\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function sets gnss device acquisition rate.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int SetAcquisitionRate
(
    const char* acqRateStr          ///< [IN] Acquisition rate in milliseconds
)
{
    char *end;
    uint32_t acqRate = strtoul(acqRateStr, &end, BASE10);

    if ('\0' != end[0])
    {
        printf("Bad acquisition rate: %s\n", acqRateStr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_SetAcquisitionRate(acqRate);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to Set acquisition rate\n");
            break;
        case LE_UNSUPPORTED:
            printf("Request is not supported\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in \"Ready \" state\n");
            break;
        case LE_TIMEOUT:
            printf("Timeout error\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function sets the GNSS minimum elevation.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int SetMinElevation
(
    const char* minElevationPtr           ///< [IN] Minimum elevation in degrees [range 0..90]
)
{
    char *end;
    uint32_t minElevation = strtoul(minElevationPtr, &end, BASE10);

    if ('\0' != end[0])
    {
        printf("Bad minimum elevation: %s\n", minElevationPtr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_SetMinElevation(minElevation);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to set the minimum elevation\n");
            break;
        case LE_UNSUPPORTED:
            printf("Setting the minimum elevation is not supported\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in \"Ready\" state\n");
            break;
        case LE_OUT_OF_RANGE:
            printf("The minimum elevation is above range\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function starts the GNSS device in the specified start mode.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int StartMode
(
    const char* startModePtr           ///< [IN] Start mode
)
{
    char *end;
    uint32_t mode = strtoul(startModePtr, &end, BASE10);

    if ('\0' != end[0])
    {
        printf("Bad mode : %s\n", startModePtr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_StartMode(mode);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to set the specified start Mode\n");
            break;
        case LE_DUPLICATE:
            printf("The GNSS device is already in Active State\n");
            break;
        case LE_BAD_PARAMETER:
            printf("Bad parameter to set StartMode\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in Ready State\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function configure Engine state for the specified engine type.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int ConfigureEngineState
(
    const char* engineTypePtr,           ///< [IN] Engine type
    const char* engineStatePtr           ///< [IN] Engine state
)
{
    char *end;
    uint32_t engineType = strtoul(engineTypePtr, &end, BASE10);

    if ('\0' != end[0])
    {
        printf("Bad engine type : %s\n", engineTypePtr);
        return EXIT_FAILURE;
    }

    uint32_t engineState = strtoul(engineStatePtr, &end, BASE10);
    if ('\0' != end[0])
    {
        printf("Bad engine state : %s\n", engineStatePtr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_ConfigureEngineState(engineType,engineState);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to set engine state for the specified engine type\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in Ready/Active State\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function configure Robust Location finromation.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int ConfigureRobustLocation
(
    const char* enablePtr,           ///< [IN] Enable
    const char* enable911Ptr         ///< [IN] Enable911
)
{
    char *end;
    uint32_t enable = strtoul(enablePtr, &end, BASE10);

    if ('\0' != end[0])
    {
        printf("Bad enable : %s\n", enablePtr);
        return EXIT_FAILURE;
    }

    uint32_t enable911 = strtoul(enable911Ptr, &end, BASE10);
    if ('\0' != end[0])
    {
        printf("Bad enable911 : %s\n", enable911Ptr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_ConfigureRobustLocation(enable,enable911);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to configure Robust location information\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in Ready/Active State\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function configure secondary band constellations.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int ConfigureSecondaryBandConstellations
(
    const char* secondBandConstPtr        ///< [IN] secondary band constellation
)
{
    uint32_t constellationMask = 0;

    char *endPtr;
    errno = 0;
    int constellationSum = strtoul(secondBandConstPtr, &endPtr, 10);

    if (endPtr[0] != '\0' || errno != 0 || constellationSum == 0)
    {
        fprintf(stderr, "Bad constellation parameter: %s\n", secondBandConstPtr);
        exit(EXIT_FAILURE);
    }

    char constellationStr[CONSTELLATIONS_NAME_LEN] = "[";

    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_GPS-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_GPS-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_GPS-1));
        le_utf8_Append(constellationStr, "GPS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_GALILEO-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_GALILEO-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_GALILEO-1));
        le_utf8_Append(constellationStr, "GALILEO ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_SBAS-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_SBAS-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_SBAS-1));
        le_utf8_Append(constellationStr, "SBAS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_GLONASS-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_GLONASS-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_GLONASS-1));
        le_utf8_Append(constellationStr, "GLONASS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_BDS-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_BDS-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_BDS-1));
        le_utf8_Append(constellationStr, "BDS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_QZSS-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_QZSS-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_QZSS-1));
        le_utf8_Append(constellationStr, "QZSS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & (1<<(LE_GNSS_SB_CONSTELLATION_NAVIC-1)))
    {
        constellationMask |= (uint32_t)(1<<(LE_GNSS_SB_CONSTELLATION_NAVIC-1));
        constellationSum -= (1<<(LE_GNSS_SB_CONSTELLATION_NAVIC-1));
        le_utf8_Append(constellationStr, "NAVIC ", sizeof(constellationStr), NULL);
    }
    le_utf8_Append(constellationStr, "]", sizeof(constellationStr), NULL);

    LE_INFO("Configuring secondary constellation %s",constellationStr);

    // Right now all constellation sum should be zero
    if (constellationSum != 0)
    {
        fprintf(stderr, "Bad constellation parameter: %s\n", secondBandConstPtr);
        exit(EXIT_FAILURE);
    }

    le_result_t result = le_gnss_ConfigureSecondaryBandConstellations(constellationMask);

    switch (result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_FAULT:
            printf("Failed to configure secondary band constellations\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in Ready State\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function sets constellation of gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int SetConstellation
(
    const char* constellationPtr   ///< [IN] GNSS constellation used in solution
)
{
    uint32_t constellationMask = 0;

    char *endPtr;
    errno = 0;
    int constellationSum = strtoul(constellationPtr, &endPtr, 10);

    if (endPtr[0] != '\0' || errno != 0 || constellationSum == 0)
    {
        fprintf(stderr, "Bad constellation parameter: %s\n", constellationPtr);
        exit(EXIT_FAILURE);
    }

    char constellationStr[CONSTELLATIONS_NAME_LEN] = "[";

    if (constellationSum & CONSTELLATION_GPS)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_GPS;
        constellationSum -= CONSTELLATION_GPS;
        le_utf8_Append(constellationStr, "GPS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & CONSTELLATION_GLONASS)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_GLONASS;
        constellationSum -= CONSTELLATION_GLONASS;
        le_utf8_Append(constellationStr, "GLONASS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & CONSTELLATION_BEIDOU)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_BEIDOU;
        constellationSum -= CONSTELLATION_BEIDOU;
        le_utf8_Append(constellationStr, "BEIDOU ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & CONSTELLATION_GALILEO)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_GALILEO;
        constellationSum -= CONSTELLATION_GALILEO;
        le_utf8_Append(constellationStr, "GALILEO ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & CONSTELLATION_SBAS)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_SBAS;
        constellationSum -= CONSTELLATION_SBAS;
        le_utf8_Append(constellationStr, "SBAS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & CONSTELLATION_QZSS)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_QZSS;
        constellationSum -= CONSTELLATION_QZSS;
        le_utf8_Append(constellationStr, "QZSS ", sizeof(constellationStr), NULL);
    }
    if (constellationSum & CONSTELLATION_NAVIC)
    {
        constellationMask |= (uint32_t)LE_GNSS_CONSTELLATION_NAVIC;
        constellationSum -= CONSTELLATION_NAVIC;
        le_utf8_Append(constellationStr, "NAVIC ", sizeof(constellationStr), NULL);
    }
    le_utf8_Append(constellationStr, "]", sizeof(constellationStr), NULL);

    LE_INFO("Setting constellation %s",constellationStr);

    // Right now all constellation sum should be zero
    if (constellationSum != 0)
    {
        fprintf(stderr, "Bad constellation parameter: %s\n", constellationPtr);
        exit(EXIT_FAILURE);
    }

    le_result_t result =
       le_gnss_SetConstellation((le_gnss_ConstellationBitMask_t)constellationMask);

    switch(result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_UNSUPPORTED:
            printf("Setting constellation %s is not supported\n", constellationStr);
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not initialized, disabled. See logs for  \
                    details\n");
            break;
        case LE_FAULT:
            printf("Failed!\n");
            break;
        default:
            printf("Bad return value: %d\n", result);
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if 0
//-------------------------------------------------------------------------------------------------
/**
 * This function sets the area for a given constellation
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int SetConstellationArea
(
    const char* constellationPtr,      ///< [IN] GNSS constellation used in solution
    const char* constellationAreaPtr   ///< [IN] GNSS constellation area
)
{
    char *endPtr;
    errno = 0;
    int constellation = strtoul(constellationPtr, &endPtr, BASE10);
    int constArea = strtoul(constellationAreaPtr, &endPtr, BASE10);

    if (('\0' != endPtr[0]) || (0 != errno) || (0 == constellation) || (0 == constArea))
    {
        fprintf(stderr, "Bad constellation or area parameter: %s %s\n", constellationPtr,
                                                                        constellationAreaPtr);
        exit(EXIT_FAILURE);
    }

    le_result_t result = le_gnss_SetConstellationArea((le_gnss_Constellation_t)constellation,
                                                      (le_gnss_ConstellationArea_t) constArea);
    switch(result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_UNSUPPORTED:
            printf("Setting area %d for constellation %d is not supported\n",
                   constArea, constellation);
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not initialized, disabled or active. See logs for  \
                    details\n");
            break;
        case LE_FAULT:
            printf("Failed!\n");
            break;
        case LE_BAD_PARAMETER:
            printf("Invalid area\n");
            break;
        default:
            printf("Bad return value: %d\n", result);
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif

#if 0
//-------------------------------------------------------------------------------------------------
/**
 * This function sets agps mode of gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int SetAgpsMode
(
    const char* agpsModePtr    ///< [IN] agps to set
)
{
    le_gnss_AssistedMode_t suplAgpsMode;

    if (strcmp(agpsModePtr, "alone") == 0)
    {
        suplAgpsMode = LE_GNSS_STANDALONE_MODE;
    }
    else if (strcmp(agpsModePtr,"msBase") == 0)
    {
        suplAgpsMode = LE_GNSS_MS_BASED_MODE;
    }
    else if (strcmp(agpsModePtr,"msAssist") == 0)
    {
        suplAgpsMode = LE_GNSS_MS_ASSISTED_MODE;
    }
    else
    {
        printf("Bad agps mode: %s\n", agpsModePtr);
        return EXIT_FAILURE;
    }

    le_result_t result = le_gnss_SetSuplAssistedMode(suplAgpsMode);

    switch(result)
    {
        case LE_OK:
            printf("Success!\n");
            break;
        case LE_UNSUPPORTED:
            printf("The request is not supported\n");
            break;
        case LE_TIMEOUT:
            printf("Timeout error\n");
            break;
        case LE_FAULT:
            printf("Failed!\n");
            break;
        default:
            printf("Bad return value: %d\n", result);
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
//-------------------------------------------------------------------------------------------------
/**
 * This function sets the enabled NMEA sentences.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int SetNmeaSentences
(
    const char* nmeaMaskStr     ///< [IN] Enabled NMEA sentences bit mask
)
{
    int nmeaMask = le_hex_HexaToInteger(nmeaMaskStr);


    le_result_t result = le_gnss_SetNmeaSentences(nmeaMask);

    switch (result)
    {
        case LE_OK:
            printf("Successfully set enabled NMEA sentences!\n");
            break;
        case LE_FAULT:
            printf("Failed to set enabled NMEA sentences. See logs for details\n");
            break;
        case LE_BAD_PARAMETER:
            printf("Failed to set enabled NMEA sentences, incompatible bit mask\n");
            break;
        case LE_BUSY:
            printf("Failed to set enabled NMEA sentences, service is busy\n");
            break;
        case LE_TIMEOUT:
            printf("Failed to set enabled NMEA sentences, timeout error\n");
            break;
       case LE_NOT_PERMITTED:
            printf("GNSS is not in ready state!\n");
            break;
        default:
            printf("Failed to set enabled NMEA sentences, error %d (%s)\n",
                    result, LE_RESULT_TXT(result));
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets TTFF (Time to First Fix) value.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetTtff
(
    le_gnss_State_t state    ///< [IN] GNSS state
)
{
    uint32_t ttff;
    le_result_t result;

    if (LE_GNSS_STATE_ACTIVE != state)
    {
        printf("GNSS is not in active state!\n");
        return EXIT_FAILURE;
    }

    result = le_gnss_GetTtff(&ttff);
    switch (result)
    {
        case LE_OK:
            printf("TTFF(Time to First Fix) = %ums\n", ttff);
            break;
        case LE_BUSY:
            printf("TTFF not calculated (Position not fixed)\n");
            break;
        case LE_NOT_PERMITTED:
            printf("The GNSS device is not started or disabled. See logs for details\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#if 0
//-------------------------------------------------------------------------------------------------
/**
 * This function gets the agps mode.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetAgpsMode
(
    void
)
{
    le_gnss_AssistedMode_t assistedMode;
    le_result_t result = le_gnss_GetSuplAssistedMode(&assistedMode);

    if (result == LE_OK)
    {
        switch (assistedMode)
        {
            case LE_GNSS_STANDALONE_MODE:
                printf("AGPS mode: Standalone\n");
                break;
            case LE_GNSS_MS_BASED_MODE:
                printf("AGPS mode: MS-based\n");
                break;
            case LE_GNSS_MS_ASSISTED_MODE:
                printf("AGPS mode: MS-assisted\n");
                break;
        }
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif

//-------------------------------------------------------------------------------------------------
/**
 * This function gets constellation of gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetConstellation
(
    void
)
{
    le_gnss_ConstellationBitMask_t constellationMask;
    le_result_t result = le_gnss_GetConstellation(&constellationMask);

    if (result == LE_OK)
    {
        printf("ConstellationType %d\n", constellationMask);

        (constellationMask & LE_GNSS_CONSTELLATION_GPS)     ? printf("***GPS activated***\n") :
                                                              printf("GPS not activated\n");
        (constellationMask & LE_GNSS_CONSTELLATION_GLONASS) ? printf("***GLONASS activated***\n") :
                                                              printf("GLONASS not activated\n");
        (constellationMask & LE_GNSS_CONSTELLATION_BEIDOU)  ? printf("***BEIDOU activated***\n") :
                                                              printf("BEIDOU not activated\n");
        (constellationMask & LE_GNSS_CONSTELLATION_GALILEO) ? printf("***GALILEO activated***\n") :
                                                              printf("GALILEO not activated\n");
        (constellationMask & LE_GNSS_CONSTELLATION_SBAS)    ? printf("***SBAS activated***\n") :
                                                              printf("SBAS not activated\n");
        (constellationMask & LE_GNSS_CONSTELLATION_QZSS)    ? printf("***QZSS activated***\n") :
                                                              printf("QZSS not activated\n");
        (constellationMask & LE_GNSS_CONSTELLATION_NAVIC)   ? printf("***NAVIC activated***\n") :
                                                              printf("NAVIC not activated\n");
    }
    else if(result == LE_NOT_PERMITTED)
    {
        printf("GNSS is not in ready or active state!\n");
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets secondary band constellations.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int RequestSecondaryBandConstellations
(
    void
)
{
    uint32_t secondaryBandMask = 0;
    le_result_t result = le_gnss_RequestSecondaryBandConstellations(&secondaryBandMask);

    if (result == LE_OK)
    {
        printf("secondary band constellation %d\n", secondaryBandMask);
        if(secondaryBandMask & (1<<(LE_GNSS_SB_CONSTELLATION_GPS-1)))
        {
            printf("GPS constellation is disabled \n");
        }
        if(secondaryBandMask & (1<<(LE_GNSS_SB_CONSTELLATION_GALILEO-1)))
        {
            printf("GALILEO constellation is disabled \n");
        }
        if(secondaryBandMask & (1<<(LE_GNSS_SB_CONSTELLATION_SBAS-1)))
        {
            printf("SBAS constellation is disabled \n");
        }
        if(secondaryBandMask & (1<<(LE_GNSS_SB_CONSTELLATION_GLONASS-1)))
        {
            printf("GLONASS constellation is disabled \n");
        }
        if(secondaryBandMask &(1<<(LE_GNSS_SB_CONSTELLATION_BDS-1)))
        {
            printf("BDS constellation is disabled \n");
        }
        if(secondaryBandMask & (1<<(LE_GNSS_SB_CONSTELLATION_QZSS-1)))
        {
            printf("QZAS constellation is disabled \n");
        }
        if(secondaryBandMask &(1<<(LE_GNSS_SB_CONSTELLATION_NAVIC-1)))
        {
            printf("NAVIC constellation is disabled \n");
        }
    }
    else if(result == LE_NOT_PERMITTED)
    {
        printf("GNSS is not in ready state!\n");
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets Robust Location Information.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int RobustLocationInformation
(
    void
)
{
    uint8_t enable;
    uint8_t enabled911;
    uint8_t majorVersion;
    uint8_t minorVersion;
    le_result_t result = le_gnss_RobustLocationInformation(&enable,&enabled911,
                             &majorVersion,&minorVersion);

    if (result == LE_OK)
    {
        printf("Robust Location Information Enable: %d\n", enable);
        printf("Robust Location Information enabled911: %d\n", enabled911);
        printf("Robust Location Information majorVersion number: %d\n", majorVersion);
        printf("Robust Location Information minorVersion number: %d\n", minorVersion);
    }
    else if(result == LE_NOT_PERMITTED)
    {
        printf("GNSS is not in ready/active state!\n");
    }
    else
    {
        printf("Failed to get robust location information\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if 0
//-------------------------------------------------------------------------------------------------
/**
 * This function gets the area of each constellation of gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetConstellationArea
(
    void
)
{
    le_gnss_ConstellationArea_t constellationArea;
    le_gnss_Constellation_t constType = LE_GNSS_SV_CONSTELLATION_GPS;
    le_result_t result;
    static const char *tabConstellation[] =
    {
        "UNDEFINED CONSTELLATION",
        "GPS CONSTELLATION",
        "SBAS CONSTELLATION",
        "GLONASS CONSTELLATION ",
        "GALILEO CONSTELLATION",
        "BEIDOU CONSTELLATION",
        "QZSS CONSTELLATION",
    };

    do
    {
        result = le_gnss_GetConstellationArea(constType, &constellationArea);
        if (LE_OK == result)
        {
            printf("%s area %d\n", tabConstellation[constType], constellationArea);
        }
        else if (LE_UNSUPPORTED == result)
        {
            printf("%s unsupported area\n", tabConstellation[constType]);
        }
        else
        {
            printf("Failed! See log for details!\n");
            return EXIT_FAILURE;
        }
        constType++;
    }
    while (LE_GNSS_SV_CONSTELLATION_MAX != constType);

    return EXIT_SUCCESS;
}

#endif
//-------------------------------------------------------------------------------------------------
/**
 * This function gets gnss device acquisition rate.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetAcquisitionRate
(
    void
)
{
    uint32_t acqRate;
    le_result_t result = le_gnss_GetAcquisitionRate(&acqRate);

    switch (result)
    {
        case LE_OK:
            printf("Acquisition Rate: %ums\n", acqRate);
            break;
        case LE_FAULT:
            printf("Failed to get acquisition rate. See logs for details\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in \"active or ready\" state\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets the GNSS minimum elevation.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetMinElevation
(
    void
)
{
    uint8_t  minElevation;
    le_result_t result = le_gnss_GetMinElevation(&minElevation);

    switch (result)
    {
        case LE_OK:
            printf("Minimum elevation: %d\n", minElevation);
            break;
        case LE_FAULT:
            printf("Failed to get the minimum elevation. See logs for details\n");
            break;
        case LE_UNSUPPORTED:
            printf("Request not supported\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS device is not in \"Ready\" state\n");
            break;
        default:
            printf("Invalid status\n");
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets the enabled NMEA sentences.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetNmeaSentences
(
    void
)
{
    le_gnss_NmeaBitMask_t nmeaMask;
    le_result_t result = le_gnss_GetNmeaSentences(&nmeaMask);

    switch (result)
    {
        case LE_OK:
            printf("Enabled NMEA sentences bit mask = 0x%08X\n", nmeaMask);
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGGA)
            {
                printf("\tGPGGA (GPS fix data) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGSA)
            {
                printf("\tGPGSA (GPS DOP and active satellites) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGSV)
            {
                printf("\tGPGSV (GPS satellites in view) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPRMC)
            {
                printf("\tGPRMC (GPS recommended minimum data) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPVTG)
            {
                printf("\tGPVTG (GPS vector track and speed over the ground) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGNS)
            {
                printf("\tGPGNS enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GLGSV)
            {
                printf("\tGLGSV (GLONASS satellites in view) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GNGNS)
            {
                printf("\tGNGNS (GNSS fix data) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GNGSA)
            {
                printf("\tGNGSA (GNSS DOP and active satellites) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GAGGA)
            {
                printf("\tGAGGA (Galileo fix data) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GAGSA)
            {
                printf("\tGAGSA (Galileo DOP and active satellites) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GAGSV)
            {
                printf("\tGAGSV (Galileo satellites in view) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GQGSV)
            {
                printf("\tGQGSV enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GBGSV)
            {
                printf("\tGBGSV enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GIGSV)
            {
                printf("\tNMEA_MASK_GIGSV enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GARMC)
            {
                printf("\tGARMC (Galileo recommended minimum data) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GAVTG)
            {
                printf("\tGAVTG (Galileo vector track and speed over the ground) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_PSTIS)
            {
                printf("\tPSTIS (GPS session start indication) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_PQXFI)
            {
                printf("\tPQXFI (Proprietary Qualcomm eXtended Fix Information) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_PTYPE)
            {
                printf("\tPTYPE (Proprietary Type mask) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGRS)
            {
                printf("\tGPGRS (GPS Range residuals) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGLL)
            {
                printf("\tGPGLL (GPS Geographic position, latitude / longitude) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_DEBUG)
            {
               printf("\tDEBUG (Debug NMEA indication) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPDTM)
            {
               printf("\tGPDTM (Local geodetic datum and datum offset from a reference) enabled\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GAGNS)
            {
               printf("\tGAGNS (Fix data for Galileo) enabled\n");
            }
            break;
        case LE_FAULT:
            printf("Failed to get enabled NMEA sentences. See logs for details\n");
            break;
        case LE_BUSY:
            printf("Failed to get enabled NMEA sentences, service is busy\n");
            break;
        case LE_TIMEOUT:
            printf("Failed to get enabled NMEA sentences, timeout error\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS is not in active or ready state!\n");
            break;
        default:
            printf("Failed to get enabled NMEA sentences, error %d (%s)\n",
                    result, LE_RESULT_TXT(result));
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets the Supported NMEA sentences.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetSupportedNmeaSentences
(
    void
)
{
    le_gnss_NmeaBitMask_t nmeaMask;
    le_result_t result = le_gnss_GetSupportedNmeaSentences(&nmeaMask);

    switch (result)
    {
        case LE_OK:
            printf("Supported NMEA sentences bit mask = 0x%08X\n", nmeaMask);
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGGA)
            {
                printf("\tGPGGA (GPS fix data) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPRMC)
            {
                printf("\tGPRMC (GPS recommended minimum data) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GNGSA)
            {
                printf("\tGNGSA (GNSS DOP and active satellites) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPVTG)
            {
                printf("\tGPVTG (GPS vector track and speed over the ground) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGNS)
            {
                printf("\tGPGNS Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPDTM)
            {
               printf("\tGPDTM (Local geodetic datum and datum offset from a reference) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GPGSV)
            {
                printf("\tGPGSV (GPS satellites in view) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GLGSV)
            {
                printf("\tGLGSV (GLONASS satellites in view) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GAGSV)
            {
                printf("\tGAGSV (Galileo satellites in view) Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GQGSV)
            {
                printf("\tGQGSV Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GBGSV)
            {
                printf("\tGBGSV Supported\n");
            }
            if (nmeaMask & LE_GNSS_NMEA_MASK_GIGSV)
            {
                printf("\tGIGSV Supported\n");
            }
            break;
        case LE_FAULT:
            printf("Failed to get Supported NMEA sentences. See logs for details\n");
            break;
        case LE_BUSY:
            printf("Failed to get Supported NMEA sentences, service is busy\n");
            break;
        case LE_TIMEOUT:
            printf("Failed to get Supported NMEA sentences, timeout error\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS is not in Ready state!\n");
            break;
        default:
            printf("Failed to get Supported NMEA sentences, error %d (%s)\n",
                    result, LE_RESULT_TXT(result));
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets the Supported Constellations
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetSupportedConstellations
(
    void
)
{
    le_gnss_ConstellationBitMask_t constMask;
    le_result_t result = le_gnss_GetSupportedConstellations(&constMask);

    switch (result)
    {
        case LE_OK:
            printf("Supported Constellations bit mask = 0x%08X\n", constMask);
            if (constMask & LE_GNSS_CONSTELLATION_GLONASS)
            {
                printf("\tGLONASS is Supported\n");
            }
            if (constMask & LE_GNSS_CONSTELLATION_BEIDOU)
            {
                printf("\tBEDIDOU is Supported\n");
            }
            if (constMask & LE_GNSS_CONSTELLATION_GALILEO)
            {
                printf("\tGALILEO is Supported\n");
            }
            if (constMask & LE_GNSS_CONSTELLATION_SBAS)
            {
                printf("\tSBAS is Supported\n");
            }
            if (constMask & LE_GNSS_CONSTELLATION_QZSS)
            {
                printf("\tQZSS is Supported\n");
            }
            break;
        case LE_FAULT:
            printf("Failed to get Supported Constellations. See logs for details\n");
            break;
        case LE_BUSY:
            printf("Failed to get Supported Constellations, service is busy\n");
            break;
        case LE_TIMEOUT:
            printf("Failed to get Supported Constellations, timeout error\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS is not in ready state!\n");
            break;
        default:
            printf("Failed to get Supported Constellations, error %d (%s)\n",
                    result, LE_RESULT_TXT(result));
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function configures default second band constellations
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int DefaultSecondaryBandConstellations
(
    void
)
{
    le_result_t result = le_gnss_DefaultSecondaryBandConstellations();

    switch (result)
    {
        case LE_OK:
                printf("Succesfully configured default second band constellations");
            break;
        case LE_FAULT:
            printf("Failed to configure secondary band constellations\n");
            break;
        case LE_NOT_PERMITTED:
            printf("GNSS is not in ready state!\n");
            break;
        default:
            printf("Failure error %d (%s)\n",
                    result, LE_RESULT_TXT(result));
            break;
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets position fix for last updated sample.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetPosState
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    le_gnss_FixState_t state;
    le_result_t result = le_gnss_GetPositionState( positionSampleRef,
                                                   &state);
    if (result == LE_OK)
    {
        printf("Position state: %s\n", (state == LE_GNSS_STATE_FIX_NO_POS)?"No Fix"
                                     : (state == LE_GNSS_STATE_FIX_2D)?"2D Fix"
                                     : (state == LE_GNSS_STATE_FIX_3D)?"3D Fix"
                                     : (state == LE_GNSS_STATE_FIX_ESTIMATED)?"Estimated Fix"
                                     : "Invalid");
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets latitude, longitude and horizontal accuracy of last updated location.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int Get2Dlocation
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    int32_t     latitude;
    int32_t     longitude;
    int32_t     hAccuracy;

    le_result_t result = le_gnss_GetLocation( positionSampleRef,
                                              &latitude,
                                              &longitude,
                                              &hAccuracy);

    if (result == LE_OK)
    {
        printf("Latitude(positive->north) : %.6f\n"
               "Longitude(positive->east) : %.6f\n"
               "hAccuracy                 : %.2fm\n",
                (float)latitude/1e6,
                (float)longitude/1e6,
                (float)hAccuracy/1e2);
    }
    else if(result == LE_OUT_OF_RANGE)
    {
        printf("Location invalid [%d, %d, %d]\n",
               latitude,
               longitude,
               hAccuracy);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets latitude and vertical accuracy of last updated location.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetAltitude
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    int32_t altitude;
    int32_t vAccuracy;

    le_result_t result = le_gnss_GetAltitude( positionSampleRef,
                                              &altitude,
                                              &vAccuracy);

    if(result == LE_OK)
    {
        printf("Altitude  : %.3fm\n"
               "vAccuracy : %.1fm\n",
               (float)altitude/1e3,
               (float)vAccuracy/10.0);
    }
    else if (result == LE_OUT_OF_RANGE)
    {
        printf("Altitude invalid [%d, %d]\n",
               altitude,
               vAccuracy);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets the altitude with respect to the WGS-84 ellipsoid of last updated location.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
#if 0
static int GetAltitudeOnWgs84
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    int32_t altitudeOnWgs84;

    le_result_t result = le_gnss_GetAltitudeOnWgs84(positionSampleRef, &altitudeOnWgs84);

    if (LE_OK == result)
    {
        printf("AltitudeOnWgs84  : %.3fm\n", (float)altitudeOnWgs84/1e3);
    }
    else if (LE_OUT_OF_RANGE == result)
    {
        printf("AltitudeOnWgs84 invalid [%d]\n", altitudeOnWgs84);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif

//-------------------------------------------------------------------------------------------------
/**
 * This function gets gps time of last updated sample.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetGpsTime
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint32_t gpsWeek;
    uint32_t gpsTimeOfWeek;

    le_result_t result = le_gnss_GetGpsTime( positionSampleRef,
                                             &gpsWeek,
                                             &gpsTimeOfWeek);

    if (result == LE_OK)
    {
        printf("GPS time, Week %02d:TimeOfWeek %d ms\n",
                gpsWeek,
                gpsTimeOfWeek);
    }
    else if (result == LE_OUT_OF_RANGE)
    {
        printf("GPS time invalid [%d, %d]\n",
                gpsWeek,
                gpsTimeOfWeek);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets time of last updated location.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetTime
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint16_t    hours = 0;
    uint16_t    minutes = 0;
    uint16_t    seconds = 0;
    uint16_t    milliseconds = 0;

    le_result_t result = le_gnss_GetTime( positionSampleRef,
                                          &hours,
                                          &minutes,
                                          &seconds,
                                          &milliseconds);

    if (result == LE_OK)
    {
        printf("Time(HH:MM:SS:MS) %02d:%02d:%02d:%03d\n",
                hours,
                minutes,
                seconds,
                milliseconds);
    }
    else if(result == LE_OUT_OF_RANGE)
    {
        printf("Time invalid %02d:%02d:%02d.%03d\n",
                hours,
                minutes,
                seconds,
                milliseconds);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets Epoch time of last updated location.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetEpochTime
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint64_t epochTime;          ///< Epoch time in milliseconds since Jan. 1, 1970

    le_result_t result = le_gnss_GetEpochTime( positionSampleRef, &epochTime);

    if (LE_OK == result)
    {
        printf("Epoch Time %llu ms\n", (unsigned long long int) epochTime);
    }
    else if (LE_OUT_OF_RANGE == result)
    {
        printf("Time invalid %llu ms\n", (unsigned long long int) epochTime);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    int status = (result == LE_OK) ? EXIT_SUCCESS: EXIT_FAILURE;
    return status;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets time accuracy of last updated sample.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetTimeAccuracy
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint32_t timeAccuracy;
    le_result_t result = le_gnss_GetTimeAccuracy( positionSampleRef,
                                                  &timeAccuracy);

    if (result == LE_OK)
    {
        printf("GPS time accuracy %dms\n", timeAccuracy);
    }
    else if (result == LE_OUT_OF_RANGE)
    {
        printf("GPS time accuracy invalid [%d]\n", timeAccuracy);
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets current GPS time, leap seconds, next leap seconds event time and next leap
 * seconds value.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetLeapSeconds
(
    void
)
{
    int32_t currentLeapSec, nextLeapSec;
    uint64_t gpsTimeMs, nextEventMs;
    le_result_t result;

    result = le_gnss_GetLeapSeconds(&gpsTimeMs, &currentLeapSec, &nextEventMs,&nextLeapSec);

    if (LE_OK == result)
    {
        printf("Leap seconds report:\n");

        printf("\tCurrent GPS time: ");
        if (gpsTimeMs != UINT64_MAX)
        {
            printf("%"PRIu64" ms\n", gpsTimeMs);
        }
        else
        {
            printf("\n");
        }

        printf("\tLeap seconds: ");
        if (currentLeapSec != INT32_MAX)
        {
            printf("%"PRIi32" ms\n",currentLeapSec);
        }
        else
        {
            printf("\n");
        }

        printf("\tNext event in: ");
        if (nextEventMs != UINT64_MAX)
        {
            printf("%"PRIu64" ms\n", nextEventMs);
        }
        else
        {
            printf("\n");
        }

        printf("\tNext leap seconds in: ");
        if (nextLeapSec != INT32_MAX)
        {
            printf("%"PRIi32" ms\n",nextLeapSec);
        }
        else
        {
            printf("\n");
        }
    }
    else if (LE_TIMEOUT == result)
    {
        printf("Timeout for getting next leap second event.\n");
    }
    else if (LE_UNSUPPORTED == result)
    {
       printf("Not Supported !\n");
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets position sample's UTC leap seconds in advance
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetGpsLeapSeconds
(
   le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint8_t leapSecondsPtr;
    le_result_t result;
    result = le_gnss_GetGpsLeapSeconds(positionSampleRef, &leapSecondsPtr);
    if (LE_OK == result)
    {
        printf("Gps Leap seconds : ");
        if (leapSecondsPtr != UINT8_MAX)
        {
            printf("%"PRIu8"s\n", leapSecondsPtr);
        }
        else
        {
            printf("\n");
        }
    }
    else if(LE_TIMEOUT == result)
    {
        printf("Timeout for getting Gps leap seconds\n");
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets position he position sample's magnetic deviation
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetMagneticDeviation
(
   le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    int32_t magneticDeviationPtr ;
    le_result_t result;
    result = le_gnss_GetMagneticDeviation(positionSampleRef, &magneticDeviationPtr);
    if (LE_OK == result)
    {
        printf("Magnetic Deviation : ");
        if (magneticDeviationPtr != INT32_MAX)
        {
            printf("%.1f degrees\n", (float)(magneticDeviationPtr/10.0));
        }
        else
        {
            printf("\n");
        }
    }
    else if(LE_TIMEOUT == result)
    {
        printf("Timeout for getting Magnetic Deviation\n");
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int GetEllipticalUncertainty
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint32_t horUncEllipseSemiMajorPtr;
    uint32_t horUncEllipseSemiMinorPtr;
    uint8_t  horConfidencePtr;

    le_result_t result = le_gnss_GetEllipticalUncertainty( positionSampleRef,
                                                     &horUncEllipseSemiMajorPtr,
                                                     &horUncEllipseSemiMinorPtr,
                                                     &horConfidencePtr);
    if (result == LE_OK)
    {
        printf("HorizontalUncertainty SemiMajor: %.2fm/s\n",(float)horUncEllipseSemiMajorPtr);
        printf("HorizontalUncertainty SemiMinor: %.2fm/s\n",(float)horUncEllipseSemiMinorPtr);
        printf("Horizontal Confidence level: %d%%\n",horConfidencePtr);
    }
    else if (result == LE_OUT_OF_RANGE)
    {
        printf("HorizontalUncertainty invalid [%u, %u %u]\n",
                horUncEllipseSemiMajorPtr,
                horUncEllipseSemiMinorPtr,
                horConfidencePtr);
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}
//-------------------------------------------------------------------------------------------------
/**
 * This function gets the date of updated location.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetDate
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint16_t    year = 0;
    uint16_t    month = 0;
    uint16_t    day = 0;

    le_result_t result = le_gnss_GetDate( positionSampleRef,
                                         &year,
                                         &month,
                                         &day);

    if (result == LE_OK)
    {
        printf("Date(YYYY-MM-DD) %04d-%02d-%02d\n",
                year,
                month,
                day);

    }
    else if(result == LE_OUT_OF_RANGE)
    {
        printf("Date invalid %04d-%02d-%02d\n",
                year,
                month,
                day);
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets horizontal speed and its accuracy of last updated sample.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetHorizontalSpeed
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint32_t hSpeed;
    uint32_t hSpeedAccuracy;

    le_result_t result = le_gnss_GetHorizontalSpeed( positionSampleRef,
                                                     &hSpeed,
                                                     &hSpeedAccuracy);
    if (result == LE_OK)
    {
        printf("hSpeed %.2fm/s\n"
               "Accuracy %.1fm/s\n",
                hSpeed/100.0,
                hSpeedAccuracy/10.0);
    }
    else if (result == LE_OUT_OF_RANGE)
    {
        printf("hSpeed invalid [%u, %u]\n",
                hSpeed,
                hSpeedAccuracy);
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets vertical speed and its accuracy of last updated sample.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetVerticalSpeed
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    int32_t vSpeed;
    int32_t vSpeedAccuracy;

    le_result_t result = le_gnss_GetVerticalSpeed( positionSampleRef,
                                                   &vSpeed,
                                                   &vSpeedAccuracy);
    if (result == LE_OK)
    {
        printf( "vSpeed %.2fm/s\n"
                "Accuracy %.1fm/s\n",
                vSpeed/100.0,
                vSpeedAccuracy/10.0);
    }
    else if (result == LE_OUT_OF_RANGE)
    {
        printf("vSpeed invalid [%d, %d]\n",
                vSpeed,
                vSpeedAccuracy);
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets direction of gnss device.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetDirection
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint32_t direction         = 0;
    uint32_t directionAccuracy = 0;

    le_result_t result = le_gnss_GetDirection(positionSampleRef,
                                              &direction,
                                              &directionAccuracy);

    if (result == LE_OK)
    {
        printf("Direction(0 degree is True North) : %.1f degrees\n"
               "Accuracy                          : %.1f degrees\n",
                (float)direction/10.0,
                (float)directionAccuracy/10.0);
    }
    else if(result == LE_OUT_OF_RANGE)
    {
        printf("Direction invalid [%u, %u]\n",
               direction,
               directionAccuracy);
    }
    else
    {
        printf("Failed! See log for details\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets the DOP (Dilution of Precision).
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetDop
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint16_t dop[LE_GNSS_RES_UNKNOWN] = { 0 };
    bool err = false;
    le_result_t result = LE_FAULT;
    le_gnss_DopType_t dopType = LE_GNSS_PDOP;
    le_gnss_Resolution_t DopRes;

    static const char *tabDop[] =
    {
        "Position dilution of precision (PDOP)",
        "Horizontal dilution of precision (HDOP)",
        "Vertical dilution of precision (VDOP)",
        "Geometric dilution of precision (GDOP)",
        "Time dilution of precision (TDOP)"
    };

    do
    {
        // Get DOP parameter in all resolutions
        for (DopRes=LE_GNSS_RES_ZERO_DECIMAL; DopRes<LE_GNSS_RES_UNKNOWN; DopRes++)
        {
            if (LE_OK != le_gnss_SetDopResolution(DopRes))
            {
                printf("Failed! See log for details!\n");
                return EXIT_FAILURE;
            }

            result = le_gnss_GetDilutionOfPrecision(positionSampleRef,
                                                    dopType,
                                                    &dop[DopRes]);
            if (LE_OUT_OF_RANGE == result)
            {
                printf("%s invalid %d\n", tabDop[dopType], dop[0]);
                err = true;
                break;
            }
            else if (LE_OK != result)
            {
                printf("Failed! See log for details!\n");
                return EXIT_FAILURE;
            }
        }
        if (LE_OK == result)
        {
            printf("%s [%.1f %.1f %.2f %.3f]\n", tabDop[dopType],
                   (float)dop[LE_GNSS_RES_ZERO_DECIMAL],
                   (float)dop[LE_GNSS_RES_ONE_DECIMAL]/10,
                   (float)dop[LE_GNSS_RES_TWO_DECIMAL]/100,
                   (float)dop[LE_GNSS_RES_THREE_DECIMAL]/1000);
        }
        dopType++;
    }
    while (dopType != LE_GNSS_DOP_LAST);

    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
/**
 * This function gets the Satellites Vehicle information.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetSatelliteInfo
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    // Satellites information
    uint16_t satIdPtr[LE_GNSS_SV_INFO_MAX_LEN];
    size_t satIdNumElements = NUM_ARRAY_MEMBERS(satIdPtr);
    le_gnss_Constellation_t satConstPtr[LE_GNSS_SV_INFO_MAX_LEN];
    size_t satConstNumElements = NUM_ARRAY_MEMBERS(satConstPtr);
    bool satUsedPtr[LE_GNSS_SV_INFO_MAX_LEN];
    size_t satUsedNumElements = NUM_ARRAY_MEMBERS(satUsedPtr);
    uint8_t satSnrPtr[LE_GNSS_SV_INFO_MAX_LEN];
    size_t satSnrNumElements = NUM_ARRAY_MEMBERS(satSnrPtr);
    uint16_t satAzimPtr[LE_GNSS_SV_INFO_MAX_LEN];
    size_t satAzimNumElements = NUM_ARRAY_MEMBERS(satAzimPtr);
    uint8_t satElevPtr[LE_GNSS_SV_INFO_MAX_LEN];
    size_t satElevNumElements = NUM_ARRAY_MEMBERS(satElevPtr);
    int i;

    le_result_t result =  le_gnss_GetSatellitesInfo( positionSampleRef,
                                                     satIdPtr,
                                                     &satIdNumElements,
                                                     satConstPtr,
                                                     &satConstNumElements,
                                                     satUsedPtr,
                                                     &satUsedNumElements,
                                                     satSnrPtr,
                                                     &satSnrNumElements,
                                                     satAzimPtr,
                                                     &satAzimNumElements,
                                                     satElevPtr,
                                                     &satElevNumElements);

    if((result == LE_OK)||(result == LE_OUT_OF_RANGE))
    {
        // Satellite Vehicle information
        for(i=0; i<satIdNumElements; i++)
        {
            if((satIdPtr[i] != 0)&&(satIdPtr[i] != UINT8_MAX))
            {
                printf("[%02d] SVid %03d - C%01d - U%d - SNR%02d - Azim%03d - Elev%02d\n"
                        , i
                        , satIdPtr[i]
                        , satConstPtr[i]
                        , satUsedPtr[i]
                        , satSnrPtr[i]
                        , satAzimPtr[i]
                        , satElevPtr[i]);

                if (LE_GNSS_SV_CONSTELLATION_SBAS == satConstPtr[i])
                {
                    //uncomment this, when feature is enabled
                    /*printf("SBAS category : %d\n",
                           le_gnss_GetSbasConstellationCategory(satIdPtr[i]));*/
                }
            }
        }
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function gets the Satellites Vehicle status.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetSatelliteStatus
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    uint8_t satsInViewCount;
    uint8_t satsTrackingCount;
    uint8_t satsUsedCount;
    le_result_t result =  le_gnss_GetSatellitesStatus( positionSampleRef,
                                                       &satsInViewCount,
                                                       &satsTrackingCount,
                                                       &satsUsedCount);

    LE_ASSERT((result == LE_OK)||(result == LE_OUT_OF_RANGE));

    if ((result == LE_OK) || (result == LE_OUT_OF_RANGE))
    {
        printf("satsInView %d - satsTracking %d - satsUsed %d\n",
               (satsInViewCount == UINT8_MAX) ? 0: satsInViewCount,
               (satsTrackingCount == UINT8_MAX) ? 0: satsTrackingCount,
               (satsUsedCount == UINT8_MAX) ? 0: satsUsedCount);
    }
    else
    {
        printf("Failed! See log for details!\n");
    }

    return (LE_OK == result) ? EXIT_SUCCESS : EXIT_FAILURE;
}


//-------------------------------------------------------------------------------------------------
/**
 * Function to get all positional information of last updated sample.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int GetPosInfo
(
    le_gnss_SampleRef_t positionSampleRef    ///< [IN] Position sample reference
)
{
    int status = EXIT_SUCCESS;
    status = (EXIT_FAILURE == GetTtff(le_gnss_GetState())) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetPosState(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == Get2Dlocation(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetAltitude(positionSampleRef)) ? EXIT_FAILURE : status;
    //status = (EXIT_FAILURE == GetAltitudeOnWgs84(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetGpsTime(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetTime(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetEpochTime(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetTimeAccuracy(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetDate(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetDop(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetHorizontalSpeed(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetVerticalSpeed(positionSampleRef)) ? EXIT_FAILURE : status;
    status = (EXIT_FAILURE == GetDirection(positionSampleRef)) ? EXIT_FAILURE : status;
    return status;
}


//-------------------------------------------------------------------------------------------------
/**
 * Function to do first position fix.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//-------------------------------------------------------------------------------------------------
static int DoPosFix
(
    uint32_t fixVal          ///< [IN] Position fix time in seconds
)
{
    uint32_t count = 0;
    le_result_t result = le_gnss_Start();

    if (result == LE_NOT_PERMITTED)
    {
        printf("GNSS was not enabled. Enabling it\n");
        if (le_gnss_Enable() != LE_OK)
        {
            fprintf(stderr, "Failed to enable GNSS. Try rebooting device. Exiting\n");
            return EXIT_FAILURE;
        }

        // Now start GNSS device
        if(le_gnss_Start() != LE_OK)
        {
            fprintf(stderr, "Failed to start GNSS. Try rebooting device. Exiting\n");
            return EXIT_FAILURE;
        }
    }
    else if(result == LE_FAULT)
    {
        fprintf(stderr, "Failed to start GNSS. Try rebooting device. Exiting\n");
        return EXIT_FAILURE;
    }

    result = LE_BUSY;

    while ((result == LE_BUSY) && (count < fixVal))
    {
        // Get TTFF
        uint32_t ttff;
        result = le_gnss_GetTtff(&ttff);

        if (result == LE_OK)
        {
            printf("TTFF start = %d msec\n", ttff);
            return EXIT_SUCCESS;
        }
        else if (result == LE_BUSY)
        {
            count++;
            printf("TTFF not calculated (Position not fixed)\n");
            le_thread_Sleep(1);
        }
        else
        {
            printf("Failed! See log for details\n");
            return EXIT_FAILURE;
        }
    }

    return EXIT_FAILURE;
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler function for Position Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void PositionHandlerFunction
(
    le_gnss_SampleRef_t positionSampleRef,    ///< [IN] Position sample reference
    void* contextPtr                          ///< [IN] The context pointer
)
{

    if (strcmp(ParamsName, "watch") == 0)
    {
        GetPosInfo(positionSampleRef);
        GetSatelliteStatus(positionSampleRef);
        GetSatelliteInfo(positionSampleRef);
        // Release provided Position sample reference
        le_gnss_ReleaseSampleRef(positionSampleRef);
    }
    else
    {
        int status = EXIT_FAILURE;

        if (strcmp(ParamsName, "posState") == 0)
        {
            status = GetPosState(positionSampleRef);
        }
        else if (strcmp(ParamsName, "loc2d") == 0)
        {
            status = Get2Dlocation(positionSampleRef);
        }
        else if (strcmp(ParamsName, "alt") == 0)
        {
            status = GetAltitude(positionSampleRef);
        }
        /*else if (0 == strcmp(ParamsName, "altOnWgs84"))
        {
            status = GetAltitudeOnWgs84(positionSampleRef);
        }*/
        else if (strcmp(ParamsName, "loc3d") == 0)
        {
            status = EXIT_SUCCESS;
            status = (Get2Dlocation(positionSampleRef) == EXIT_FAILURE) ? EXIT_FAILURE : status;
            status = (GetAltitude(positionSampleRef) == EXIT_FAILURE) ? EXIT_FAILURE : status;
        }
        else if (strcmp(ParamsName, "gpsTime") == 0)
        {
            status = GetGpsTime(positionSampleRef);
        }
        else if (strcmp(ParamsName, "time") == 0)
        {
            status = GetTime(positionSampleRef);
        }
        else if (strcmp(ParamsName, "epochTime") == 0)
        {
            status = GetEpochTime(positionSampleRef);
        }
        else if (strcmp(ParamsName, "timeAcc") == 0)
        {
            status = GetTimeAccuracy(positionSampleRef);
        }
        else if (strcmp(ParamsName, "date") == 0)
        {
            status = GetDate(positionSampleRef);
        }
        else if (strcmp(ParamsName, "hSpeed") == 0)
        {
            status = GetHorizontalSpeed(positionSampleRef);
        }
        else if (strcmp(ParamsName, "vSpeed") == 0)
        {
            status = GetVerticalSpeed(positionSampleRef);
        }
        else if (strcmp(ParamsName, "motion") == 0)
        {
            status = EXIT_SUCCESS;
            status = (GetHorizontalSpeed(positionSampleRef) == EXIT_FAILURE) ?
                                                                             EXIT_FAILURE : status;
            status = (GetVerticalSpeed(positionSampleRef) == EXIT_FAILURE) ? EXIT_FAILURE : status;
        }
        else if (strcmp(ParamsName, "direction") == 0)
        {
            status = GetDirection(positionSampleRef);
        }
        else if (strcmp(ParamsName, "satInfo") == 0)
        {
            status = GetSatelliteInfo(positionSampleRef);
        }
        else if (strcmp(ParamsName, "satStat") == 0)
        {
            status = GetSatelliteStatus(positionSampleRef);
        }
        else if (strcmp(ParamsName, "dop") == 0)
        {
            status = GetDop(positionSampleRef);
        }
        else if (strcmp(ParamsName, "posInfo") == 0)
        {
            status = GetPosInfo(positionSampleRef);
        }

        else if (strcmp(ParamsName, "GpsLeapSeconds") == 0)
        {
            status = GetGpsLeapSeconds(positionSampleRef);
        }
        else if (strcmp(ParamsName, "magDev") == 0)
        {
            status = GetMagneticDeviation(positionSampleRef);
        }
        else if (strcmp(ParamsName, "elliUnc") == 0)
        {
            status = GetEllipticalUncertainty(positionSampleRef);
        }
        le_gnss_ReleaseSampleRef(positionSampleRef);
        exit(status);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Thread to monitor all gnss information.
 *
*/
//--------------------------------------------------------------------------------------------------
static void* PositionThread
(
    void* contextPtr             ///< [IN] The context pointer
)
{
    le_gnss_ConnectService();

    PositionHandlerRef = le_gnss_AddPositionHandler(PositionHandlerFunction, NULL);
    LE_ASSERT((PositionHandlerRef != NULL));

    le_event_RunLoop();
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to enable gnss and monitor its information.
 *
 * @return
 *     - EXIT_SUCCESS on success.
 *     - EXIT_FAILURE on failure.
 */
//--------------------------------------------------------------------------------------------------
static int WatchGnssInfo
(
    uint32_t watchPeriod          ///< [IN] Watch period in seconds
)
{
    le_thread_Ref_t positionThreadRef;

    // Add Position Handler
    positionThreadRef = le_thread_Create("PositionThread",PositionThread,NULL);
    le_thread_Start(positionThreadRef);

    printf("Watch positioning data for %ds\n", watchPeriod);
    le_thread_Sleep(watchPeriod);

    le_gnss_RemovePositionHandler(PositionHandlerRef);

    // stop thread
    le_thread_Cancel(positionThreadRef);

    return EXIT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------
/**
 * This function prints the GNSS device status.
 */
//-------------------------------------------------------------------------------------------------
static int GetGnssDeviceStatus
(
void
)
{
    le_gnss_State_t state;
    const char *status;

    state = le_gnss_GetState();

    switch (state)
    {
        case LE_GNSS_STATE_UNINITIALIZED:
            status = "not initialized";
            break;
        case LE_GNSS_STATE_READY:
            status = "ready";
            break;
        case LE_GNSS_STATE_ACTIVE:
            status = "active";
            break;
        case LE_GNSS_STATE_DISABLED:
            status = "disabled";
            break;
        default:
            status = "unknown";
            break;
    }

    fprintf(stdout, "%s\n", status);

    return 0;
};

//-------------------------------------------------------------------------------------------------
/**
 * This function gets different gnss parameters.
 */
//-------------------------------------------------------------------------------------------------
static void GetGnssParams
(
    const char *params      ///< [IN] gnss parameters
)
{
    le_gnss_State_t state = le_gnss_GetState();

    if (0 == strcmp(params, "ttff"))
    {
        exit(GetTtff(state));
    }
    else if (0 == strcmp(params, "acqRate"))
    {
        exit(GetAcquisitionRate());
    }
    else if (0 == strcmp(params, "LeapSeconds"))
    {
        exit(GetLeapSeconds());
    }
    /*else if (0 == strcmp(params, "agpsMode"))
    {
        exit(GetAgpsMode());
    }*/
    else if (0 == strcmp(params, "constellation"))
    {
        exit(GetConstellation());
    }
    else if (0 == strcmp(params, "secondBandConst"))
    {
        exit(RequestSecondaryBandConstellations());
    }
    else if (0 == strcmp(params, "robustloc"))
    {
        exit(RobustLocationInformation());
    }
    #if 0
    else if (0 == strcmp(params, "constArea"))
    {
        exit(GetConstellationArea());
    }
    #endif
    else if (0 == strcmp(params, "nmeaSentences"))
    {
        exit(GetNmeaSentences());
    }
    else if (0 == strcmp(params, "minElevation"))
    {
        exit(GetMinElevation());
    }
    else if ((0 == strcmp(params, "posState"))    ||
             (0 == strcmp(params, "loc2d"))       ||
             (0 == strcmp(params, "alt"))         ||
             (0 == strcmp(params, "altOnWgs84"))  ||
             (0 == strcmp(params, "loc3d"))       ||
             (0 == strcmp(params, "gpsTime"))     ||
             (0 == strcmp(params, "time"))        ||
             (0 == strcmp(params, "epochTime"))   ||
             (0 == strcmp(params, "timeAcc"))     ||
             (0 == strcmp(params, "LeapSeconds")) ||
             (0 == strcmp(params, "date"))        ||
             (0 == strcmp(params, "hSpeed"))      ||
             (0 == strcmp(params, "vSpeed"))      ||
             (0 == strcmp(params, "motion"))      ||
             (0 == strcmp(params, "direction"))   ||
             (0 == strcmp(params, "satInfo"))     ||
             (0 == strcmp(params, "satStat"))     ||
             (0 == strcmp(params, "dop"))         ||
             (0 == strcmp(params, "magDev"))      ||
             (0 == strcmp(params, "elliUnc"))     ||
             (0 == strcmp(params,"GpsLeapSeconds"))||
             (0 == strcmp(params, "posInfo")))
    {
        if (LE_GNSS_STATE_ACTIVE != state)
        {
            printf("GNSS is not in active state!\n");
            exit(EXIT_FAILURE);
        }

        // Copy the param
        le_utf8_Copy(ParamsName, params, sizeof(ParamsName), NULL);

        PositionHandlerRef = le_gnss_AddPositionHandler(PositionHandlerFunction, NULL);
        LE_ASSERT((PositionHandlerRef != NULL));
    }
    else if (strcmp(params, "status") == 0)
    {
        exit(GetGnssDeviceStatus());
    }
    else
    {
        printf("Bad parameter: %s\n", params);
        exit(EXIT_FAILURE);
    }

}


//-------------------------------------------------------------------------------------------------
/**
 * This function sets different gnss parameters .
 */
//-------------------------------------------------------------------------------------------------
static int SetGnssParams
(
    const char * argNamePtr,    ///< [IN] gnss parameters
    const char * argValPtr,     ///< [IN] gnss parameters
    const char * arg2ValPtr     ///< [IN] gnss parameters
)
{
    int status = EXIT_FAILURE;

    if (strcmp(argNamePtr, "constellation") == 0)
    {
        status = SetConstellation(argValPtr);
    }
    #if 0
    else if (strcmp(argNamePtr, "constArea") == 0)
    {
        if (NULL == arg2ValPtr)
        {
            LE_ERROR("arg2ValPtr is NULL");
            exit(EXIT_FAILURE);
        }
        status = SetConstellationArea(argValPtr, arg2ValPtr);
    }
    #endif
    else if (strcmp(argNamePtr, "acqRate") == 0)
    {
        status = SetAcquisitionRate(argValPtr);
    }
    /*else if (strcmp(argNamePtr, "agpsMode") == 0)
    {
        status = SetAgpsMode(argValPtr);
    }*/
    else if (strcmp(argNamePtr, "nmeaSentences") == 0)
    {
        status = SetNmeaSentences(argValPtr);
    }
    else if (0 == strcmp(argNamePtr, "minElevation"))
    {
        status = SetMinElevation(argValPtr);
    }
    else if (0 == strcmp(argNamePtr, "startMode"))
    {
       status = StartMode(argValPtr);
    }
    else if (0 == strcmp(argNamePtr, "configEng"))
    {
        if (NULL == arg2ValPtr)
        {
            printf("arg2ValPtr is NULL");
            exit(EXIT_FAILURE);
        }
        status = ConfigureEngineState(argValPtr, arg2ValPtr);
    }
    else if (0 == strcmp(argNamePtr, "robustloc"))
    {
        if (NULL == arg2ValPtr)
        {
            printf("arg2ValPtr is NULL");
            exit(EXIT_FAILURE);
        }
        status = ConfigureRobustLocation(argValPtr,arg2ValPtr);
    }
    else if (0 == strcmp(argNamePtr, "secondBandConst"))
    {
        status = ConfigureSecondaryBandConstellations(argValPtr);
    }
    else
    {
        printf("Bad parameter request: %s\n", argNamePtr);
    }

    exit(status);
}

//--------------------------------------------------------------------------------------------------
/**
 * Verify if enough parameter passed into command.
 * If not, output error message and terminate the program.
 */
//--------------------------------------------------------------------------------------------------
void CheckEnoughParams
(
    size_t requiredParam,      ///< [IN] Required parameters for the command
    size_t numArgs,            ///< [IN] Number of arguments passed into the command line
    const char * errorMsgPtr   ///< [IN] Error message to output if not enough parameters
)
{
    if ( (requiredParam + 1) <= numArgs)
    {
        return;
    }
    else
    {
        printf("%s\nTry '%s help'\n", errorMsgPtr, le_arg_GetProgramName());
        exit(EXIT_FAILURE);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Program init
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Process the command
    if (le_arg_NumArgs() < 1)
    {
        // No argument specified. Print help and exit.
        PrintGnssHelp();
        exit(EXIT_FAILURE);
    }

    const char* commandPtr = le_arg_GetArg(0);
    if(NULL == commandPtr)
    {
        LE_ERROR("commandPtr is NULL");
        exit(EXIT_FAILURE);
    }
    size_t numArgs = le_arg_NumArgs();

    if (strcmp(commandPtr, "help") == 0)
    {
        PrintGnssHelp();
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(commandPtr, "start") == 0)
    {
        exit(Start());
    }
    else if (0 == strcmp(commandPtr, "startType"))
    {
        const char* startTypePtr = le_arg_GetArg(1);
        if (startTypePtr == NULL)
        {
            printf("start type is NULL.\n");
            exit(EXIT_FAILURE);
        }
        exit(StartEngineType(startTypePtr));
    }
    else if (0 == strcmp(commandPtr, "ConfigLevArm"))
    {
        const char* forwOffsetPtr = le_arg_GetArg(1);
        if (forwOffsetPtr == NULL)
        {
            printf("forwOffsetPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* sideOffsetPtr = le_arg_GetArg(2);
        if (sideOffsetPtr == NULL)
        {
            printf("sideOffsetPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* upOffsePtr = le_arg_GetArg(3);
        if (upOffsePtr == NULL)
        {
            printf("upOffsePtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* levArmTypePtr = le_arg_GetArg(4);
        if (levArmTypePtr == NULL)
        {
            printf("levArmTypePtr is NULL.\n");
            exit(EXIT_FAILURE);
        }

        taf_gnss_LeverArmParams_t *leverArmParamsPtr;
        char* endPtr;
        double forwardOffsetMeters;
        double sidewaysOffsetMeters;
        double upOffsetMeters;
        uint32_t levArmType;
        forwardOffsetMeters = strtod(forwOffsetPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad forwOffsetPtr: %s\n", forwOffsetPtr);
            exit(EXIT_FAILURE);
        }
        sidewaysOffsetMeters = strtod(sideOffsetPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad sideOffsetPtr: %s\n", sideOffsetPtr);
            exit(EXIT_FAILURE);
        }

        upOffsetMeters = strtod(upOffsePtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad upOffsePtr: %s\n", upOffsePtr);
            exit(EXIT_FAILURE);
        }

        levArmType = strtod(levArmTypePtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad levArmTypePtr: %s\n", levArmTypePtr);
            exit(EXIT_FAILURE);
        }

        le_mem_PoolRef_t LevArmFramePool = NULL;
        LevArmFramePool = le_mem_CreatePool("LevArmFramePool", sizeof(taf_gnss_LeverArmParams_t));
        leverArmParamsPtr = (taf_gnss_LeverArmParams_t*) le_mem_ForceAlloc(LevArmFramePool);
        leverArmParamsPtr->forwardOffsetMeters = forwardOffsetMeters;
        leverArmParamsPtr->sidewaysOffsetMeters = sidewaysOffsetMeters;
        leverArmParamsPtr->upOffsetMeters = upOffsetMeters;
        leverArmParamsPtr->levArmType = levArmType;
        exit(ConfigureLevArm(leverArmParamsPtr));
    }

    else if (0 == strcmp(commandPtr, "ConfigDR"))
    {
        const char* rollOffsetPtr = le_arg_GetArg(1);
        if (rollOffsetPtr == NULL)
        {
            printf("rollOffsetPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* yawOffsetPtr = le_arg_GetArg(2);
        if (yawOffsetPtr == NULL)
        {
            printf("yawOffsetPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* pitchOffsetPtr = le_arg_GetArg(3);
        if (pitchOffsetPtr == NULL)
        {
            printf("pitchOffsetPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* offsetUncPtr = le_arg_GetArg(4);
        if (offsetUncPtr == NULL)
        {
            printf("offsetUncPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* speedFactorPtr = le_arg_GetArg(5);
        if (speedFactorPtr == NULL)
        {
            printf("speedFactorPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* speedFactorUncPtr = le_arg_GetArg(6);
        if (speedFactorUncPtr == NULL)
        {
            printf("speedFactorUncPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* gyroFactorPtr = le_arg_GetArg(7);
        if (gyroFactorPtr == NULL)
        {
            printf("gyroFactorPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        const char* gyroFactorUncPtr = le_arg_GetArg(8);
        if (gyroFactorUncPtr == NULL)
        {
            printf("gyroFactorUncPtr is NULL.\n");
            exit(EXIT_FAILURE);
        }
        taf_gnss_DrParams_t *DrParamsPtr;
        char* endPtr;
        double rollOffset;
        double yawOffset;
        double pitchOffset;
        double offsetUnc;
        double speedFactor;
        double speedFactorUnc;
        double gyroFactor;
        double gyroFactorUnc;
        rollOffset = strtod(rollOffsetPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad rollOffsetPtr: %s\n", rollOffsetPtr);
            exit(EXIT_FAILURE);
        }
        yawOffset = strtod(yawOffsetPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad yawOffsetPtr: %s\n", yawOffsetPtr);
            exit(EXIT_FAILURE);
        }

        pitchOffset = strtod(pitchOffsetPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad pitchOffsetPtr: %s\n", pitchOffsetPtr);
            exit(EXIT_FAILURE);
        }

        offsetUnc = strtod(offsetUncPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad offsetUncPtr: %s\n", offsetUncPtr);
            exit(EXIT_FAILURE);
        }

        speedFactor = strtod(speedFactorPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad speedFactorPtr: %s\n", speedFactorPtr);
            exit(EXIT_FAILURE);
        }

        speedFactorUnc = strtod(speedFactorUncPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad speedFactorUncPtr: %s\n", speedFactorUncPtr);
            exit(EXIT_FAILURE);
        }

        gyroFactor = strtod(gyroFactorPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad gyroFactorPtr: %s\n", gyroFactorPtr);
            exit(EXIT_FAILURE);
        }

        gyroFactorUnc = strtod(gyroFactorUncPtr,&endPtr);
        if(endPtr[0]!='\0')
        {
            printf("Bad gyroFactorUncPtr: %s\n", gyroFactorUncPtr);
            exit(EXIT_FAILURE);
        }

        le_mem_PoolRef_t DrFramePool = NULL;
        DrFramePool = le_mem_CreatePool("DrFramePool", sizeof(taf_gnss_LeverArmParams_t));
        DrParamsPtr = (taf_gnss_DrParams_t*) le_mem_ForceAlloc(DrFramePool);
        if(DrParamsPtr != NULL)
        {
            DrParamsPtr->rollOffset = rollOffset;
            DrParamsPtr->yawOffset = yawOffset;
            DrParamsPtr->pitchOffset = pitchOffset;
            DrParamsPtr->offsetUnc = offsetUnc;
            DrParamsPtr->speedFactor = speedFactor;
            DrParamsPtr->speedFactorUnc = speedFactorUnc;
            DrParamsPtr->gyroFactor = gyroFactor;
            DrParamsPtr->gyroFactorUnc = gyroFactorUnc;
            exit(ConfigureDeadReckoning(DrParamsPtr));
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }

    else if (strcmp(commandPtr, "stop") == 0)
    {
        exit(Stop());
    }
    else if (strcmp(commandPtr, "enable") == 0)
    {
        exit(Enable());
    }
    else if (strcmp(commandPtr, "disable") == 0)
    {
        exit(Disable());
    }
    else if (strcmp(commandPtr, "restart") == 0)
    {
        const char* restartTypePtr = le_arg_GetArg(1);
        if (restartTypePtr == NULL)
        {
            printf("Restart type is NULL.\n");
            exit(EXIT_FAILURE);
        }
        // Following function exit on failure, so no need to check return code.
        CheckEnoughParams( 1,
                           numArgs,
                          "Restart type missing");
        exit(Restart(restartTypePtr));

    }
    else if (strcmp(commandPtr, "fix") == 0)
    {
        const char* fixPeriodPtr = le_arg_GetArg(1);
        uint32_t fixPeriod = DEFAULT_3D_FIX_TIME;
        //Check whether any watch period value is specified.
        if (NULL != fixPeriodPtr)
        {
            char *endPtr;
            errno = 0;
            fixPeriod = strtoul(fixPeriodPtr, &endPtr, 10);

            if (endPtr[0] != '\0' || errno != 0)
            {
                fprintf(stderr, "Bad fix period value: %s\n", fixPeriodPtr);
                exit(EXIT_FAILURE);
            }
        }
        exit(DoPosFix(fixPeriod));
    }
    else if (strcmp(commandPtr, "supportedNmeaSentences") == 0)
    {
        exit(GetSupportedNmeaSentences());
    }
    else if (strcmp(commandPtr, "supportedConstellations") == 0)
    {
        exit(GetSupportedConstellations());
    }

    else if (strcmp(commandPtr, "configDefSecBand") == 0)
    {
        exit(DefaultSecondaryBandConstellations());
    }

    else if (strcmp(commandPtr, "get") == 0)
    {
        const char* paramsPtr = le_arg_GetArg(1);
        if (NULL == paramsPtr)
        {
            LE_ERROR("paramsPtr is NULL");
            exit(EXIT_FAILURE);
        }
        CheckEnoughParams( 1,
                           numArgs,
                           "Missing arguments");
        GetGnssParams(paramsPtr);
    }
    else if (strcmp(commandPtr, "set") == 0)
    {
        const char* argNamePtr = le_arg_GetArg(1);
        const char* argValPtr = le_arg_GetArg(2);
        const char* arg2ValPtr = le_arg_GetArg(3);
        if (NULL == argNamePtr)
        {
            LE_ERROR("argNamePtr is NULL");
            printf("argNamePtr is NULL");
            exit(EXIT_FAILURE);
        }
        if (NULL == argValPtr)
        {
            LE_ERROR("argValPtr is NULL");
            printf("argValPtr is NULL");
            exit(EXIT_FAILURE);
        }
        CheckEnoughParams( 2,
                           numArgs,
                           "Missing arguments");
        exit(SetGnssParams(argNamePtr, argValPtr, arg2ValPtr));
    }
    else if (strcmp(commandPtr, "watch") == 0)
    {
        if (LE_GNSS_STATE_ACTIVE != le_gnss_GetState())
        {
            printf("GNSS is not in active state!\n");
            exit(EXIT_FAILURE);
        }

        const char* watchPeriodPtr = le_arg_GetArg(1);
        uint32_t watchPeriod = DEFAULT_WATCH_PERIOD;
        //Check whether any watch period value is specified.
        if (NULL != watchPeriodPtr)
        {
            char *endPtr;
            errno = 0;
            watchPeriod = strtoul(watchPeriodPtr, &endPtr, 10);

            if (endPtr[0] != '\0' || errno != 0)
            {
                fprintf(stderr, "Bad watch period value: %s\n", watchPeriodPtr);
                exit(EXIT_FAILURE);
            }
        }

        // Copy the command
        le_utf8_Copy(ParamsName, commandPtr, sizeof(ParamsName), NULL);
        exit(WatchGnssInfo(watchPeriod));
    }
    else
    {
        printf("Invalid command for GNSS service\n");
        exit(EXIT_FAILURE);
    }

}

