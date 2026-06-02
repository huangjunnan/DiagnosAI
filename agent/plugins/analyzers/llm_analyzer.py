from typing import Any, Dict
from agent.core.interfaces import Analyzer
from openai import OpenAI

class LLMAnalyzer(Analyzer):
    def __init__(self, api_key: str, model: str = "gpt-3.5-turbo"):
        self.client = OpenAI(api_key=api_key, base_url="https://api.deepseek.com")
        self.model = model

    def analyze(self, structured_data: Dict[str, Any]) -> Dict[str, Any]:
        report_text = structured_data.get("raw_report", "")
        if len(report_text) > 8000:
            report_text = report_text[:8000] + "\n... (truncated)"

        prompt = f"""你是一个资深的 C++ 性能专家，下面是一个程序的内存分析报告（来自 heaptrack）。
请分析并回答：
1. 是否存在内存泄漏？依据是什么？
2. 如果存在，泄漏规模有多大？（估算字节数）
3. 最可能的泄漏原因是什么？（给出调用栈或代码位置）
4. 提供修复建议。

内存分析报告：
{report_text}
"""
        response = self.client.chat.completions.create(
            model=self.model,
            messages=[{"role": "user", "content": prompt}],
            temperature=0.1,
        )
        return {"analysis": response.choices[0].message.content, "model": self.model}
