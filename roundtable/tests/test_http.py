# no-port-check: NereusSDR-original.
"""The HTTP listener over real sockets (wire spec 2.1, 10.1): one request a
connection, static files with the CSP header, no directory listings, no
way out of the static folder, the request limits, the route table later
parts add to, and the read API's headers and address groups."""

from __future__ import annotations

import asyncio
import json
import os
from typing import Dict, List, Optional, Tuple

import pytest

from helpers import live_service
from nereus_roundtable import http as rt_http
from nereus_roundtable.http import CSP, Response

CSP_TEXT = (
    "default-src 'self'; connect-src 'self'; img-src 'self' data:; style-src 'self'; "
    "script-src 'self'; frame-ancestors 'none'; form-action 'self'"
)


class Answer:
    def __init__(self, status: int, headers: Dict[str, List[str]], body: bytes) -> None:
        self.status = status
        self.headers = headers
        self.body = body

    def header(self, name: str) -> Optional[str]:
        values = self.headers.get(name.lower())
        return values[-1] if values else None


async def raw(port: int, data: bytes) -> Tuple[Answer, bool]:
    """Send bytes, read the whole answer; True when the server then closed."""
    reader, writer = await asyncio.open_connection("127.0.0.1", port)
    writer.write(data)
    await writer.drain()
    text = await asyncio.wait_for(reader.read(), 5)
    writer.close()
    head, _, body = text.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    status = int(lines[0].split()[1])
    headers: Dict[str, List[str]] = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        headers.setdefault(name.lower(), []).append(value.strip())
    return Answer(status, headers, body), True


async def request(port: int, method: str, target: str, headers: str = "", body: bytes = b"") -> Answer:
    head = f"{method} {target} HTTP/1.1\r\nHost: roundtable.conformance.invalid\r\n{headers}"
    if body:
        head += f"Content-Length: {len(body)}\r\n"
    answer, _ = await raw(port, (head + "\r\n").encode("ascii") + body)
    return answer


@pytest.fixture
def static(tmp_path):
    root = tmp_path / "static"
    (root / "css").mkdir(parents=True)
    (root / "empty").mkdir()
    (root / "index.html").write_text("<!doctype html><title>Roundtable</title>\n", encoding="utf-8")
    (root / "css" / "page.css").write_text("body { margin: 0 }\n", encoding="utf-8")
    (root / ".hidden").write_text("secret\n", encoding="utf-8")
    (tmp_path / "outside.txt").write_text("outside\n", encoding="utf-8")
    os.symlink(str(tmp_path / "outside.txt"), str(root / "link.txt"))
    return str(root)


def test_the_csp_is_the_brief_text():
    assert CSP == CSP_TEXT


def test_static_files_carry_the_csp_and_their_type(static):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:
            port = live.http_port
            answer = await request(port, "GET", "/")
            assert answer.status == 200 and answer.body.startswith(b"<!doctype html>")
            assert answer.header("Content-Type") == "text/html; charset=utf-8"
            assert answer.header("Content-Security-Policy") == CSP_TEXT
            assert answer.header("Connection") == "close"
            assert answer.header("Content-Length") == str(len(answer.body))
            answer = await request(port, "GET", "/css/page.css?v=1")
            assert answer.status == 200 and answer.header("Content-Type") == "text/css; charset=utf-8"
            assert answer.header("Content-Security-Policy") == CSP_TEXT
            answer = await request(port, "HEAD", "/index.html")
            assert answer.status == 200 and answer.body == b""
            assert int(answer.header("Content-Length")) > 0

    asyncio.run(go())


def test_no_directory_listing_no_dot_dot_and_nothing_outside(static):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:
            port = live.http_port
            for target in (
                "/css",
                "/css/",
                "/empty/",
                "/../outside.txt",
                "/css/../index.html",
                "/%2e%2e/outside.txt",
                "/css/%2E%2E/index.html",
                "/css%2F..%2Findex.html",
                "/..%5Coutside.txt",
                "/.hidden",
                "/link.txt",
                "/missing.html",
                "/index.html%00",
                "/%ff",
            ):
                answer = await request(port, "GET", target)
                assert answer.status == 404, target
                assert answer.header("Content-Security-Policy") == CSP_TEXT, target
                assert b"index.html" not in answer.body and b"page.css" not in answer.body, target

    asyncio.run(go())


def test_methods_and_request_limits(static):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:
            port = live.http_port
            answer = await request(port, "POST", "/index.html", body=b"x=1")
            assert answer.status == 405 and answer.header("Allow") == "GET, HEAD"
            answer = await request(port, "POST", "/index.html", body=b"x" * 16385)
            assert answer.status == 413
            answer = await request(port, "GET", "/", headers="X-Long: " + "a" * 17000 + "\r\n")
            assert answer.status == 431
            answer = await request(port, "GET", "/", headers="Transfer-Encoding: chunked\r\n")
            assert answer.status == 400
            answer = await request(port, "GET", "/", headers="Content-Length: 1\r\nContent-Length: 2\r\n")
            assert answer.status == 400
            for bad in (b"GET /\r\n\r\n", b"GET / HTTP/2.0\r\n\r\n", b"GET http://x/ HTTP/1.1\r\n\r\n", b"get / HTTP/1.1\r\n\r\n"):
                answer, _ = await raw(port, bad)
                assert answer.status == 400, bad
                assert answer.header("Content-Security-Policy") == CSP_TEXT
            # A connection that sends nothing and closes gets nothing.
            reader, writer = await asyncio.open_connection("127.0.0.1", port)
            writer.close()

    asyncio.run(go())


def test_one_request_per_connection(static):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:
            reader, writer = await asyncio.open_connection("127.0.0.1", live.http_port)
            one = b"GET / HTTP/1.1\r\nHost: x\r\nConnection: keep-alive\r\n\r\n"
            writer.write(one + one)
            await writer.drain()
            text = await asyncio.wait_for(reader.read(), 5)
            writer.close()
            assert text.count(b"HTTP/1.1 200 OK") == 1

    asyncio.run(go())


def test_routes_later_parts_add(static):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:
            seen = []

            async def form(req: rt_http.Request) -> Response:
                seen.append((req.method, req.body, req.header("content-type")))
                return rt_http.text_response(303, "", [("Location", "/done")])

            def page(req: rt_http.Request) -> Response:
                return Response(200, b"<p>key</p>", [("Content-Type", "text/html; charset=utf-8")])

            http_app = live.http_app
            http_app.route("POST", "/key", form)
            http_app.route("GET", "/key", page)
            port = live.http_port
            answer = await request(
                port, "POST", "/key", headers="Content-Type: application/x-www-form-urlencoded\r\n", body=b"a=1"
            )
            assert answer.status == 303 and answer.header("Location") == "/done"
            assert answer.header("Content-Security-Policy") == CSP_TEXT
            assert seen == [("POST", b"a=1", "application/x-www-form-urlencoded")]
            answer = await request(port, "GET", "/key")
            assert answer.status == 200 and answer.body == b"<p>key</p>"
            answer = await request(port, "HEAD", "/key")
            assert answer.status == 200 and answer.body == b""
            answer = await request(port, "PUT", "/key")
            assert answer.status == 405 and answer.header("Allow") == "GET, HEAD, POST"

    asyncio.run(go())


def test_a_handler_that_raises_answers_500_and_logs_only_the_type(static, caplog):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:

            def broken(req: rt_http.Request) -> Response:
                raise RuntimeError("secret detail K4ABC")

            live.http_app.route("GET", "/broken", broken)
            answer = await request(live.http_port, "GET", "/broken")
            assert answer.status == 500
            assert answer.body == b"Something went wrong answering that request.\n"
            assert answer.header("Content-Security-Policy") == CSP_TEXT
            # The server goes on answering.
            assert (await request(live.http_port, "GET", "/index.html")).status == 200

    with caplog.at_level("INFO", logger="nereus_roundtable"):
        asyncio.run(go())
    said = [r.getMessage() for r in caplog.records]
    assert "an HTTP request failed (RuntimeError)" in said
    assert not any("secret" in m or "K4ABC" in m for m in said)


def test_the_read_api_over_the_socket_and_its_address_groups(static):
    async def go():
        async with live_service(http_app=True, static_dir=static) as live:
            port = live.http_port
            answer = await request(port, "GET", "/v1/api/rooms", headers="X-Forwarded-For: 203.0.113.5\r\n")
            assert answer.status == 200
            assert answer.header("Content-Type") == "application/json; charset=utf-8"
            assert answer.header("Access-Control-Allow-Origin") == "*"
            assert answer.header("Cache-Control") == "no-store"
            assert answer.header("Content-Security-Policy") == CSP_TEXT
            assert json.loads(answer.body) == {"time": live.clock.wall_ms(), "rooms": []}
            # The loopback peer is a trusted proxy: the forwarded address is
            # the group, so one network's 60 do not hold another's.
            for _ in range(59):
                assert (await request(port, "GET", "/v1/api/rooms", headers="X-Forwarded-For: 203.0.113.5\r\n")).status == 200
            answer = await request(port, "GET", "/v1/api/rooms", headers="X-Forwarded-For: 203.0.113.5\r\n")
            assert answer.status == 429 and int(answer.header("Retry-After")) >= 1
            answer = await request(port, "GET", "/v1/api/rooms", headers="X-Forwarded-For: 203.0.113.6\r\n")
            assert answer.status == 200
            answer = await request(port, "DELETE", "/v1/api/rooms")
            assert answer.status == 405
            assert json.loads(answer.body) == {"error": "not-allowed", "reason": "Only GET and HEAD are supported."}

    asyncio.run(go())
