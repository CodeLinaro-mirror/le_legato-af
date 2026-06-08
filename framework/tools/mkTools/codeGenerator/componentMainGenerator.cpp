//--------------------------------------------------------------------------------------------------
/**
 * @file componentMainGenerator.cpp
 *
 * Copyright (C) Sierra Wireless Inc.
 **/
//--------------------------------------------------------------------------------------------------

#include "mkTools.h"

//--------------------------------------------------------------------------------------------------
/**
 * Check if a component depends on a PA library by inspecting its ldFlags.
 *
 * A component is considered to use PA logging if its ldFlags contain a reference to a
 * "Component_taf_pa_" shared library (stored as "-lComponent_taf_pa_xxx" after mkTool
 * strips the "lib" prefix and ".so" suffix via path::GetLibShortName() during cdef parsing).
 *
 * @return true if the component depends on a PA library, false otherwise.
 */
//--------------------------------------------------------------------------------------------------
static bool UsesPaLogging
(
    const model::Component_t* componentPtr
)
//--------------------------------------------------------------------------------------------------
{
    // Check ldFlags for PA shared library references.
    // Note: mkTool stores ldFlags as "-lComponent_taf_pa_xxx" (lib prefix and .so suffix
    // are stripped by path::GetLibShortName() during cdef parsing).
    for (const auto& flag : componentPtr->ldFlags)
    {
        if (flag.find("Component_taf_pa_") != std::string::npos)
        {
            return true;
        }
    }

    return false;
}


//--------------------------------------------------------------------------------------------------
/**
 * Define the service name variables for an IPC interface.
 */
//--------------------------------------------------------------------------------------------------
static void DefineServiceNameVars
(
    std::ofstream& fileStream,      ///< File stream to write to.
    const model::ApiRef_t* interfacePtr,  ///< Ptr to client or server interface.
    bool isStandAlone   ///< true = fully resolve all interface name variables.
)
//--------------------------------------------------------------------------------------------------
{
    std::string ifLevelVar = interfacePtr->internalName + "_ServiceInstanceNamePtr";

    // If the component is being built for use by an executable built by mkexe, mkapp, or mksys,
    // then create an extern variable declaration that will be satisfied by the generated
    // _main.c for the executable, thereby allowing exe-specific interface instance naming.
    if (!isStandAlone)
    {
        std::string exeLevelVar = "_" + interfacePtr->componentPtr->name
                                + "_" + interfacePtr->internalName
                                + "_ServiceInstanceName";

        fileStream << "extern const char* " << exeLevelVar << ";\n";

        fileStream << "const char** " << ifLevelVar << " = &" << exeLevelVar << ";\n";
    }
    // If the component is being built for stand-alone use, then fully resolve the interface
    // instance names, using the internal name as the name to send to the Service Directory.
    else
    {
        std::string constName = interfacePtr->internalName + "_InterfaceName";
        fileStream << "static const char* " << constName << " = \""
                                                    << interfacePtr->internalName << "\";\n";
        fileStream << "const char** " << ifLevelVar << " = &" << constName << ";\n";
    }
}


namespace code
{


//--------------------------------------------------------------------------------------------------
/**
 * Generate _componentMain.c for a given component.
 *
 * This resolves the undefined service name symbols in all the interfaces' .o files
 * and creates a component-specific interface initialization function.
 **/
//--------------------------------------------------------------------------------------------------
void GenerateCLangComponentMainFile
(
    const model::Component_t* componentPtr,
    const mk::BuildParams_t& buildParams
)
//--------------------------------------------------------------------------------------------------
{
    auto& compName = componentPtr->name;

    // Compute the path to the output file.
    std::string outputDir = path::Minimize(buildParams.workingDir
                                        + '/'
                                        + componentPtr->workingDir
                                        + "/src");
    std::string filePath = outputDir + "/_componentMain.c";

    if (buildParams.beVerbose)
    {
        std::cout << mk::format(LE_I18N("Generating component-specific IPC code for"
                                        " component '%s' in '%s'."),
                                compName, filePath)
                  << std::endl;
    }

    // Open the .c file for writing.
    file::MakeDir(outputDir);
    std::ofstream fileStream(filePath, std::ofstream::trunc);
    if (!fileStream.is_open())
    {
        throw mk::Exception_t(
            mk::format(LE_I18N("Failed to open file '%s' for writing."), filePath)
        );
    }

    // Determine if this component depends on a PA library.
    bool enablePaLogging = UsesPaLogging(componentPtr);

    // Generate file header and #include directives.
    fileStream << "/*\n"
                  " * AUTO-GENERATED _componentMain.c for the " << compName <<
                  " component.\n"
                  "\n"
                  " * Don't bother hand-editing this file.\n"
                  " */\n"
                  "\n"
                  "#include \"legato.h\"\n";

    if (enablePaLogging)
    {
        fileStream << "#include \"tafCommonPa.h\"\n";
    }

    fileStream << "\n"
                  "#ifdef __cplusplus\n"
                  "extern \"C\" {\n"
                  "#endif\n"
                  "\n";

    // For each of the component's client-side interfaces,
    for (auto interfacePtr : componentPtr->clientApis)
    {
        DefineServiceNameVars(fileStream, interfacePtr, componentPtr->isStandAloneComp);

        // Declare the client-side interface initialization function.
        fileStream << "void " << interfacePtr->internalName << "_ConnectService(void);\n";
    }

    // For each of the component's server-side interfaces,
    for (auto interfacePtr : componentPtr->serverApis)
    {
        DefineServiceNameVars(fileStream, interfacePtr, componentPtr->isStandAloneComp);

        // Declare the server-side interface initialization function.
        fileStream << "void " << interfacePtr->internalName << "_AdvertiseService(void);\n";
    }

    // Declare the component's log session variables.
    fileStream << "// Component log session variables.\n"
                  "le_log_SessionRef_t " << compName << "_LogSession;\n"
                  "le_log_Level_t* " << compName << "_LogLevelFilterPtr;\n"
                  "\n";

    // If PA logging is enabled, generate the static level-mapping helper and the
    // PA log level change callback function forward declaration.
    if (enablePaLogging)
    {
        fileStream
            << "// Forward declaration of log_GetDltContextHandlePtr() from liblegato.\n"
            << "void* log_GetDltContextHandlePtr(void);\n"
            << "\n"
            << "// PA log level change callback (forward declaration).\n"
            << "static void " << compName << "_PaLogLevelChangeHandler("
            << "le_log_Level_t level, void *contextPtr);\n"
            << "\n"
            << "// Map Legato log level to PA log level.\n"
            << "static taf_pa_common_LogLevel_t " << compName << "_MapLeToPaLevel("
            << "le_log_Level_t level)\n"
            << "{\n"
            << "    switch (level)\n"
            << "    {\n"
            << "        case LE_LOG_DEBUG: return TAF_PA_COMMON_LOG_LEVEL_DEBUG;\n"
            << "        case LE_LOG_INFO:  return TAF_PA_COMMON_LOG_LEVEL_INFO;\n"
            << "        case LE_LOG_WARN:  return TAF_PA_COMMON_LOG_LEVEL_WARN;\n"
            << "        case LE_LOG_ERR:   return TAF_PA_COMMON_LOG_LEVEL_ERROR;\n"
            << "        case LE_LOG_CRIT:  return TAF_PA_COMMON_LOG_LEVEL_CRIT;\n"
            << "        case LE_LOG_EMERG: return TAF_PA_COMMON_LOG_LEVEL_EMERG;\n"
            << "        default:           return TAF_PA_COMMON_LOG_LEVEL_INFO;\n"
            << "    }\n"
            << "}\n"
            << "\n";
    }

    fileStream << "\n"

    // Generate forward declaration of the COMPONENT_INIT function.
                  "// Declare component's COMPONENT_INIT_ONCE function,\n"
                  "// and provide default empty implementation.\n"
                  "__attribute__((weak))\n"
                  "COMPONENT_INIT_ONCE\n"
                  "{\n"
                  "}\n"
                  "// Component initialization function (COMPONENT_INIT).\n"
                  "COMPONENT_INIT;\n"
                  "\n"

    // Define the library initialization function to be run by the dynamic linker/loader.
                  "// Library initialization function.\n"
                  "// Will be called by the dynamic linker loader when the library is loaded.\n"
                  "__attribute__((constructor)) void _" << compName << "_Init(void)\n"
                  "{\n"
                  "    LE_DEBUG(\"Initializing " << compName << " component library.\");\n"
                  "\n";

    // Call each of the component's server-side interfaces' initialization functions,
    // except those that are marked [manual-start].
    if (!componentPtr->serverApis.empty())
    {
        fileStream << "    // Advertise server-side IPC interfaces.\n";

        for (auto ifPtr : componentPtr->serverApis)
        {
            // If not marked for manual start,
            if (!(ifPtr->manualStart))
            {
                // Call the interface initialization function.
                fileStream << "    " << ifPtr->internalName << "_AdvertiseService();\n";
            }
            else
            {
                fileStream << "    // '" << ifPtr->internalName << "' is [manual-start].\n";
            }
        }

        fileStream << "\n";
    }

    // Call each of the component's client-side interfaces' initialization functions,
    // except those that are marked [manual-start].
    if (!componentPtr->clientApis.empty())
    {
        fileStream << "    // Connect client-side IPC interfaces.\n";

        for (auto ifPtr : componentPtr->clientApis)
        {
            // If not marked for manual start,
            if (!(ifPtr->manualStart))
            {
                // Call the interface initialization function.
                fileStream << "    " << ifPtr->internalName << "_ConnectService();\n";
            }
            else
            {
                fileStream << "    // '" << ifPtr->internalName << "' is [manual-start].\n";
            }
        }

        fileStream << "\n";
    }

    // Register with the Log Daemon.
    fileStream << "    // Register the component with the Log Daemon.\n"
                  "    " << compName << "_LogSession = le_log_RegComponent(\"" <<
                  compName << "\", &" << compName << "_LogLevelFilterPtr);\n";

    // If PA logging is enabled, generate PA log initialization code.
    if (enablePaLogging)
    {
        fileStream
            << "\n"
            << "    // Initialize PA logging backend and register log level change callback.\n"
            << "    {\n"
            << "        taf_pa_common_LogLevel_t initPaLevel =\n"
            << "            " << compName << "_MapLeToPaLevel(\n"
            << "                (" << compName << "_LogLevelFilterPtr != NULL)\n"
            << "                    ? *" << compName << "_LogLevelFilterPtr\n"
            << "                    : LE_LOG_INFO);\n"
            << "        taf_pa_common_LogInit(\n"
            << "#ifdef LE_CONFIG_ENABLE_DLT_LOGGING\n"
            << "                             TAF_PA_COMMON_LOG_BACKEND_DLT,\n"
            << "#else\n"
            << "                             TAF_PA_COMMON_LOG_BACKEND_SYSLOG,\n"
            << "#endif\n"
            << "                             initPaLevel,\n"
            << "                             log_GetDltContextHandlePtr());\n"
            << "        le_log_SetLevelChangeCallback(" << compName
            << "_PaLogLevelChangeHandler, NULL);\n"
            << "    }\n";
    }

    // Queue the initialization function to the event loop.
    fileStream << "\n"
                  "    // Queue the default component's COMPONENT_INIT_ONCE to Event Loop.\n"
                  "    le_event_QueueFunction(&COMPONENT_INIT_ONCE_NAME, NULL, NULL);\n"
                  "\n"
                  "    // Queue the COMPONENT_INIT function to be called by the event loop\n"
                  "    le_event_QueueFunction(&COMPONENT_INIT_NAME, NULL, NULL);\n"
                  "}\n"
                  "\n";

    // If PA logging is enabled, generate the PA log level change callback implementation.
    if (enablePaLogging)
    {
        fileStream
            << "\n"
            << "// PA log level change callback.\n"
            << "// Invoked by the Legato log system whenever the log level of this component\n"
            << "// is changed externally (via log control tool or DLT).\n"
            << "static void " << compName << "_PaLogLevelChangeHandler("
            << "le_log_Level_t level, void *contextPtr)\n"
            << "{\n"
            << "    (void)contextPtr;\n"
            << "    taf_pa_common_LogSetlevel(" << compName << "_MapLeToPaLevel(level));\n"
            << "}\n"
            << "\n";
    }

    // Put the finishing touches on the file.
    fileStream << "\n"
                  "#ifdef __cplusplus\n"
                  "}\n"
                  "#endif\n";
}


} // namespace code