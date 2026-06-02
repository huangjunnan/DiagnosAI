import os
from agent.core.agent import DiagnosticAgent
from agent.tools.heaptrack_tool import HeaptrackTool
from agent.plugins.analyzers import LLMAnalyzer
from agent.plugins.reporters import MarkdownReporter

if __name__ == "__main__":
    OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "your-api-key")
    MODEL_NAME = "deepseek-chat"   # DeepSeek 模型
    # test_app 编译产物的绝对路径
    TARGET_PROGRAM = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..", "test_app", "build", "Debug", "test_app"
    )

    tool = HeaptrackTool(agent_url="http://localhost:8080")
    analyzer = LLMAnalyzer(api_key=OPENAI_API_KEY, model=MODEL_NAME)
    reporter = MarkdownReporter()

    agent = DiagnosticAgent(tool, analyzer, reporter)
    print("===== C++ 内存泄漏智能诊断 Agent 启动 =====")
    report = agent.run(TARGET_PROGRAM)
    print("\n" + report)

    with open("ai_diagnosis_report.md", "w") as f:
        f.write(report)
    print("\n报告已保存到 ai_diagnosis_report.md")
