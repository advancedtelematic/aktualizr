#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os


import gzip
from flask import Flask, send_from_directory, safe_join, Response

VECTOR_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'vectors')

app = Flask(__name__, static_folder=None, template_folder=None)


@app.route('/<path:path>')
def static(path):
    if path.endswith('.gz'):
        path = safe_join(VECTOR_DIR, path[:-3])
        with open(path, 'rb') as f:
            out = gzip.compress(f.read())
        return Response(response=out, headers={'Content-Type': 'application/gzip'})
    else:
        return send_from_directory(VECTOR_DIR, path)


def main():
    app.run(host='127.0.0.1', port=8080, debug=True)

if __name__ == '__main__':
    main()
