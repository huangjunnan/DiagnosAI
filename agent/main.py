#!/usr/bin/env python3
"""
DiagnosAI Agent 入口。
负责加载配置、初始化插件、执行诊断并生成报告。
"""
import os
import sys
import logging
import yaml
from typing import Any, Dict

from agent.core.multi_tool_agent import MultiToolAgent
from agent.tools.heaptrack_tool import HeaptrackTool
from agent.tools.perf_tool import PerfTool
from agent.plugins.analyzers.llm_analyzer import LLMAnalyzer
from agent.plugins.reporters.md_reporter import MarkdownReporter


def setup_logging(level: str = "INFO") -> None:
    """初始化日志系统（输出到控制台）"""
    logging.basicConfig(
        level=getattr(logging, level.upper(), logging.INFO),
        format="[%(asctime)s] [%(name)s] %(levelname)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )


def load_config(config_path: str = "config/agent.yaml") -> Dict[str, Any]:
    """加载 YAML 配置文件并返回 agent 部分"""
    try:
        with open(config_path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
        return data["agent"]
    except Exception as e:
        logging.error("无法加载配置文件 %s: %s", config_path, str(e))
        sys.exit(1)


def get_tool_class(name: str):
    """工具名称到类的映射"""
    tool_map = {
        "heaptrack": HeaptrackTool,
        "perf": PerfTool,
    }
    if name not in tool_map:
        raise ValueError(f"未知工具: {name}")
    return tool_map[name]


def main() -> None:
    # 1. 加载配置
    config = load_config("config/agent.yaml")
    log_level = config.get("log_level", "INFO")
    setup_logging(log_level)
    logger = logging.getLogger(__name__)

    # 2. 读取 API Key（必须由环境变量提供）
    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        logger.error("环境变量 OPENAI_API_KEY 未设置，无法调用 LLM")
        sys.exit(1)

    # 3. 准备目标程序路径（支持环境变量覆盖）
    target_program = os.getenv("DIAGNOSAI_TARGET", config["target"]["program"])

    # 4. 初始化工具
    proxy_url = config["proxy_url"]
    tools_enabled = config["tools"]["enabled"]
    tools = []
    for tool_name in tools_enabled:
        tool_cls = get_tool_class(tool_name)
        tools.append(tool_cls(agent_url=proxy_url))
        logger.info("已注册工具: %s", tool_name)

    # 5. 初始化分析器
    model_cfg = config["model"]
    context_limit = model_cfg.get("context_length_limit", 8000)
    analyzer = LLMAnalyzer(
        api_key=api_key,
        model=model_cfg.get("model_name", "deepseek-chat"),
        base_url=model_cfg.get("base_url", "https://api.deepseek.com"),
        temperature=model_cfg.get("temperature", 0.1),
        max_context_length=context_limit,
        prompt_template=model_cfg["prompt_template"],
    )

    # 6. 初始化报告生成器
    report_cfg = config.get("report", {})
    include_raw = report_cfg.get("include_raw", False)
    reporter = MarkdownReporter(include_raw=include_raw)

    # 7. 创建多工具代理并执行
    agent = MultiToolAgent(tools, analyzer, reporter)
    logger.info("开始诊断目标: %s", target_program)
    report = agent.run(target_program)

    # 8. 输出与保存报告
    print("\n" + report)
    output_dir = report_cfg.get("output_dir", ".")
    os.makedirs(output_dir, exist_ok=True)

    # 保存 AI 报告
    report_path = os.path.join(output_dir, "ai_diagnosis_report.md")
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(report)
    logger.info("AI 诊断报告已保存至 %s", report_path)

    # 保存原始诊断数据
    raw_data = agent.get_raw_data()
    raw_data_path = os.path.join(output_dir, "raw_report.txt")
    with open(raw_data_path, "w", encoding="utf-8") as f:
        f.write(raw_data)
    logger.info("原始诊断数据已保存至 %s", raw_data_path)


if __name__ == "__main__":
    main()
