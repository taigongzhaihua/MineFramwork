os.vrunv("xmake", {"f", "-c"})
os.vrunv("xmake", {"project", "-k", "compile_commands"})

print("MineFramework 基础构建图已生成。")