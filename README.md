DiagnosAI - C++ 智能诊断系统
AI 驱动的 C++ 程序内存泄漏、死锁和性能问题自动诊断工具
项目简介
DiagnosAI 是一个结合了传统调试工具和大语言模型的智能诊断系统，能够自动检测 C++ 程序中的内存泄漏、死锁和性能瓶颈，并生成详细的分析报告和修复建议。
✨ 核心功能
已实现
内存泄漏诊断：基于 heaptrack 的精准内存泄漏检测
AI 智能分析：集成 DeepSeek/OpenAI 大模型，自动分析诊断报告
插件式架构：支持多种诊断工具的无缝集成
本地代理：高性能 C++ 代理，负责执行诊断命令和收集数据
复杂场景支持：条件提前返回泄漏、异常泄漏等复杂场景诊断
开发中
CPU 性能分析：基于 perf 的 CPU 热点诊断
死锁检测：基于 gdb 的自动死锁检测和分析
LLM 分析器 v2：更强大的代码理解和修复建议生成能力
🏗️ 项目架构
plaintext
DiagnosAI/
├── diagnostic-proxy/    # C++ 诊断代理（执行本地命令）
│   ├── src/             # 代理源代码
│   ├── lib/             # 第三方库（Git子模块）
│   └── tools/           # 诊断工具插件
├── agent/               # Python AI Agent
│   ├── core/            # 核心框架
│   ├── plugins/         # 分析器插件
│   └── tools/           # 工具客户端
├── tests/            # 测试用例程序
├── docker/              # Docker 部署文件
└── Makefile             # 统一构建系统
🚀 快速开始
环境要求
Linux 系统（推荐 Ubuntu 22.04+ / Debian 12+）
Clang++ 14+
CMake 3.20+
Ninja 构建系统
Python 3.10+
heaptrack 内存分析工具
安装依赖
bash
运行
# 安装系统依赖
sudo apt update && sudo apt install -y \
    clang cmake ninja-build python3 python3-venv \
    heaptrack git build-essential
克隆项目
bash
运行
git clone --recursive https://github.com/huangjunnan/DiagnosAI.git
cd DiagnosAI
配置 API 密钥
创建 .env 文件：
env
# 选择一个你喜欢的LLM提供商
# DeepSeek（推荐，性价比高）
OPENAI_API_KEY=sk-xxx
OPENAI_BASE_URL=https://api.deepseek.com/v1
LLM_MODEL=deepseek-chat

# 或者 OpenAI
# OPENAI_API_KEY=sk-xxx
# LLM_MODEL=gpt-4o
运行开发模式
bash
运行
# 自动构建所有组件并运行
make dev
🛠️ 开发指南
构建系统
项目使用 Makefile 统一管理所有构建任务：
bash
运行
# 构建所有组件（Debug模式）
make

# 以 Release 模式构建
make BUILD_TYPE=Release

# 只构建诊断代理
make build-proxy

# 只构建测试程序
make build-test

# 创建Python虚拟环境并安装依赖
make venv

# 前台运行代理
make run-proxy

# 单独运行AI Agent
make run-agent

# 清理所有构建产物
make clean
分支管理流程
我们采用 Git Flow 分支管理模型：
main：稳定发布分支，只能通过合并 develop 分支更新
develop：开发分支，所有功能开发都基于此分支
feature/ ：功能分支，用于开发新功能
4. fix/ ：修复分支，用于修复 bug
5. refactor/* **：重构分支，用于代码重构
开发新功能
bash
运行
# 切到开发分支
git checkout develop

# 创建功能分支
git checkout -b feature/cpu-diagnose

# ... 编写代码 ...

# 提交修改
git add .
git commit -m "feat: 实现CPU热点诊断功能"

# 合并回开发分支
git checkout develop
git merge feature/cpu-diagnose
发布新版本
bash
运行
# 确保开发分支稳定
git checkout develop
git status

# 合并到主分支
git checkout main
git merge develop

# 打标签
git tag -a v0.1.0 -m "v0.1.0 - 首个内存泄漏诊断原型"

# 推送到远程
git push origin main --tags
🐳 Docker 部署
构建工具链镜像
bash
运行
make docker-toolchain
构建部署镜像
bash
运行
make docker-deploy
推送镜像
bash
运行
make docker-login
make docker-push-deploy
📝 版本历史
v0.1.0 (2026-06-02)
✅ 首个可演示版本发布
✅ 实现 C++ 本地代理（diagnostic-proxy），插件式工具封装
✅ 实现 Python AI Agent，调用 DeepSeek/OpenAI 分析 heaptrack 报告
✅ 支持条件提前返回泄漏、异常泄漏等复杂场景诊断
✅ 使用 Clang + Ninja + ccache 构建，Makefile 统一管理
✅ 集成 Git 子模块管理第三方库，Docker 部署支持
🤝 贡献指南
Fork 本仓库
创建你的功能分支 (git checkout -b feature/amazing-feature)
提交你的修改 (git commit -m 'feat: 添加一些 amazing 的功能')
推送到分支 (git push origin feature/amazing-feature)
打开一个 Pull Request
📄 许可证
本项目采用 MIT 许可证 - 详见 LICENSE 文件。


这个错误非常常见且完全正常：feature/perf-tool 是你在本地创建的新分支，还没有推送到远程仓库，所以 Git 不知道它应该跟踪哪个远程分支。
一步到位的完整操作
bash
运行
# 1. 先切换到develop分支，拉取最新代码（确保你的功能分支基于最新代码）
git checkout develop
git pull origin develop

# 2. 切回你的功能分支，合并最新的develop代码（避免后续冲突）
git checkout feature/perf-tool
git merge develop

# 3. 将本地分支推送到远程，并自动设置上游跟踪
git push -u origin feature/perf-tool
