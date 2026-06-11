/**
 * @file Async.h
 * @brief mine.async 模块伞形头文件。
 *
 * 包含所有 mine.async 模块的公开接口：
 *   - Future<T> / Promise<T>：异步结果传递
 *   - Task<T>：可组合异步任务
 *   - Dispatcher：跨线程任务调度
 *   - ThreadPool：工作线程池
 *   - Timer：延迟/周期性定时器
 *
 * 使用方式：在需要异步功能的源文件中 `#include <mine/async/Async.h>`。
 */

#pragma once

#include <mine/async/Api.h>
#include <mine/async/ModuleTag.h>
#include <mine/async/Future.h>
#include <mine/async/Task.h>
#include <mine/async/Dispatcher.h>
#include <mine/async/ThreadPool.h>
#include <mine/async/Timer.h>
