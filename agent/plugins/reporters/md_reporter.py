from typing import Any, Dict
from agent.core.interfaces import Reporter

class MarkdownReporter(Reporter):
    def generate(self, analysis_result: Dict[str, Any]) -> str:
        analysis = analysis_result.get("analysis", "No analysis.")
        return f"# AI 诊断报告\n\n{analysis}\n\n*报告由 AI 自动生成，请人工复核。*"
