from flask import Flask, request, Response, send_file

import os
import time
import random
import string
import json
from subprocess import call, Popen, PIPE

app = Flask(__name__)

app.debug = True

CWD = os.getcwd()
TMP = os.path.join(CWD, "tmp")

def get_random_str():
    return "".join([random.choice(string.letters) for i in range(1, 11)])

def get_random_name():
    fn = "%s_%s"%(time.strftime("%Y%m%d%H%M%S"), get_random_str())
    return os.path.join(TMP, fn)

def execute_code(filename, trace_file):
    trace = []

    proc = Popen(["./picoc", "-t", trace_file, filename],
        stderr=PIPE, cwd=CWD)
    (_, stderr) = proc.communicate()
    if proc.returncode != 0:
        stderr = stderr.split("\n")
        lineno = stderr[0]
        columnno = stderr[1]
        msg = "\n".join(stderr[2:])
        trace = [{"exception_msg": msg,
                  "line": lineno,
                  "offset": columnno,
                  "event": "error"
                }]
    else:
        tf = "%s.trace"%(trace_file)
        trace = [json.loads(i.strip()) for i in open(tf) if i.strip()]

    json_obj = {"code": open(filename).read(),
                "trace": trace
               }

    return json.dumps(json_obj)

@app.route('/exec')
def execute():
    trace_file = get_random_name()
    script = request.args.get('user_script')
    code_file = "%s.c"%(trace_file)
    open(code_file, "w").write(script)

    json_data = execute_code(code_file, trace_file)

    resp = Response(json_data, mimetype="application/json")
    resp.headers['Cache-Control'] = 'no-cache'
    return resp

@app.route('/')
def index():
    return send_file("static/visualize.html")

if __name__ == '__main__':
    app.run()
