"""
核心接口定义。
所有插件必须实现这些抽象基类，以保证可扩展性。
"""
from abc import ABC, abstractmethod
from typing import Any, Dict

class DiagnosticTool(ABC):
    """诊断工具接口"""
    @abstractmethod
    def execute(self, target: str, **params: Any) -> str:
        """执行诊断并返回原始输出"""
        pass

    @abstractmethod
    def parse(self, raw_output: str) -> Dict[str, Any]:
        """解析原始输出，返回结构化数据"""
        pass

class Analyzer(ABC):
    """分析器接口"""
    @abstractmethod
    def analyze(self, structured_data: Dict[str, Any]) -> Dict[str, Any]:
        """对结构化数据进行分析，返回分析结果"""
        pass

class Reporter(ABC):
    """报告生成器接口"""
    @abstractmethod
    def generate(self, analysis_result: Dict[str, Any]) -> str:
        """将分析结果格式化为可读文本（如Markdown）"""
        pass
