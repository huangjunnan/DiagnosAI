.PHONY: all submodules build-proxy build-test run-proxy run-agent dev clean clean-all \
        venv docker-deploy docker-push-deploy docker-login \
        docker-build-toolchain docker-build-deploy docker-build-all \
        docker-debug docker-debug-diagnose format lint \
        help

# ---------- 可配置参数（支持环境变量覆盖） ----------
BUILD_TYPE      ?= Debug
CXX             ?= clang++   # 条件赋值，尊重用户环境变量
CMAKE_GENERATOR ?= Ninja     # 可配置CMake生成器
NPROC           := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ---------- Docker 镜像信息 ----------
DOCKER_REGISTRY      ?= ghcr.io
DOCKER_NAMESPACE     ?= huangjunnan
TOOLCHAIN_IMAGE      := $(DOCKER_REGISTRY)/$(DOCKER_NAMESPACE)/diagnosai-toolchain:latest
DEPLOY_IMAGE         := $(DOCKER_REGISTRY)/$(DOCKER_NAMESPACE)/diagnosai:latest
TOOLCHAIN_DOCKERFILE := docker/Dockerfile.toolchain
DEPLOY_DOCKERFILE    := docker/Dockerfile

# ---------- Python 虚拟环境 ----------
PYTHON      := python3
VENV_DIR    := agent/venv
VENV_PYTHON := $(VENV_DIR)/bin/python

# ---------- 路径定义（统一按编译类型生成子目录） ----------
ROOT_DIR    := $(shell pwd)
PROXY_SRC   := diagnostic-proxy
TEST_SRC    := tests
AGENT_SRC   := agent

# 强制统一构建目录结构：无论什么生成器，都使用 build/$(BUILD_TYPE)
PROXY_BUILD := $(PROXY_SRC)/build/$(BUILD_TYPE)
TEST_BUILD  := $(TEST_SRC)/build/$(BUILD_TYPE)

# ===================== 默认目标 =====================
all: build-proxy build-test

# ===================== 子模块 =====================
submodules:
	@if [ ! -f $(PROXY_SRC)/lib/httplib/httplib.h ]; then \
		echo "📥 初始化 Git 子模块..."; \
		git submodule update --init --recursive; \
	fi

# ===================== 本地构建（核心修复：强制指定二进制输出目录） =====================
build-proxy: submodules
	@echo "🔧 正在构建 $(PROXY_SRC) ($(BUILD_TYPE), $(CXX), $(CMAKE_GENERATOR))"
	@echo "📂 构建目录: $(PROXY_BUILD)"
	@mkdir -p $(PROXY_BUILD) && cd $(PROXY_BUILD) && \
		cmake -G $(CMAKE_GENERATOR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DCMAKE_CXX_COMPILER=$(CXX) \
			-B . \
			-S $(ROOT_DIR)/$(PROXY_SRC) && \
		cmake --build . --parallel $(NPROC)

build-test: submodules
	@echo "🔧 正在构建 $(TEST_SRC) ($(BUILD_TYPE), $(CXX), $(CMAKE_GENERATOR))"
	@echo "📂 构建目录: $(TEST_BUILD)"
	@mkdir -p $(TEST_BUILD) && cd $(TEST_BUILD) && \
		cmake -G $(CMAKE_GENERATOR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DCMAKE_CXX_COMPILER=$(CXX) \
			-B . \
			-S $(ROOT_DIR)/$(TEST_SRC) && \
		cmake --build . --parallel $(NPROC)

# ===================== 代码质量工具 =====================
lint: build-proxy
	@echo "🔍 正在运行静态分析..."
	@cd $(PROXY_BUILD) && cmake --build . --target lint
	@echo "✅ 静态分析完成"

# ===================== Python 虚拟环境 =====================
venv: $(VENV_PYTHON)

$(VENV_PYTHON): $(AGENT_SRC)/requirements.txt
	@echo "🐍 正在创建 Python 虚拟环境 $(VENV_DIR)..."
	$(PYTHON) -m venv $(VENV_DIR)
	$(VENV_PYTHON) -m pip install --upgrade pip
	$(VENV_PYTHON) -m pip install -r $(AGENT_SRC)/requirements.txt
	@echo "✅ 虚拟环境准备完成。"

# ===================== 运行 =====================
run-proxy:
	@echo "🚀 正在启动诊断代理，监听 0.0.0.0:8080..."
	@echo "📂 执行文件: $(PROXY_BUILD)/diagnostic-proxy"
	@cd $(PROXY_BUILD) && ./diagnostic-proxy

run-agent: venv
	@echo "🤖 正在启动 AI Agent..."
	@if [ -z "$$OPENAI_API_KEY" ]; then \
		echo "❌ 环境变量 OPENAI_API_KEY 未设置。请运行 export OPENAI_API_KEY='你的密钥' 后重试。"; \
		exit 1; \
	fi
	@cd $(ROOT_DIR) && $(VENV_PYTHON) -m agent.main

dev: venv
	@echo "🛑 正在停止旧代理进程..."
	-@pkill -f "$(PROXY_BUILD)/diagnostic-proxy" 2>/dev/null || true
	@sleep 0.5
	@echo "⚡ 正在启动新代理（工作目录：$(ROOT_DIR)）"
	@echo "📂 执行文件: $(PROXY_BUILD)/diagnostic-proxy"
	@cd $(ROOT_DIR) && $(PROXY_BUILD)/diagnostic-proxy & \
		PROXY_PID=$$!; \
		echo "代理 PID: $$PROXY_PID"; \
		sleep 2; \
		if ! kill -0 $$PROXY_PID 2>/dev/null; then \
			echo "❌ 代理启动失败！"; \
			exit 1; \
		fi; \
		if [ -z "$$OPENAI_API_KEY" ]; then \
			echo "⚠️ 环境变量 OPENAI_API_KEY 未设置。请运行 export OPENAI_API_KEY='你的密钥' 后重试。"; \
			kill $$PROXY_PID 2>/dev/null || true; \
			exit 1; \
		fi; \
		echo "🤖 正在运行 AI Agent..."; \
		cd $(ROOT_DIR) && $(VENV_PYTHON) -m agent.main; \
		AGENT_EXIT_CODE=$$?; \
		echo "🛑 诊断结束，正在停止代理..."; \
		kill $$PROXY_PID 2>/dev/null || true; \
		exit $$AGENT_EXIT_CODE

# ===================== Docker 构建与调试 =====================
docker-build-toolchain:
	@echo "🐳 构建工具链镜像: $(TOOLCHAIN_IMAGE)"
	docker build \
		--build-arg http_proxy=$(http_proxy) \
		--build-arg https_proxy=$(https_proxy) \
		-t $(TOOLCHAIN_IMAGE) -f $(TOOLCHAIN_DOCKERFILE) $(ROOT_DIR)

docker-build-deploy:
	@echo "🏗️ 构建部署镜像: $(DEPLOY_IMAGE)"
	docker build \
		--build-arg TOOLCHAIN_IMAGE=$(TOOLCHAIN_IMAGE) \
		-t $(DEPLOY_IMAGE) \
		-f $(DEPLOY_DOCKERFILE) \
		$(ROOT_DIR)

docker-build-all: docker-build-toolchain docker-build-deploy

docker-debug:
	@echo "🐳 启动调试容器（交互式 shell）"
	docker run -it --rm \
		--name diagnosai-debug \
		--cap-add=SYS_PTRACE \
		-p 8080:8080 \
		-e OPENAI_API_KEY="$(OPENAI_API_KEY)" \
		--entrypoint /bin/bash \
		$(DEPLOY_IMAGE)

docker-debug-diagnose:
	@echo "🐳 启动诊断容器（自动运行 agent）"
	@if [ -z "$(OPENAI_API_KEY)" ]; then \
		echo "❌ 请设置环境变量 OPENAI_API_KEY"; \
		exit 1; \
	fi
	docker run -it --rm \
		--name diagnosai-diagnose \
		--cap-add=SYS_PTRACE \
		--security-opt seccomp=unconfined \
		-p 8080:8080 \
		-e OPENAI_API_KEY="$(OPENAI_API_KEY)" \
		-v $(ROOT_DIR)/reports:/app/reports \
		--entrypoint /bin/bash \
		$(DEPLOY_IMAGE) -c "\
			echo '⚡ 启动代理...'; \
			./proxy & \
			sleep 2; \
			echo '🤖 运行 AI Agent...'; \
			python3 -m agent.main"

# ===================== Docker 镜像推送与登录 =====================
docker-push-deploy: docker-build-deploy
	@echo "📤 推送部署镜像到 $(DOCKER_REGISTRY)..."
	docker push $(DEPLOY_IMAGE)

docker-push-toolchain:
	@echo "📤 推送工具链镜像到 $(DOCKER_REGISTRY)..."
	docker push $(TOOLCHAIN_IMAGE)

docker-login:
	docker login $(DOCKER_REGISTRY) -u $(DOCKER_NAMESPACE)

# ===================== 清理 =====================
clean:
	@echo "🧹 正在清理构建工件..."
	@if [ -n "$(PROXY_SRC)" ] && [ -n "$(TEST_SRC)" ]; then \
		rm -rf $(PROXY_SRC)/build $(TEST_SRC)/build .lh 2>/dev/null; \
	fi
	@if [ -n "$(VENV_DIR)" ]; then \
		rm -rf $(VENV_DIR) 2>/dev/null; \
	fi
	@echo "🧹 清理 /tmp 下的 DiagnosAI 临时文件..."
	@rm -f /tmp/diagnosai_heaptrack_* /tmp/diagnosai_gperf_*

clean-all: clean
	@echo "🧹 正在清理所有内容..."
	@rm -rf /tmp/diagnosai_heaptrack.gz 2>/dev/null
	@rm -rf $(ROOT_DIR)/reports/* 2>/dev/null
	@echo "🧹 清理 /tmp 下的 DiagnosAI 临时文件..."
	@rm -f /tmp/diagnosai_heaptrack_* /tmp/diagnosai_gperf_*
	@echo "✅ 所有构建工件、虚拟环境、临时文件已清除。"

# ===================== 帮助 =====================
help:
	@echo "========================================="
	@echo "  DiagnosAI 构建系统（统一目录版）"
	@echo "========================================="
	@echo "📦 本地开发:"
	@echo "  make / make all              构建代理和测试程序 (Debug)"
	@echo "  make BUILD_TYPE=Release      以 Release 模式构建"
	@echo "  make CXX=g++                 使用GCC编译器构建"
	@echo "  make CMAKE_GENERATOR=Unix Makefiles  使用Makefile生成器"
	@echo "  make build-proxy             只构建代理"
	@echo "  make build-test              只构建测试程序"
	@echo "  make format                  格式化所有C++代码"
	@echo "  make lint                    运行clang-tidy静态分析"
	@echo "  make venv                    创建 Python 虚拟环境并安装依赖"
	@echo "  make run-proxy               前台运行代理（阻塞）"
	@echo "  make run-agent               运行 AI Agent (自动检查API_KEY)"
	@echo "  make dev                     开发模式（后台代理+Agent）"
	@echo "  make clean                   清理构建工件和虚拟环境"
	@echo "  make clean-all               彻底清理（包括报告和临时文件）"
	@echo ""
	@echo "🐳 Docker 辅助命令:"
	@echo "  make docker-build-toolchain   构建工具链镜像"
	@echo "  make docker-build-deploy      构建部署镜像"
	@echo "  make docker-build-all         构建所有镜像"
	@echo "  make docker-debug             启动调试容器（交互式 shell）"
	@echo "  make docker-debug-diagnose    自动运行诊断（需 OPENAI_API_KEY）"
	@echo "  make docker-push-deploy       构建并推送部署镜像"
	@echo "  make docker-login             登录 Docker Registry"
	@echo ""
	@echo "⚙️  变量:"
	@echo "  BUILD_TYPE=Debug|Release     本地构建类型"
	@echo "  CXX                          指定 C++ 编译器 (默认 clang++)"
	@echo "  CMAKE_GENERATOR              CMake生成器 (默认 Ninja)"
	@echo "  DOCKER_REGISTRY              镜像仓库地址 (默认 ghcr.io)"
	@echo "  DOCKER_NAMESPACE             命名空间 (默认 huangjunnan)"
	@echo "  OPENAI_API_KEY               你的 OpenAI 或 DeepSeek API 密钥 (必需)"
	@echo ""
	@echo "📘 示例:"
	@echo "  export OPENAI_API_KEY='sk-你的密钥'"
	@echo "  make dev BUILD_TYPE=Debug CXX=g++"
	@echo "  make docker-build-all http_proxy=http://127.0.0.1:7890"
	@echo "  make docker-debug-diagnose OPENAI_API_KEY=sk-xxx"
	@echo "========================================="
