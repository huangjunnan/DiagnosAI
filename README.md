
feature/perf-tool — 新增 Perf 诊断工具

feature/gdb-tool — 新增 GDB 死锁检测

feature/llm-analyzer-v2 — 升级 LLM 分析器

fix/heaptrack-parser — 修复解析器 Bug

refactor/agent-interfaces — 重构接口

1. 日常开发在分支上进行，别在标签上改代码
标签不是一个“工作区”，你不能 git checkout v0.1.0 然后改代码提交。正确流程：

# 开发新功能

git checkout develop                    # 切到开发分支
git checkout -b feature/cpu-diagnose   # 创建功能分支

# ... 写代码

git add . && git commit -m "feat: CPU热点诊断"
git checkout develop && git merge feature/cpu-diagnose

# 功能稳定后合并到 main 并打新标签

git checkout main && git merge develop
git tag v0.2.0
git push origin main --tags
