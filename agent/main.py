#!/usr/bin/env python3
"""
DiagnosAI Agent 入口 —— 支持并发、增量、多进程诊断，集成业务感知内存分析。
"""

import os
import sys
import json
import logging
import time
import yaml
from concurrent.futures import ThreadPoolExecutor, as_completed, TimeoutError
from typing import Any, Dict, List, Optional

from agent.core.multi_tool_agent import MultiToolAgent
from agent.tools.heaptrack_tool import HeaptrackTool
from agent.tools.perf_tool import PerfTool
from agent.tools.gperf_tool import GperfTool
from agent.plugins.analyzers.llm_analyzer import LLMAnalyzer
from agent.plugins.reporters.json_reporter import JsonReporter


def setup_logging(level: str = "INFO") -> None:
    logging.basicConfig(
        level=getattr(logging, level.upper(), logging.INFO),
        format="[%(asctime)s] [%(name)s] %(levelname)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )


def load_config(config_path: str = "config/agent.yaml") -> Dict[str, Any]:
    with open(config_path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)
    return data["agent"]


def load_biz_life_config(config_path: str = "config/biz_life.yaml") -> Dict[str, Any]:
    """加载业务生命周期配置"""
    logger = logging.getLogger(__name__)
    try:
        with open(config_path, "r", encoding="utf-8") as f:
            return yaml.safe_load(f)
    except FileNotFoundError:
        logger.warning("未找到业务配置文件 %s，将不使用业务规则", config_path)
        return {}
    except Exception as e:
        logger.error("加载业务配置失败: %s", str(e))
        return {}


def get_tool_class(name: str):
    tool_map = {
        "heaptrack": HeaptrackTool,
        "perf": PerfTool,
        "gperf": GperfTool,
    }
    if name not in tool_map:
        raise ValueError(f"未知工具: {name}")
    return tool_map[name]


def load_history(history_dir: str, target: str) -> Dict[str, Any]:
    """加载指定目标的诊断历史记录"""
    os.makedirs(history_dir, exist_ok=True)
    safe_name = target.replace("/", "_").replace(".", "_")
    history_file = os.path.join(history_dir, f"{safe_name}.json")
    if os.path.exists(history_file):
        with open(history_file, "r", encoding="utf-8") as f:
            return json.load(f)
    return {}


def save_history(history_dir: str, target: str, record: Dict[str, Any]) -> None:
    """保存当前诊断摘要到历史记录"""
    os.makedirs(history_dir, exist_ok=True)
    safe_name = target.replace("/", "_").replace(".", "_")
    history_file = os.path.join(history_dir, f"{safe_name}.json")
    with open(history_file, "w", encoding="utf-8") as f:
        json.dump(record, f, indent=2, ensure_ascii=False)


def diagnose_single_target(
    target: str,
    tools: List[Any],
    analyzer: LLMAnalyzer,
    reporter: JsonReporter,
    history_dir: Optional[str] = None,
) -> Dict[str, Any]:
    """
    对单个目标执行诊断，可选择增量模式。
    返回包含 target 和 report 的字典。
    """
    logger = logging.getLogger(__name__)
    logger.info("开始诊断目标: %s", target)

    agent = MultiToolAgent(tools, analyzer, reporter)
    report_json_str = agent.run(target)

    try:
        report_data = json.loads(report_json_str)
    except json.JSONDecodeError:
        report_data = {"diagnosis": report_json_str}

    if history_dir:
        prev = load_history(history_dir, target)
        leak_info = report_data.get("memory_leak", {})
        current_leaked = leak_info.get("total_leaked_bytes", 0)
        current_ts = time.time()

        if prev and "total_leaked_bytes" in prev:
            prev_leaked = prev["total_leaked_bytes"]
            if prev_leaked > 0:
                growth = (current_leaked - prev_leaked) / prev_leaked
            else:
                growth = 1.0 if current_leaked > 0 else 0.0
            trend = {
                "previous_leaked_bytes": prev_leaked,
                "current_leaked_bytes": current_leaked,
                "growth_rate": round(growth, 4),
                "previous_timestamp": prev.get("timestamp"),
                "current_timestamp": current_ts,
            }
            report_data.setdefault("memory_leak", {})["trend"] = trend
            logger.info(
                "增量诊断: 上次泄漏 %d B，本次 %d B，增长率 %.2f%%",
                prev_leaked,
                current_leaked,
                growth * 100,
            )

        save_history(
            history_dir,
            target,
            {
                "timestamp": current_ts,
                "total_leaked_bytes": current_leaked,
                "leak_points": leak_info.get("leak_points", []),
            },
        )

    return {"target": target, "report": report_data}


def run_concurrent_diagnostics(
    targets: List[str],
    tools: List[Any],
    analyzer: LLMAnalyzer,
    reporter: JsonReporter,
    max_workers: int = 4,
    history_dir: Optional[str] = None,
    timeout: int = 600,  # 单个任务最大等待时间（秒）
) -> List[Dict[str, Any]]:
    """使用线程池并发诊断多个目标，带超时保护"""
    logger = logging.getLogger(__name__)
    results = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        future_to_target = {
            executor.submit(
                diagnose_single_target, target, tools, analyzer, reporter, history_dir
            ): target
            for target in targets
        }
        for future in as_completed(future_to_target, timeout=timeout):
            target = future_to_target[future]
            try:
                res = future.result(timeout=30)  # 每个任务最多等30秒
                results.append(res)
            except TimeoutError:
                logger.error("诊断目标 %s 超时，已跳过", target)
                results.append({"target": target, "report": {"error": "timeout"}})
            except Exception as e:
                logger.error("诊断目标 %s 失败: %s", target, str(e))
                results.append({"target": target, "report": {"error": str(e)}})
    return results


def main() -> None:
    config = load_config("config/agent.yaml")
    log_level = config.get("log_level", "INFO")
    setup_logging(log_level)
    logger = logging.getLogger(__name__)

    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        logger.error("OPENAI_API_KEY 未设置")
        sys.exit(1)

    diag_cfg = config.get("diagnostics", {})
    targets = config.get("targets", [])
    if not targets:
        single = os.getenv(
            "DIAGNOSAI_TARGET", config.get("target", {}).get("program", "")
        )
        if single:
            targets = [single]
    if not targets:
        logger.error("没有配置任何诊断目标")
        sys.exit(1)

    proxy_url = config["proxy_url"]
    tools_enabled = config["tools"]["enabled"]
    tools = []
    for name in tools_enabled:
        tool_cls = get_tool_class(name)
        tools.append(tool_cls(agent_url=proxy_url))
        logger.info("已注册工具: %s", name)

    model_cfg = config["model"]
    context_limit = model_cfg.get("context_length_limit", 8000)

    # 加载业务配置
    biz_life = load_biz_life_config()

    analyzer = LLMAnalyzer(
        api_key=api_key,
        model=model_cfg.get("model_name", "deepseek-chat"),
        base_url=model_cfg.get("base_url", "https://api.deepseek.com"),
        temperature=model_cfg.get("temperature", 0.1),
        max_context_length=context_limit,
        memory_prompt_template=model_cfg["memory_prompt_template"],
        cpu_prompt_template=model_cfg["cpu_prompt_template"],
        biz_life_config=biz_life,  # 业务配置
    )

    report_cfg = config.get("report", {})
    include_raw = report_cfg.get("include_raw", False)
    reporter = JsonReporter(include_raw=include_raw)

    mode = diag_cfg.get("mode", "once")
    history_dir = diag_cfg.get("history_dir") if mode == "incremental" else None

    if mode == "concurrent":
        max_workers = diag_cfg.get("max_workers", 4)
        logger.info("并发模式：目标数 %d，最大线程 %d", len(targets), max_workers)
        results = run_concurrent_diagnostics(
            targets, tools, analyzer, reporter, max_workers, history_dir
        )
    else:
        results = []
        for target in targets:
            res = diagnose_single_target(target, tools, analyzer, reporter, history_dir)
            results.append(res)

    output_dir = report_cfg.get("output_dir", ".")
    os.makedirs(output_dir, exist_ok=True)
    summary = {}
    for res in results:
        summary[res["target"]] = res["report"]

    summary_path = os.path.join(output_dir, "diagnosis_summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    logger.info("汇总报告已保存至 %s", summary_path)


if __name__ == "__main__":
    main()
