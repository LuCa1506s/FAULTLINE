"""Internal HTTP worker for the model adapter's hard deadline."""

import json
import sys
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener

MAX_BODY = 131072


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def main() -> int:
    try:
        data = sys.stdin.buffer.read(MAX_BODY + 1)
        if len(data) > MAX_BODY:
            return 1
        envelope = json.loads(data)
        request = Request(envelope["url"], data=json.dumps(envelope["body"]).encode(), headers=envelope["headers"], method="POST")
        with build_opener(ProxyHandler({}), NoRedirect()).open(request, timeout=envelope["timeout"]) as response:
            output = response.read(MAX_BODY + 1)
        if len(output) > MAX_BODY:
            return 1
        sys.stdout.buffer.write(output)
        return 0
    except (KeyError, OSError, ValueError):
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
