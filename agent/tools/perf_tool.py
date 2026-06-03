"""
Perf 工具插件。
通过 HTTP 调用本地 C++ 代理执行 perf stat。
"""
import logging
import requests
from typing import Any, Dict
from agent.core.interfaces import DiagnosticTool

logger = logging.getLogger(__name__)


class PerfTool(DiagnosticTool):
    """封装 perf 性能分析工具"""

    def __init__(self, agent_url: str = "http://localhost:8080") -> None:
        self.agent_url = agent_url

    def execute(self, target: str, **params: Any) -> str:
        payload = {"tool": "perf", "target": target, "params": params}
        logger.debug("发送 perf 请求到 %s", self.agent_url)
        try:
            resp = requests.post(f"{self.agent_url}/diagnose", json=payload, timeout=120)
            resp.raise_for_status()
            data = resp.json()
            if data.get("status") != "success":
                raise RuntimeError(f"代理返回错误: {data.get('error')}")
            raw = data["result"]
            logger.debug("PerfTool 返回原始数据长度: %d 字符", len(raw))
            return raw
        except requests.RequestException as e:
            raise RuntimeError(f"网络请求失败: {str(e)}") from e

    def parse(self, raw_output: str) -> Dict[str, Any]:
        return {"raw_report": raw_output, "line_count": len(raw_output.splitlines())}
