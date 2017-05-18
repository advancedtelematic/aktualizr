#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os


from flask import Flask, send_from_directory

VECTOR_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'vectors')


def main():
    app = Flask(__name__, static_folder=None, template_folder=None)

    @app.route('/<path:path>')
    def static(path):
        return send_from_directory(VECTOR_DIR, path)

    app.run(host='127.0.0.1', port=8080, debug=True)

if __name__ == '__main__':
    main()
