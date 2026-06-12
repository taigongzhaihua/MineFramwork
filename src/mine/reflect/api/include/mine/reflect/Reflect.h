/**
 * @file Reflect.h
 * @brief mine.reflect 模块统一包含头。
 *
 * 包含本头文件即可获得 mine.reflect 模块的全部公开 API：
 *   - TypeInfo、PropertyInfo、MethodInfo 元数据描述符
 *   - TypeRegistry 全局类型注册表
 *   - MINE_REFLECT_DECL / MINE_REFLECT_IMPL 反射注册宏
 */

#pragma once

#include <mine/reflect/TypeInfo.h>
#include <mine/reflect/PropertyInfo.h>
#include <mine/reflect/MethodInfo.h>
#include <mine/reflect/TypeRegistry.h>
#include <mine/reflect/ReflectMacros.h>
