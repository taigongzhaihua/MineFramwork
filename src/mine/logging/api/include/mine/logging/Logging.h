/**
 * @file Logging.h
 * @brief mine.logging 伞形头文件，包含所有公开日志 sink 接口。
 *
 * 快速起步：
 * @code
 *   #include <mine/logging/Logging.h>
 *   #include <mine/diag/Diag.h>
 *
 *   // 注册控制台 sink（彩色输出）
 *   uint32_t console_token = mine::logging::add_console_sink();
 *
 *   // 注册文件 sink
 *   mine::logging::FileSinkOptions file_opts;
 *   file_opts.path = "app.log";
 *   uint32_t file_token = mine::logging::add_file_sink(file_opts);
 *
 *   // 设置全局最低级别
 *   mine::diag::Logger::global().set_min_level(mine::diag::LogLevel::Info);
 *
 *   // 使用宏写日志
 *   MINE_LOG_INFO("应用启动");
 *   MINE_LOGF_ERROR("加载 %s 失败，错误码 %d", "config.toml", -1);
 * @endcode
 */

#pragma once

#include <mine/logging/Api.h>
#include <mine/logging/ModuleTag.h>
#include <mine/logging/ConsoleSink.h>
#include <mine/logging/FileSink.h>
