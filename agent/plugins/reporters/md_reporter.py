"""
Markdown 格式报告生成器。
"""
from typing import Any, Dict
from agent.core.interfaces import Reporter


class MarkdownReporter(Reporter):
    """生成 Markdown 格式的诊断报告，可选是否包含原始数据"""

    def __init__(self, include_raw: bool = False) -> None:
        self.include_raw = include_raw

    def generate(self, analysis_result: Dict[str, Any]) -> str:
        analysis = analysis_result.get("analysis", "无法生成分析。")
        md = f"# AI 诊断报告\n\n{analysis}\n\n*报告由 AI 自动生成，请人工复核。*"
        if self.include_raw and "raw_data" in analysis_result:
            raw = analysis_result["raw_data"]
            md += f"\n\n## 原始诊断数据\n\n```\n{raw}\n```"
        return md
