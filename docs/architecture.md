
DiagnosAI/
├── diagnostic-proxy/          # 本地诊断代理 (C++)
│   ├── include/
│   │   ├── interfaces/
│   │   │   └── idiagnostic_tool.h
│   │   └── plugins/
│   │       └── heaptrack_tool.h
│   ├── src/
│   │   ├── core/
│   │   │   └── server.cpp
│   │   ├── plugins/
│   │   │   └── heaptrack_tool.cpp
│   │   └── main.cpp
│   ├── lib/                   # 第三方 header-only 库
│   │   ├── httplib.h
│   │   └── json.hpp
│   └── CMakeLists.txt
├── agent/                     # 云端诊断 Agent (Python)
│   ├── core/
│   │   ├── interfaces.py
│   │   └── agent.py
│   ├── plugins/
│   │   ├── analyzers/
│   │   │   ├── __init__.py
│   │   │   └── llm_analyzer.py
│   │   └── reporters/
│   │       ├── __init__.py
│   │       └── md_reporter.py
│   ├── tools/
│   │   └── heaptrack_tool.py
│   ├── main.py
│   └── requirements.txt
├── test_app/
│   ├── main.cpp
│   └── CMakeLists.txt
├── docker/
│   ├── Dockerfile
│   ├── supervisord.conf
│   └── entrypoint.sh
├── .vscode/
│   ├── extensions.json
│   └── settings.json
├── .gitignore
├── docker-compose.yml
├── LICENSE
└── README.md
