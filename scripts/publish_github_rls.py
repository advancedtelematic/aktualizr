#!/usr/bin/env python3

import json
import mimetypes
import os
import os.path
import re
import sys
import time
import urllib.request


def urlopen_retry(req):
    delay = 1
    last_exc = Exception()
    for k in range(5):
        try:
            return urllib.request.urlopen(req)
        except urllib.error.HTTPError as e:
            if e.code < 500:
                raise
            last_exc = e
        time.sleep(delay)
        delay *= 2
    raise last_exc


def main():
    if len(sys.argv) < 2:
        print("usage: {} RLS_TAG [assets]".format(sys.argv[0]))

    rls_tag = sys.argv[1]
    if "GITHUB_API_TOKEN" not in os.environ:
        raise RuntimeError("Please define $GITHUB_API_TOKEN")
    api_token = os.environ["GITHUB_API_TOKEN"]
    files = sys.argv[2:]

    req = urllib.request.Request(
        "https://api.github.com/repos/advancedtelematic/aktualizr/releases/tags/{}".format(
            rls_tag
        ),
        headers={
            "Accept": "application/vnd.github.v3+json",
            "Authorization": "token {}".format(api_token),
            "Content-Type": "application/json",
        },
        method="GET",
    )
    try:
        with urlopen_retry(req) as f:
            json.loads(f.read())
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise
    else:
        print("release already exist, nothing to do...")
        return 0

    # create release
    c = {"tag_name": rls_tag, "name": rls_tag, "body": "", "draft": False}
    req = urllib.request.Request(
        "https://api.github.com/repos/advancedtelematic/aktualizr/releases",
        data=json.dumps(c).encode(),
        headers={
            "Accept": "application/vnd.github.v3+json",
            "Authorization": "token {}".format(api_token),
            "Content-Type": "application/json",
        },
        method="POST",
    )
    with urlopen_retry(req) as f:
        resp = json.loads(f.read())

    upload_url = re.sub("{.*}", "", resp["upload_url"])

    for fn in files:
        bn = os.path.basename(fn)
        url = upload_url + "?name={}".format(bn)
        with open(fn, "rb") as f:
            req = urllib.request.Request(
                url,
                data=f,
                headers={
                    "Accept": "application/vnd.github.v3+json",
                    "Authorization": "token {}".format(api_token),
                    "Content-Length": str(os.path.getsize(fn)),
                    "Content-Type": mimetypes.guess_type(bn)[0],
                },
                method="POST",
            )
            urlopen_retry(req)

    return 0


if __name__ == "__main__":
    sys.exit(main())
