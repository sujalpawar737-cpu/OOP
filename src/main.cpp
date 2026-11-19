// Government Scheme Eligibility Finder - OOP course project (C++)
// Starts a small web server (cpp-httplib) that serves the pages in web/ and answers
// the requests that the pages make.
//
// Run from the project folder:   ./scheme_finder [--port 8080] [--host 127.0.0.1]
#include <cstdlib>
#include <iostream>
#include <string>

#include "Api.h"
#include "Repository.h"
#include "UsageLog.h"
#include "../lib/httplib.h"

// copies the form fields of a request into a plain map
static Params readParams(const httplib::Request& req) {
    Params params;
    for (auto it = req.params.begin(); it != req.params.end(); ++it) {
        params[it->first] = it->second;
    }
    return params;
}

static void sendJson(httplib::Response& res, const std::string& body, int status = 200) {
    res.status = status;
    res.set_header("Cache-Control", "no-store");
    res.set_content(body, "application/json; charset=utf-8");
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    std::string dataFolder = "data";
    std::string webFolder = "web";
    int port = 8080;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: scheme_finder [--port N] [--host ADDRESS]\n";
            return 0;
        }
    }

    try {
        std::cout << "Loading data from '" << dataFolder << "' ...\n";
        SchemeRepository repo(dataFolder);
        std::cout << repo.summary() << "\n";

        UsageLog log(dataFolder + "/usage.dat");
        ApiController api(repo, log);

        httplib::Server server;
        server.set_payload_max_length(64 * 1024);   // the answers are tiny

        // basic security headers on every response
        server.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
            res.set_header("X-Content-Type-Options", "nosniff");
            res.set_header("X-Frame-Options", "DENY");
            res.set_header("Content-Security-Policy",
                           "default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:");
        });

        server.Get("/api/meta", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, api.meta());
        });

        server.Get("/api/domains", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, api.domains());
        });

        server.Get("/api/schemes", [&](const httplib::Request& req, httplib::Response& res) {
            std::string domain = req.has_param("domain") ? req.get_param_value("domain") : "all";
            std::string query = req.has_param("q") ? req.get_param_value("q") : "";
            sendJson(res, api.directory(domain, query));
        });

        server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
            sendJson(res, api.stats());
        });

        server.Post("/api/evaluate", [&](const httplib::Request& req, httplib::Response& res) {
            int status = 200;
            std::string body = api.evaluate(readParams(req), status);
            sendJson(res, body, status);
        });

        server.Post("/api/combine", [&](const httplib::Request& req, httplib::Response& res) {
            int status = 200;
            std::string body = api.combine(readParams(req), status);
            sendJson(res, body, status);
        });

        server.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr) {
            sendJson(res, "{\"error\":\"Something went wrong on the server.\",\"details\":[]}", 500);
        });

        if (!server.set_mount_point("/", webFolder)) {
            std::cerr << "Cannot find the folder '" << webFolder << "'. Run the program from the project folder.\n";
            return 1;
        }

        std::cout << "Open http://localhost:" << port << "/ in your browser (Ctrl+C to stop)\n";
        if (!server.listen(host, port)) {
            std::cerr << "Could not start on " << host << ":" << port << " - is the port already in use?\n";
            return 1;
        }
    } catch (const DataError& e) {
        std::cerr << "Data problem: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
