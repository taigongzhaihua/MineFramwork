-- mine.mvvm 模块构建配置
-- MVVM 模式基础：ViewModelBase、ObservableObject、ObservableCollection<T>
-- RelayCommand / ICommand 由 mine.ui.event 提供（此处重导出）

mine_module("mine.mvvm", {
    short_name = "mvvm",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.ui.binding",   -- INotifyPropertyChanged
        "mine.ui.event",     -- ICommand / RelayCommand（重导出）
    },
})
