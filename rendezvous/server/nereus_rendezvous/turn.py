# no-port-check: NereusSDR-original.
"""TURN credentials in coturn's time-limited form (use-auth-secret).

username = "<unix expiry>:<station id>"
password = base64(HMAC-SHA1(shared secret, username))

The secret is the bytes of coturn's static-auth-secret as written in its
configuration (UTF-8), the same bytes the service reads from its secret
file with the trailing line end removed. The password is standard base64
with padding, as coturn computes it.
"""

from __future__ import annotations

import base64
import hashlib
import hmac
from typing import Dict, List


def username_for(expires: int, station_id: str) -> str:
    return f"{int(expires)}:{station_id}"


def password_for(secret: bytes, username: str) -> str:
    digest = hmac.new(bytes(secret), username.encode("utf-8"), hashlib.sha1).digest()
    return base64.b64encode(digest).decode("ascii")


def mint(secret: bytes, station_id: str, now_seconds: int, ttl_seconds: int, urls: List[str]) -> Dict[str, object]:
    expires = int(now_seconds) + int(ttl_seconds)
    username = username_for(expires, station_id)
    return {
        "username": username,
        "password": password_for(secret, username),
        "expires": expires,
        "urls": list(urls),
    }
