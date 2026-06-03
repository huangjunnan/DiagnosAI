"""
Heaptrack 工具插件。
通过 HTTP 调用本地 C++ 代理执行 heaptrack。
"""
import logging
import requests
from typing import Any, Dict
from agent.core.interfaces import DiagnosticTool

logger = logging.getLogger(__name__)


class HeaptrackTool(DiagnosticTool):
    """封装 heaptrack 诊断工具"""

    def __init__(self, agent_url: str = "http://localhost:8080") -> None:
        self.agent_url = agent_url

    def execute(self, target: str, **params: Any) -> str:
        """
        向本地代理发送诊断请求。
        参数:
            target: 要诊断的可执行文件路径
        """
        payload = {"tool": "heaptrack", "target": target, "params": params}
        logger.debug("发送 heaptrack 请求到 %s", self.agent_url)
        try:
            resp = requests.post(f"{self.agent_url}/diagnose", json=payload, timeout=120)
            resp.raise_for_status()
            data = resp.json()
            if data.get("status") != "success":
                raise RuntimeError(f"代理返回错误: {data.get('error')}")
            raw = data["result"]
            logger.debug("HeaptrackTool 返回原始数据长度: %d 字符", len(raw))
            return raw
        except requests.RequestException as e:
            raise RuntimeError(f"网络请求失败: {str(e)}") from e

    def parse(self, raw_output: str) -> Dict[str, Any]:
        """直接返回原始文本，附加行数信息"""
        return {"raw_report": raw_output, "line_count": len(raw_output.splitlines())}
