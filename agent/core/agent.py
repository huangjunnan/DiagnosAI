from typing import Any
from .interfaces import DiagnosticTool, Analyzer, Reporter

class DiagnosticAgent:
    def __init__(self, tool: DiagnosticTool, analyzer: Analyzer, reporter: Reporter):
        self.tool = tool
        self.analyzer = analyzer
        self.reporter = reporter

    def run(self, target: str, **params: Any) -> str:
        raw = self.tool.execute(target, **params)
        data = self.tool.parse(raw)
        result = self.analyzer.analyze(data)
        return self.reporter.generate(result)
