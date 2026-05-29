/**
 * @file Config.h
 * @brief mine.config 模块伞形头文件。
 *
 * 包含此文件即引入 mine.config 模块的全部公共接口：
 *   - ConfigValueType  — 值类型枚举
 *   - ConfigValue      — 配置值类型（null/bool/integer/float/string）
 *   - ConfigLayer      — 配置层枚举（Default/File/Env）
 *   - ConfigFormat     — 配置文件格式（JSON/TOML/Auto）
 *   - ConfigManager    — 分层配置管理器
 */

#pragma once

#include <mine/config/Api.h>
#include <mine/config/ModuleTag.h>
#include <mine/config/ConfigValue.h>
#include <mine/config/ConfigManager.h>
