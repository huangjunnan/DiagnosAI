"""
JSON 格式报告生成器。
输出简洁的 JSON 对象，包含诊断结论和可选的原始数据。
"""

import json
from typing import Any, Dict
from agent.core.interfaces import Reporter


class JsonReporter(Reporter):
    """生成 JSON 格式的诊断报告"""

    def __init__(self, include_raw: bool = False) -> None:
        self.include_raw = include_raw

    def generate(self, analysis_result: Dict[str, Any]) -> str:
        """
        从分析结果生成最终的 JSON 字符串。
        如果 analysis_result 中包含 'analysis' 键且其值为字符串，
        则尝试从中提取 JSON 对象（可能被 markdown 代码块包裹）。
        其他字段（如 memory_leak、cpu_hotspots）若存在则直接使用。
        最后统一输出缩进格式化且保持中文可读的 JSON。
        """
        # 1. 如果存在 'analysis' 文本字段，尝试提取并解析其中的 JSON
        if "analysis" in analysis_result and isinstance(
            analysis_result["analysis"], str
        ):
            raw_text = analysis_result["analysis"]
            # 去除可能的 markdown 代码块标记
            cleaned = re.sub(r"```(?:json)?\s*\n?", "", raw_text)
            cleaned = re.sub(r"\n?```", "", cleaned)
            cleaned = cleaned.strip()
            try:
                parsed = json.loads(cleaned)
                # 用解析出的内容更新 analysis_result（保留原有其他字段如 raw_data）
                analysis_result.update(parsed)
            except json.JSONDecodeError:
                # 如果无法解析为 JSON，则保留原始 analysis 文本
                pass

        # 2. 构建最终输出对象
        report = {}
        # 优先使用已知的诊断字段，如果没有则尝试保留 analysis 文本
        for key in ("memory_leak", "cpu_hotspots", "target", "tools"):
            if key in analysis_result:
                report[key] = analysis_result[key]
        # 如果没有任何诊断字段，但存在 analysis 文本，则作为后备
        if not report and "analysis" in analysis_result:
            report["diagnosis"] = analysis_result["analysis"]

        # 3. 如果配置要求包含原始数据，则追加
        if self.include_raw and "raw_data" in analysis_result:
            report["raw_data"] = analysis_result["raw_data"]

        # 4. 序列化为格式化 JSON 字符串
        return json.dumps(report, indent=2, ensure_ascii=False)
