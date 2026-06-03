"""
多工具协同代理。
可同时运行多个诊断工具，合并报告后统一分析。
"""
import logging
from typing import Any, List
from .interfaces import DiagnosticTool, Analyzer, Reporter

logger = logging.getLogger(__name__)


class MultiToolAgent:
    """可编排多个诊断工具的代理"""

    def __init__(self, tools: List[DiagnosticTool], analyzer: Analyzer, reporter: Reporter) -> None:
        self.tools = tools
        self.analyzer = analyzer
        self.reporter = reporter
        self._last_raw_data = ""  # 保存最近一次合并的原始报告

    def run(self, target: str, **params: Any) -> str:
        """
        依次执行所有工具，收集其输出，合并后交给分析器。
        即使某些工具失败，也会继续执行剩余工具，并将错误信息作为报告的一部分。
        """
        tool_reports: List[str] = []
        for tool in self.tools:
            tool_name = tool.__class__.__name__
            logger.info("正在执行工具: %s", tool_name)
            try:
                raw = tool.execute(target, **params)
                data = tool.parse(raw)
                report = data.get("raw_report", "")
                if report:
                    tool_reports.append(f"=== {tool_name} 报告 ===\n{report}")
                    logger.info("工具 %s 输出 %d 字符", tool_name, len(report))
                    logger.debug("%s 原始输出 (前500字符):\n%s", tool_name, report[:500])
                else:
                    tool_reports.append(f"=== {tool_name} 报告为空 ===")
                    logger.warning("工具 %s 未返回任何报告", tool_name)
            except Exception as e:
                error_msg = f"工具 {tool_name} 执行失败: {str(e)}"
                logger.error(error_msg)
                tool_reports.append(f"=== {tool_name} 报告 ===\n错误: {error_msg}")

        combined_raw = "\n\n".join(tool_reports)
        self._last_raw_data = combined_raw
        logger.info("合并后总报告长度: %d 字符", len(combined_raw))

        if not combined_raw.strip():
            logger.warning("所有工具均未产生有效输出，将返回空报告")
            return self.reporter.generate({"analysis": "所有诊断工具均未能生成报告，请检查代理和目标程序状态。"})

        analysis = self.analyzer.analyze({"raw_report": combined_raw})
        # 将原始数据传递给报告器（是否展示由报告器配置决定）
        analysis["raw_data"] = combined_raw
        return self.reporter.generate(analysis)

    def get_raw_data(self) -> str:
        """返回最近一次诊断的合并原始报告"""
        return self._last_raw_data
