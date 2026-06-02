#include "core/server.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <unordered_map>

using json = nlohmann::json;

LocalProxyServer::LocalProxyServer(std::shared_ptr<ToolRegistry> registry)
    : registry_(std::move(registry)) {}

void LocalProxyServer::start(int port)
{
    httplib::Server svr;

    svr.Get("/tools", [this](const httplib::Request &, httplib::Response &res)
            {
        json j = registry_->listTools();
        res.set_content(j.dump(), "application/json"); });

    svr.Post("/diagnose", [this](const httplib::Request &req, httplib::Response &res)
             {
        json resp;
        try {
            json body = json::parse(req.body);
            if (!body.contains("tool") || !body.contains("target")) {
                resp["status"] = "error";
                resp["error"] = "Missing 'tool' or 'target'";
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }

            std::string toolName = body["tool"];
            std::string target   = body["target"];
            std::unordered_map<std::string, std::string> params;
            if (body.contains("params") && body["params"].is_object()) {
                for (auto& [k, v] : body["params"].items())
                    params[k] = v.get<std::string>();
            }

            auto* tool = registry_->getTool(toolName);
            if (!tool) {
                resp["status"] = "error";
                resp["error"] = "Unknown tool: " + toolName;
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
                return;
            }

            std::string raw   = tool->execute(target, params);
            std::string parsed = tool->parseResult(raw);

            resp["status"] = "success";
            resp["result"] = parsed;
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            resp["status"] = "error";
            resp["error"] = e.what();
            res.status = 500;
            res.set_content(resp.dump(), "application/json");
        } });

    std::cout << "Diagnostic proxy listening on 0.0.0.0:" << port << std::endl;
    svr.listen("0.0.0.0", port);
}
