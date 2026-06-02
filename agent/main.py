import os
from agent.core.multi_tool_agent import MultiToolAgent
from agent.tools.heaptrack_tool import HeaptrackTool
from agent.tools.perf_tool import PerfTool
from agent.plugins.analyzers import LLMAnalyzer
from agent.plugins.reporters import MarkdownReporter

if __name__ == "__main__":
    OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "your-api-key")
    MODEL_NAME = "deepseek-chat"
    TARGET_PROGRAM = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..", "test_app", "build", "Debug", "test_app"
    )

    tools = [
        HeaptrackTool(agent_url="http://localhost:8080"),
        PerfTool(agent_url="http://localhost:8080"),
    ]
    analyzer = LLMAnalyzer(api_key=OPENAI_API_KEY, model=MODEL_NAME)
    reporter = MarkdownReporter()

    agent = MultiToolAgent(tools, analyzer, reporter)
    print("===== C++ 智能诊断 Agent 启动 (多工具) =====")
    report = agent.run(TARGET_PROGRAM)
    print("\n" + report)

    with open("ai_diagnosis_report.md", "w") as f:
        f.write(report)
    print("\n报告已保存到 ai_diagnosis_report.md")
