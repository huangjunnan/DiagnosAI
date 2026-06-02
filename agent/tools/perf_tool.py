import requests
from typing import Any, Dict
from agent.core.interfaces import DiagnosticTool

class PerfTool(DiagnosticTool):
    def __init__(self, agent_url: str = "http://localhost:8080"):
        self.agent_url = agent_url

    def execute(self, target: str, **params: Any) -> str:
        payload = {"tool": "perf", "target": target, "params": params}
        resp = requests.post(f"{self.agent_url}/diagnose", json=payload, timeout=120)
        data = resp.json()
        if data.get("status") != "success":
            raise RuntimeError(f"Proxy error: {data.get('error')}")
        return data["result"]

    def parse(self, raw_output: str) -> Dict[str, Any]:
        return {"raw_report": raw_output, "line_count": len(raw_output.splitlines())}
