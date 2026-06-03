#include "core/server.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <iostream>
#include <unordered_map>
#include "logger.h"

using json = nlohmann::json;

DEFINE_MODULE_LOG(server)

LocalProxyServer::LocalProxyServer(std::shared_ptr<ToolRegistry> registry)
    : registry_(std::move(registry)) {}

void LocalProxyServer::start(int port)
{
    L_INFO("代理启动，监听端口 {}", port);
    L_DEBUG("已注册工具数量: {}", registry_->listTools().size());

    httplib::Server svr;

    svr.Get("/tools", [this](const httplib::Request &req, httplib::Response &res)
            {
        L_INFO("GET /tools 请求，来源: {}", req.remote_addr);
        json j = registry_->listTools();
        res.set_content(j.dump(), "application/json"); });

    svr.Post("/diagnose", [this](const httplib::Request &req, httplib::Response &res)
             {
        auto start_time = std::chrono::steady_clock::now();
        L_INFO("POST /diagnose 请求，来源: {}", req.remote_addr);
        L_TRACE("请求体: {}", req.body);

        json resp;
        try {
            json body = json::parse(req.body);
            if (!body.contains("tool") || !body.contains("target")) {
                L_WARN("请求缺少 'tool' 或 'target' 字段");
                resp["status"] = "error";
                resp["error"] = "Missing 'tool' or 'target'";
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }

            std::string toolName = body["tool"];
            std::string target   = body["target"];
            L_INFO("请求诊断: tool={}, target={}", toolName, target);

            std::unordered_map<std::string, std::string> params;
            if (body.contains("params") && body["params"].is_object()) {
                for (auto& [k, v] : body["params"].items()) {
                    params[k] = v.get<std::string>();
                }
                L_TRACE("附带参数数量: {}", params.size());
            }

            auto* tool = registry_->getTool(toolName);
            if (!tool) {
                L_ERROR("未知工具: {}", toolName);
                resp["status"] = "error";
                resp["error"] = "Unknown tool: " + toolName;
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }

            L_DEBUG("开始执行工具 {}", toolName);
            auto exec_start = std::chrono::steady_clock::now();
            std::string raw = tool->execute(target, params);
            auto exec_end = std::chrono::steady_clock::now();
            auto exec_ms = std::chrono::duration_cast<std::chrono::milliseconds>(exec_end - exec_start).count();
            L_DEBUG("工具 {} 执行完成，耗时 {} ms，原始输出大小 {} 字节",
                    toolName, exec_ms, raw.size());

            auto parse_start = std::chrono::steady_clock::now();
            std::string parsed = tool->parseResult(raw);
            auto parse_end = std::chrono::steady_clock::now();
            auto parse_ms = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - parse_start).count();
            L_DEBUG("解析完成，耗时 {} ms，解析后大小 {} 字节", parse_ms, parsed.size());

            resp["status"] = "success";
            resp["result"] = parsed;
            res.set_content(resp.dump(), "application/json");

            auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start_time).count();
            L_INFO("请求处理成功，总耗时 {} ms", total_ms);
        } catch (const std::exception& e) {
            L_ERROR("诊断异常: {}", e.what());
            resp["status"] = "error";
            resp["error"] = e.what();
            res.status = 500;
            res.set_content(resp.dump(), "application/json");
        } });

    std::cout << "Diagnostic proxy listening on 0.0.0.0:" << port << std::endl;
    svr.listen("0.0.0.0", port);
}
