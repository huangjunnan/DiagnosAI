"""
LLM 分析器插件。
使用 OpenAI 兼容接口调用大语言模型进行智能分析。
所有配置项均通过构造函数传入，无硬编码。
"""
import logging
from typing import Any, Dict
from agent.core.interfaces import Analyzer
from openai import OpenAI

logger = logging.getLogger(__name__)


class LLMAnalyzer(Analyzer):
    """基于大语言模型的分析器，完全由外部配置驱动"""

    def __init__(
        self,
        api_key: str,
        model: str,
        base_url: str,
        temperature: float,
        max_context_length: int,
        prompt_template: str,
    ) -> None:
        """
        参数:
            api_key: API 密钥
            model: 模型名称
            base_url: API 端点
            temperature: 生成温度
            max_context_length: 注入 LLM 的报告最大字符数
            prompt_template: 提示词模板，需包含 {report_text} 占位符
        """
        self.client = OpenAI(api_key=api_key, base_url=base_url)
        self.model = model
        self.temperature = temperature
        self.max_context_length = max_context_length
        self.prompt_template = prompt_template

    def analyze(self, structured_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        调用 LLM 分析诊断报告。
        从 structured_data 中读取 'raw_report' 字段作为诊断数据。
        """
        report_text = structured_data.get("raw_report", "")
        logger.info("LLM 分析器收到报告，长度: %d 字符", len(report_text))

        if not report_text.strip():
            logger.warning("报告为空，无法进行分析")
            return {
                "analysis": "诊断报告为空，无法生成分析结果。",
                "model": self.model,
            }

        # 按配置截断过长报告
        if len(report_text) > self.max_context_length:
            report_text = report_text[: self.max_context_length] + "\n... (截断)"
            logger.info(
                "报告过长，已截断至 %d 字符", self.max_context_length
            )

        # 使用配置的 prompt 模板生成最终 prompt
        prompt = self.prompt_template.format(report_text=report_text)
        logger.debug("发送 Prompt 至 LLM，长度: %d", len(prompt))

        try:
            response = self.client.chat.completions.create(
                model=self.model,
                messages=[{"role": "user", "content": prompt}],
                temperature=self.temperature,
            )
            analysis = response.choices[0].message.content
            logger.info("LLM 返回分析，长度: %d 字符", len(analysis))
            return {"analysis": analysis, "model": self.model}
        except Exception as e:
            logger.error("LLM 调用失败: %s", str(e))
            return {
                "analysis": f"LLM 分析失败: {str(e)}",
                "model": self.model,
            }
