from abc import ABC, abstractmethod
from typing import Any, Dict

class DiagnosticTool(ABC):
    @abstractmethod
    def execute(self, target: str, **params: Any) -> str:
        pass

    @abstractmethod
    def parse(self, raw_output: str) -> Dict[str, Any]:
        pass

class Analyzer(ABC):
    @abstractmethod
    def analyze(self, structured_data: Dict[str, Any]) -> Dict[str, Any]:
        pass

class Reporter(ABC):
    @abstractmethod
    def generate(self, analysis_result: Dict[str, Any]) -> str:
        pass
