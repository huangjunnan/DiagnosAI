.PHONY: all submodules build-proxy build-test run-proxy run-agent dev clean help \
        venv docker-deploy docker-push-deploy docker-login


# ---------- 可配置参数 ----------
# ---------- 可配置参数 ----------
BUILD_TYPE ?= Debug
CXX          = clang++   # 默认编译器（确保不使用环境变量中的旧值）
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ---------- Docker 镜像信息 ----------
DOCKER_REGISTRY     ?= ghcr.io
DOCKER_NAMESPACE    ?= huangjunnan
TOOLCHAIN_IMAGE     := $(DOCKER_REGISTRY)/$(DOCKER_NAMESPACE)/diagnosai-toolchain:latest
DEPLOY_IMAGE        := $(DOCKER_REGISTRY)/$(DOCKER_NAMESPACE)/diagnosai:latest
TOOLCHAIN_DOCKERFILE := docker/Dockerfile.toolchain
DEPLOY_DOCKERFILE   := docker/Dockerfile

# ---------- Python 虚拟环境 ----------
PYTHON          := python3
VENV_DIR        := agent/venv
VENV_PYTHON     := $(VENV_DIR)/bin/python

# ---------- 路径定义 ----------
ROOT_DIR    := $(shell pwd)
PROXY_SRC   := diagnostic-proxy
TEST_SRC    := test_app
AGENT_SRC   := agent
PROXY_BUILD := $(PROXY_SRC)/build/$(BUILD_TYPE)
TEST_BUILD  := $(TEST_SRC)/build/$(BUILD_TYPE)

# ===================== 默认目标 =====================
all: build-proxy build-test

# ===================== 子模块 =====================
submodules:
	@if [ ! -f $(PROXY_SRC)/lib/httplib/httplib.h ]; then \
		echo "📥 Initializing Git submodules..."; \
		git submodule update --init --recursive; \
	fi

# ===================== 本地构建 =====================
build-proxy: submodules
	@echo "🔧 Building $(PROXY_SRC) ($(BUILD_TYPE), $(CXX))"
	@mkdir -p $(PROXY_BUILD) && cd $(PROXY_BUILD) && \
		cmake -G Ninja \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DCMAKE_CXX_COMPILER=$(CXX) \
			$(ROOT_DIR)/$(PROXY_SRC) && \
		cmake --build . --parallel $(NPROC)

build-test: submodules
	@echo "🔧 Building $(TEST_SRC) ($(BUILD_TYPE), $(CXX))"
	@mkdir -p $(TEST_BUILD) && cd $(TEST_BUILD) && \
		cmake -G Ninja \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DCMAKE_CXX_COMPILER=$(CXX) \
			$(ROOT_DIR)/$(TEST_SRC) && \
		cmake --build . --parallel $(NPROC)

# ===================== Python 虚拟环境 =====================
venv: $(VENV_PYTHON)

$(VENV_PYTHON): $(AGENT_SRC)/requirements.txt
	@echo "🐍 Creating Python virtual environment in $(VENV_DIR)..."
	$(PYTHON) -m venv $(VENV_DIR)
	$(VENV_PYTHON) -m pip install --upgrade pip
	$(VENV_PYTHON) -m pip install -r $(AGENT_SRC)/requirements.txt
	@echo "✅ Virtual environment ready."

# ===================== 运行 =====================
run-proxy:
	@echo "🚀 Starting diagnostic proxy on 0.0.0.0:8080..."
	@cd $(PROXY_BUILD) && ./diagnostic-proxy

run-agent: venv
	@echo "🤖 Starting AI Agent..."
	@cd $(ROOT_DIR) && $(VENV_PYTHON) -m agent.main

dev: venv
	@echo "⚡ Starting proxy in background..."
	@$(ROOT_DIR)/$(PROXY_BUILD)/diagnostic-proxy &
	@sleep 1
	@echo "🤖 Running Agent..."
	@cd $(ROOT_DIR) && $(VENV_PYTHON) -m agent.main; \
		echo "🛑 Stopping proxy..."; \
		pkill diagnostic-proxy 2>/dev/null || true

# ===================== Docker 部署（容器仅用于最后部署） =====================
docker-toolchain:
	@echo "🐳 Preparing toolchain image..."
	@docker pull $(TOOLCHAIN_IMAGE) 2>/dev/null || { \
		echo "⚠️  Pull failed, building image locally..."; \
		docker build -t $(TOOLCHAIN_IMAGE) -f $(TOOLCHAIN_DOCKERFILE) $(ROOT_DIR); \
	}

docker-deploy: docker-toolchain
	@echo "🏗️  Building deployment image ($(DEPLOY_IMAGE))..."
	docker build \
		--build-arg TOOLCHAIN_IMAGE=$(TOOLCHAIN_IMAGE) \
		-t $(DEPLOY_IMAGE) \
		-f $(DEPLOY_DOCKERFILE) \
		$(ROOT_DIR)

docker-push-deploy: docker-deploy
	@echo "📤 Pushing deployment image to $(DOCKER_REGISTRY)..."
	docker push $(DEPLOY_IMAGE)

docker-login:
	docker login $(DOCKER_REGISTRY) -u $(DOCKER_NAMESPACE)

# ===================== 清理 =====================
# 普通清理（保留工具链和 Docker 镜像）
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -rf $(PROXY_SRC)/build $(TEST_SRC)/build .lh
	rm -rf $(VENV_DIR)

# 彻底清理：包括临时文件、Docker 镜像等
clean-all: clean
	@echo "🧹 Cleaning everything..."
	rm -rf /tmp/diagnosai_heaptrack.gz   # heaptrack 临时输出
	rm -rf $(ROOT_DIR)/ai_diagnosis_report.md   # 生成的报告
	# 停止并删除所有相关 Docker 容器和镜像（如果需要）
	-docker stop $(docker ps -q --filter ancestor=$(DEPLOY_IMAGE)) 2>/dev/null || true
	-docker rmi $(DEPLOY_IMAGE) $(TOOLCHAIN_IMAGE) 2>/dev/null || true
	-docker system prune -f --filter "label=diagnosai"
	@echo "✅ 所有构建产物、虚拟环境、临时文件和 Docker 镜像已清除。"

# ===================== 帮助 =====================
help:
	@echo "========================================="
	@echo "  DiagnosAI 构建系统"
	@echo "========================================="
	@echo "📦 本地开发:"
	@echo "  make / make all              构建代理和测试程序 (Debug)"
	@echo "  make BUILD_TYPE=Release      以 Release 模式构建"
	@echo "  make build-proxy             只构建代理"
	@echo "  make build-test              只构建测试程序"
	@echo "  make venv                    创建 Python 虚拟环境并安装依赖"
	@echo "  make run-proxy               前台运行代理（阻塞）"
	@echo "  make run-agent               运行 AI Agent (使用虚拟环境)"
	@echo "  make dev                     开发模式（后台代理+Agent）"
	@echo "  make clean                   清理构建产物和虚拟环境"
	@echo "  make clean-all               彻底清理（包括 Docker 镜像和临时文件）"
	@echo ""
	@echo "🐳 Docker 部署 (容器仅用于最终部署):"
	@echo "  make docker-toolchain        准备工具链镜像"
	@echo "  make docker-deploy           构建部署镜像 (Release)"
	@echo "  make docker-push-deploy      构建并推送部署镜像"
	@echo "  make docker-login            登录 Docker Registry"
	@echo ""
	@echo "⚙️  变量:"
	@echo "  BUILD_TYPE=Debug|Release     本地构建类型"
	@echo "  CXX                          指定 C++ 编译器 (默认 clang++)"
	@echo "  DOCKER_REGISTRY              镜像仓库地址 (默认 ghcr.io)"
	@echo "  DOCKER_NAMESPACE             命名空间 (默认 huangjunnan)"
	@echo ""
	@echo "📘 示例:"
	@echo "  make dev BUILD_TYPE=Debug"
	@echo "  make docker-deploy           构建部署镜像"
	@echo "========================================="
