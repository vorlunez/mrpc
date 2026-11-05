#include "Logger.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Router.h"
#include <cassert>
#include <stdexcept>

using mrpc::HttpRequest;
using Result = HttpRequest::ParseResult;

void complete(const std::string& wire, const std::string& body) {
    for (std::size_t split = 0; split < wire.size(); ++split) {
        HttpRequest request;
        std::size_t consumed = 99;
        assert(request.parse(wire.substr(0, split), consumed) == Result::Incomplete);
        assert(consumed == 0);
    }
    HttpRequest request;
    std::size_t consumed = 0;
    assert(request.parse(wire + "NEXT", consumed) == Result::Complete);
    assert(consumed == wire.size());
    assert(request.body == body);
    assert(request.header("HOST") == "localhost");
}
void invalid(const std::string& wire, int status) {
    HttpRequest request;
    std::size_t consumed = 99;
    assert(request.parse(wire, consumed) == Result::Error);
    assert(consumed == 0);
    assert(request.errorStatus == status);
}
int main() {
    complete("GET /hello?q=1 HTTP/1.1\r\nHost: localhost\r\n\r\n", "");
    const std::string body("hi\0there", 8);
    complete("POST /echo HTTP/1.1\r\nhOsT: localhost\r\nContent-Length: 8\r\n\r\n" + body, body);
    complete("POST /echo HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
             "2;foo=bar\r\nhi\r\n3\r\n123\r\n0\r\nX-Checksum: ok\r\n\r\n", "hi123");
    complete("POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
             "1 ; foo = \"a\\\"b\";flag\r\nx\r\n0\r\n\r\n", "x");
    invalid("GET / HTTP/1.1\r\n\r\n", 400);
    invalid("GET / HTTP/1.0\r\nHost: localhost\r\n\r\n", 505);
    invalid("GET / HTTP/1.1\r\nHost: a\r\nHOST: b\r\n\r\n", 400);
    invalid("GET / HTTP/1.1\r\nHost : a\r\n\r\n", 400);
    invalid("GET / HTTP/1.1\r\nHost: a\r\n folded: b\r\n\r\n", 400);
    const std::string base = "POST / HTTP/1.1\r\nHost: localhost\r\n";
    invalid(base + "Content-Length: 2\r\nContent-Length: 2\r\n\r\nhi", 400);
    invalid(base + "Content-Length: -1\r\n\r\n", 400);
    invalid(base + "Content-Length: 1,1\r\n\r\n", 400);
    invalid(base + "Content-Length: 9999999999999999999999999\r\n\r\n", 413);
    invalid(base + "Content-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n", 400);
    invalid(base + "Transfer-Encoding: gzip\r\n\r\n", 501);
    invalid(base + "Expect: 100-continue\r\nContent-Length: 1\r\n\r\n", 417);
    invalid(base + "Transfer-Encoding: chunked\r\n\r\nZ\r\n", 400);
    invalid(base + "Transfer-Encoding: chunked\r\n\r\n1;foo=@\r\nx\r\n0\r\n\r\n", 400);
    invalid(base + "Transfer-Encoding: chunked\r\n\r\n1;foo=\"unterminated\r\nx\r\n0\r\n\r\n", 400);
    invalid(base + "Transfer-Encoding: chunked\r\n\r\n1\r\nxXX", 400);
    invalid(base + "Transfer-Encoding: chunked\r\n\r\n100001\r\n", 413);
    invalid(base + "Transfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 1\r\n\r\n", 400);
    invalid("GET / HTTP/1.1\r\nHost: localhost\r\nX: " + std::string(16384, 'x'), 431);
    HttpRequest request;
    std::size_t consumed;
    assert(request.parse("GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive, CLOSE\r\n\r\n", consumed) == Result::Complete);
    assert(!request.keepAlive);

    mrpc::HttpResponse response;
    response.body = body;
    response.headers["Content-Type"] = "application/octet-stream";
    auto encoded = response.serialize();
    assert(encoded.find("Content-Length: 8\r\n") != std::string::npos);
    assert(encoded.substr(encoded.find("\r\n\r\n") + 4) == body);
    auto head = response.serialize(true);
    assert(head.find("Content-Length: 8\r\n") != std::string::npos);
    assert(head.substr(head.find("\r\n\r\n") + 4).empty());
    response.status = 204;
    assert(response.serialize().find("Content-Length:") == std::string::npos);
    response.status = 200;
    response.headers["X-Test"] = "bad\r\nInjected: true";
    bool rejected = false;
    try { response.serialize(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    mrpc::Router router;
    int calls = 0;
    router.add("POST", "/custom", [&](const HttpRequest& incoming) {
        ++calls;
        mrpc::HttpResponse reply;
        reply.status = 201;
        reply.body = incoming.body;
        return reply;
    });
    request.method = "POST";
    request.target = "/custom?debug=true";
    request.body = "callback body";
    auto routed = router.dispatch(request);
    assert(routed.status == 201 && routed.body == request.body && calls == 1);
    request.method = "GET";
    assert(router.dispatch(request).status == 405 && calls == 1);
    request.target = "/custom/extra";
    assert(router.dispatch(request).status == 404 && calls == 1);
    bool duplicate = false;
    try { router.add("POST", "/custom", [](const HttpRequest&) { return mrpc::HttpResponse{}; }); }
    catch (const std::invalid_argument&) { duplicate = true; }
    assert(duplicate);
    LOG(INFO) << "PASS: incremental HTTP framing, chunked, invalid requests, limits, response serialization\n";
}
