"""
单工具诊断代理（兼容旧版简单场景）。
"""
from typing import Any
from .interfaces import DiagnosticTool, Analyzer, Reporter

class DiagnosticAgent:
    """使用单一诊断工具的简单代理"""
    def __init__(self, tool: DiagnosticTool, analyzer: Analyzer, reporter: Reporter):
        self.tool = tool
        self.analyzer = analyzer
        self.reporter = reporter

    def run(self, target: str, **params: Any) -> str:
        """执行单工具诊断，返回格式化报告"""
        raw = self.tool.execute(target, **params)
        data = self.tool.parse(raw)
        analysis = self.analyzer.analyze(data)
        return self.reporter.generate(analysis)
