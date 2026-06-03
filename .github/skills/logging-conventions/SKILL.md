# MineFramework 日志和临时文件规范

## 适用场景

- 调试输出重定向
- 构建日志收集
- 测试运行结果
- 脚本执行输出
- 临时数据文件

## 日志文件路径规范

### 统一日志目录

所有日志和临时输出文件必须存放在项目根目录的 `.logs/` 目录下：

```
MineFramework/
├── .logs/                    # 日志根目录（已在 .gitignore 中忽略）
│   ├── build/               # 构建日志
│   │   ├── xmake.log
│   │   ├── compile.log
│   │   └── link.log
│   ├── test/                # 测试日志
│   │   ├── unit_tests.log
│   │   └── integration_tests.log
│   ├── debug/               # 调试输出
│   │   ├── app_stdout.txt
│   │   └── app_stderr.txt
│   └── scripts/             # 脚本执行日志
│       └── deploy.log
```

### 禁止的做法

❌ **不要**在项目根目录直接创建日志文件：

```bash
# 错误示例
./build_output.txt
./debug_output.txt
./out.txt
./err.txt
./demo_build.log
```

### 正确的做法

✅ **使用统一的日志目录**：

```bash
# 正确示例
./.logs/build/xmake_build.log
./.logs/debug/app_output.txt
./.logs/test/unit_test_results.txt
```

## 使用指南

### PowerShell 脚本

```powershell
# 确保日志目录存在
New-Item -ItemType Directory -Force -Path ".logs/build" | Out-Null

# 重定向输出到日志文件
xmake build 2>&1 | Tee-Object -FilePath ".logs/build/xmake_build.log"

# 或者直接重定向
xmake build > .logs/build/stdout.txt 2> .logs/build/stderr.txt
```

### Bash 脚本

```bash
# 创建日志目录
mkdir -p .logs/build

# 重定向输出
xmake build > .logs/build/stdout.txt 2> .logs/build/stderr.txt

# 或使用 tee 同时显示和记录
xmake build 2>&1 | tee .logs/build/xmake_build.log
```

### C++ 代码中的日志

```cpp
// 调试日志应输出到 stderr 或使用日志库
// 不要在生产代码中直接写文件到根目录

#include <fstream>
#include <filesystem>

void write_debug_log(const std::string& message) {
    std::filesystem::create_directories(".logs/debug");
    std::ofstream log(".logs/debug/app.log", std::ios::app);
    log << message << "\n";
}
```

### xmake 构建脚本

```lua
-- xmake.lua 中配置日志输出
on_build(function (target)
    -- 创建日志目录
    os.mkdir(".logs/build")
    
    -- 设置日志文件
    target:set("outputdir", ".logs/build")
end)
```

## .gitignore 规则

项目根目录的 `.gitignore` 已配置以下规则：

```gitignore
# 日志和临时输出文件（统一存放到 .logs/ 目录）
.logs/
Logs/
artifacts/
*.txt
!docs/**/*.txt
!third_party/**/*.txt
!README.txt
!CMakeLists.txt
debug_output.*
build_output.*
*_output.*
*_err.*
*_stderr.*
*_stdout.*
out*.txt
err*.txt
*.log
```

## 日志文件命名规范

### 命名模式

```
<category>_<description>_<timestamp>.log
```

**示例：**

- `build_xmake_20260603_143022.log`
- `test_unit_20260603_143500.log`
- `debug_app_stdout.txt`

### 分类前缀

| 前缀 | 用途 |
|------|------|
| `build_` | 构建过程日志 |
| `test_` | 测试执行日志 |
| `debug_` | 调试输出 |
| `perf_` | 性能分析日志 |
| `script_` | 脚本执行日志 |

## 日志清理

### 手动清理

```powershell
# 删除所有日志（保留目录结构）
Remove-Item -Recurse -Force .logs/*

# 删除 7 天前的日志
Get-ChildItem .logs -Recurse -File | 
    Where-Object {$_.LastWriteTime -lt (Get-Date).AddDays(-7)} | 
    Remove-Item
```

### 自动清理（CI/CD）

在 `.github/workflows/cleanup.yml` 中配置定期清理：

```yaml
name: Clean Logs
on:
  schedule:
    - cron: '0 0 * * 0'  # 每周日午夜执行

jobs:
  cleanup:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Clean old logs
        run: |
          find .logs -type f -mtime +7 -delete
          git clean -fdX .logs/
```

## 违反规范的处理

如果发现根目录有日志文件：

1. **立即清理**：
   ```powershell
   Remove-Item *.log, *.txt, *_output.*, *_err.* -Exclude README.txt,LICENSE.txt
   ```

2. **修正脚本**：更新生成这些文件的脚本，使用 `.logs/` 目录

3. **更新 .gitignore**：确保新的日志模式被忽略

## 注意事项

- ⚠️ `.logs/` 目录不会提交到 Git，本地独享
- ⚠️ CI/CD 环境每次都是全新环境，日志不会累积
- ⚠️ 重要的测试结果应保存到 `artifacts/` 或专门的报告目录
- ⚠️ 不要在日志中记录敏感信息（密码、密钥等）

## 最佳实践

1. **使用日志库**：推荐 spdlog, boost.log
2. **结构化日志**：使用 JSON 格式便于解析
3. **日志级别**：Debug < Info < Warning < Error < Fatal
4. **定期清理**：避免磁盘占用过大
5. **CI 归档**：重要日志上传到 GitHub Actions Artifacts

## 参考资源

- [spdlog 文档](https://github.com/gabime/spdlog)
- [日志最佳实践](https://www.loggly.com/ultimate-guide/cpp-logging-basics/)
- [.gitignore 模式](https://git-scm.com/docs/gitignore)
