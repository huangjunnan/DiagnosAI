from typing import Any, List
from .interfaces import DiagnosticTool, Analyzer, Reporter

class MultiToolAgent:
    def __init__(self, tools: List[DiagnosticTool], analyzer: Analyzer, reporter: Reporter):
        self.tools = tools
        self.analyzer = analyzer
        self.reporter = reporter

    def run(self, target: str, **params: Any) -> str:
        all_results = []
        for tool in self.tools:
            print(f"\n🔧 正在执行工具: {tool.__class__.__name__} ...")
            raw = tool.execute(target, **params)
            data = tool.parse(raw)
            all_results.append({
                "tool": tool.__class__.__name__,
                "data": data
            })
        # 将所有工具的输出合并
        combined = {
            "tool_results": all_results
        }
        analysis = self.analyzer.analyze(combined)
        return self.reporter.generate(analysis)
