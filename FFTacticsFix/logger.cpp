#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <stdlib.h>
#include <wtypes.h>
#include <string>
#include <filesystem>
#include <iostream>

#define LOG_FORMAT_PREFIX "[%Y-%m-%d %H:%M:%S.%e] [FFTacticsFix] [%l]"

std::shared_ptr<spdlog::logger> logger;

bool InitializeLogger(HMODULE gameModule)
{
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(gameModule, exePath, MAX_PATH);
    std::filesystem::path sExePath = exePath;

    try
    {
        if (!std::filesystem::is_directory(sExePath.string() + "logs"))
        {
            std::filesystem::create_directory(sExePath.string() + "logs");
        }
        logger = spdlog::basic_logger_mt("FFTacticsFix", sExePath.string() + "logs\\FFTacticsFix.log", true);
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(logger);
        spdlog::set_pattern(LOG_FORMAT_PREFIX ": %v");
        spdlog::info("FFTacticsFix loaded.");
    }
    catch (const spdlog::spdlog_ex& ex) {
        return false;
    }

    return true;
}