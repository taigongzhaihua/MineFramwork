/**
 * @file Mvvm.h
 * @brief mine.mvvm 模块伞形头文件，包含所有公开 API。
 *
 * 使用时只需包含此文件：
 * @code
 *   #include <mine/mvvm/Mvvm.h>
 * @endcode
 */

#pragma once

#include <mine/mvvm/Api.h>
#include <mine/mvvm/ModuleTag.h>
#include <mine/mvvm/CollectionChangedArgs.h>
#include <mine/mvvm/INotifyCollectionChanged.h>
#include <mine/mvvm/ObservableCollectionBase.h>
#include <mine/mvvm/ObservableCollection.h>
#include <mine/mvvm/ObservableObject.h>
#include <mine/mvvm/ViewModelBase.h>
#include <mine/mvvm/Observable.h>

// 重导出命令接口（供 MVVM 模式使用，实现在 mine.ui.event）
#include <mine/ui/event/ICommand.h>
#include <mine/ui/event/RelayCommand.h>
