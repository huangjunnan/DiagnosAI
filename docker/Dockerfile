ARG TOOLCHAIN_IMAGE=ghcr.io/huangjunnan/diagnosai-toolchain:latest

# ---------- 第一阶段：编译 ----------
FROM ${TOOLCHAIN_IMAGE} AS builder
COPY . /workspace
RUN git submodule update --init --recursive 2>/dev/null || true

# 编译 C++ 代理和测试程序（利用工具链中的 Clang+Ninja）
RUN cd diagnostic-proxy && mkdir build && cd build && \
    cmake .. && cmake --build . --parallel $(nproc) && \
    cp diagnostic-proxy /workspace/proxy

RUN cd tests && mkdir build && cd build && \
    cmake .. && cmake --build . --parallel $(nproc) && \
    cp tests /workspace/tests

# ---------- 第二阶段：运行环境（直接复用工具链镜像） ----------
FROM ${TOOLCHAIN_IMAGE} AS runtime

WORKDIR /app

# 创建与开发环境一致的目录结构并复制二进制
COPY --from=builder /workspace/tests /app/tests
RUN chmod +x /app/tests

# 同样处理代理二进制（如果需要）
COPY --from=builder /workspace/proxy /app/proxy
RUN chmod +x /app/proxy

# 复制 Python Agent 源码和配置
COPY --from=builder /workspace/agent /app/agent
COPY --from=builder /workspace/config /app/config

# Supervisor 配置
COPY docker/supervisord.conf /etc/supervisor/conf.d/supervisord.conf
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8080
ENTRYPOINT ["/entrypoint.sh"]
