"""
多工具协同代理。
负责依次执行诊断工具，将每个工具的输出分别交给分析器（区分内存/CPU），最后合并结果。
"""

import logging
from typing import Any, List
from .interfaces import DiagnosticTool, Analyzer, Reporter

logger = logging.getLogger(__name__)


class MultiToolAgent:
    def __init__(
        self, tools: List[DiagnosticTool], analyzer: Analyzer, reporter: Reporter
    ) -> None:
        self.tools = tools
        self.analyzer = analyzer
        self.reporter = reporter
        self._last_raw_data = ""

    def run(self, target: str, **params: Any) -> str:
        """
        依次执行所有工具，并将每个工具的输出单独交给分析器（使用对应的 Prompt）。
        最后合并分析结果，交给报告生成器。
        """
        tool_reports: List[str] = []
        # 收集所有工具的输出
        for tool in self.tools:
            tool_name = tool.__class__.__name__
            logger.info("正在执行工具: %s", tool_name)
            try:
                raw = tool.execute(target, **params)
                data = tool.parse(raw)
                report = data.get("raw_report", "")
                if report:
                    tool_reports.append(f"=== {tool_name} 报告 ===\n{report}")
                else:
                    tool_reports.append(f"=== {tool_name} 报告为空 ===")
            except Exception as e:
                error_msg = f"工具 {tool_name} 执行失败: {str(e)}"
                logger.error(error_msg)
                tool_reports.append(f"=== {tool_name} 报告 ===\n错误: {error_msg}")

        combined_raw = "\n\n".join(tool_reports)
        self._last_raw_data = combined_raw

        # 分别分析每个工具的报告，使用不同的 data_type 触发不同的 Prompt
        analysis_results = {}
        for tool in self.tools:
            tool_name = tool.__class__.__name__
            # 根据工具名判断数据类型
            if "heaptrack" in tool_name.lower():
                data_type = "memory"
            elif "perf" in tool_name.lower() or "gperf" in tool_name.lower():
                data_type = "cpu"
            else:
                data_type = "unknown"

            # 提取该工具对应的原始输出
            tool_raw = ""
            for tr in tool_reports:
                if tr.startswith(f"=== {tool_name} 报告 ==="):
                    # 去掉标题行
                    parts = tr.split("\n", 1)
                    if len(parts) > 1:
                        tool_raw = parts[1]
                    break

            if tool_raw:
                analysis = self.analyzer.analyze(
                    {"raw_report": tool_raw, "data_type": data_type}
                )
                if isinstance(analysis, dict):
                    analysis_results.update(analysis)
            else:
                logger.warning("工具 %s 未产出有效报告，跳过分析", tool_name)

        if not analysis_results:
            analysis_results = {"error": "所有工具均未生成有效分析"}

        # 将原始数据一并传给报告生成器
        analysis_results["raw_data"] = combined_raw
        return self.reporter.generate(analysis_results)

    def get_raw_data(self) -> str:
        return self._last_raw_data
