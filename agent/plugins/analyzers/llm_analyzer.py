"""
LLM 分析器插件。
支持内存诊断和 CPU 诊断两个独立的 Prompt 模板，并可将业务生命周期配置注入分析上下文。
"""

import json
import logging
from typing import Any, Dict, Optional
from agent.core.interfaces import Analyzer
from openai import OpenAI

logger = logging.getLogger(__name__)


class LLMAnalyzer(Analyzer):
    """基于大语言模型的分析器，支持内存和 CPU 两种模式，可注入业务规则"""

    def __init__(
        self,
        api_key: str,
        model: str,
        base_url: str,
        temperature: float,
        max_context_length: int,
        memory_prompt_template: str,
        cpu_prompt_template: str,
        biz_life_config: Optional[Dict[str, Any]] = None,
    ) -> None:
        self.client = OpenAI(api_key=api_key, base_url=base_url)
        self.model = model
        self.temperature = temperature
        self.max_context_length = max_context_length
        self.memory_prompt = memory_prompt_template
        self.cpu_prompt = cpu_prompt_template
        self.biz_life = biz_life_config or {}

    def analyze(self, structured_data: Dict[str, Any]) -> Dict[str, Any]:
        report_text = structured_data.get("raw_report", "")
        data_type = structured_data.get("data_type", "memory")

        logger.info(
            "LLM 分析器收到报告，类型: %s，长度: %d 字符", data_type, len(report_text)
        )
        if not report_text.strip():
            return {"error": "报告为空"}

        if len(report_text) > self.max_context_length:
            report_text = report_text[: self.max_context_length] + "\n... (截断)"

        template = self.memory_prompt if data_type == "memory" else self.cpu_prompt
        biz_context = self._build_biz_context() if data_type == "memory" else ""

        full_prompt = biz_context + "\n\n" + template.format(report_text=report_text)
        logger.debug("发送 Prompt，类型: %s，长度: %d", data_type, len(full_prompt))

        try:
            response = self.client.chat.completions.create(
                model=self.model,
                messages=[{"role": "user", "content": full_prompt}],
                temperature=self.temperature,
                timeout=120,  # 120 秒超时
            )
            analysis = response.choices[0].message.content
            try:
                result = json.loads(analysis)
            except json.JSONDecodeError:
                result = {"analysis": analysis}
            return result
        except Exception as e:
            logger.error("LLM 调用失败: %s", str(e))
            return {"error": str(e)}

    def _build_biz_context(self) -> str:
        """根据 biz_life.yaml 构建业务上下文文本"""
        if not self.biz_life:
            return ""

        lines = ["## 业务生命周期规则（由项目配置提供）\n"]
        lines.append("以下规则用于判断内存是否应该释放：\n")

        entities = self.biz_life.get("entities", {})
        if entities:
            lines.append("**业务实体与预期生命周期**：")
            life_desc_map = {
                "ProgramExit": "程序退出时释放（全局常驻）",
                "UserLogout": "用户登出时释放",
                "ConnClose": "连接断开时释放",
                "RequestFinish": "请求处理完释放",
                "TaskFinish": "任务执行完释放",
            }
            for pattern, life in entities.items():
                desc = life_desc_map.get(life, life)
                lines.append(f"- 匹配 `{pattern}` 的类/函数 → **{desc}**")
            lines.append("")

        white_list = self.biz_life.get("white_list", [])
        if white_list:
            lines.append("**系统白名单（这些调用栈永远不算泄漏）**：")
            for item in white_list:
                lines.append(f"- `{item}`")
            lines.append("")

        thresholds = self.biz_life.get("thresholds", {})
        if thresholds:
            lines.append("**判定阈值**：")
            lines.append(
                f"- 内存增长率超过 {thresholds.get('growth_rate', 0.05)} 视为持续增长"
            )
            lines.append(
                f"- 同一栈分配超过 {thresholds.get('suspicious_alloc_count', 10)} 次且持续增长视为可疑"
            )
            lines.append(
                f"- 系统开销单次不超过 {thresholds.get('system_overhead_max_bytes', 102400)} 字节"
            )
            lines.append("")

        lines.append(
            "**请严格依据以上业务规则，结合 heaptrack 报告中的调用栈、分配次数、内存增长率，判断每个泄漏点的类别。**\n"
        )
        return "\n".join(lines)
