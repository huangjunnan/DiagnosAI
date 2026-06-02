FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake gdb heaptrack libc6-dbg \
    python3 python3-pip supervisor curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 复制源码并构建 C++ 代理
COPY diagnostic-proxy/ ./diagnostic-proxy/
RUN cd diagnostic-proxy && mkdir build && cd build && cmake .. && make -j$(nproc) && cp diagnostic-proxy /app/proxy

# 复制 Python Agent
COPY agent/ ./agent/
RUN pip3 install -r agent/requirements.txt

# 复制测试程序
COPY test_app/ ./test_app/
RUN cd test_app && mkdir build && cd build && cmake .. && make -j$(nproc) && cp test_app /app/test_app_bin

# Supervisor 配置
COPY docker/supervisord.conf /etc/supervisor/conf.d/supervisord.conf
COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8080
ENTRYPOINT ["/entrypoint.sh"]
