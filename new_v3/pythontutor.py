# Online Python Tutor
# https://github.com/pgbovine/OnlinePythonTutor/
# 
# Copyright (C) 2010-2012 Philip J. Guo (philip@pgbovine.net)
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


# TODO: if we want to enable concurrent requests, then make sure this is threadsafe (e.g., no mutable globals)
# then add this string to app.yaml: 'threadsafe: true'

import webapp2
import pg_logger
import json
import jinja2, os
import sys
import mimetypes

# TODO: this croaks for some reason ...
TEST_STR = "import os\nos.chdir('/')"


JINJA_ENVIRONMENT = jinja2.Environment(loader=jinja2.FileSystemLoader(os.path.dirname(__file__)))


class TutorPage(webapp2.RequestHandler):

  def get(self):
    self.response.headers['Content-Type'] = 'text/html'
    template = JINJA_ENVIRONMENT.get_template('visualize.html')
    self.response.out.write(template.render())


class IframeEmbedPage(webapp2.RequestHandler):
  def get(self):
    self.response.headers['Content-Type'] = 'text/html'
    template = JINJA_ENVIRONMENT.get_template('iframe-embed.html')
    self.response.out.write(template.render())


class LessonPage(webapp2.RequestHandler):

  def get(self):
    self.response.headers['Content-Type'] = 'text/html'
    template = JINJA_ENVIRONMENT.get_template('lesson.html')
    self.response.out.write(template.render())


class ExecScript(webapp2.RequestHandler):

  def json_finalizer(self, input_code, output_trace):
    ret = dict(code=input_code, trace=output_trace)
    json_output = json.dumps(ret, indent=None) # use indent=None for most compact repr
    self.response.out.write(json_output)

  """
  def get(self):
    self.response.headers['Content-Type'] = 'application/json'
    self.response.headers['Cache-Control'] = 'no-cache'

    # convert from string to a Python boolean ...
    cumulative_mode = (self.request.get('cumulative_mode') == 'true')
    
    pg_logger.exec_script_str(self.request.get('user_script'),
                              cumulative_mode,
                              self.json_finalizer)

  """
  def get(self):
    import subprocess
    self.response.headers['Content-Type'] = 'application/json'
    self.response.headers['Cache-Control'] = 'no-cache'

    # convert from string to a Python boolean ...
    cumulative_mode = (self.request.get('cumulative_mode') == 'true')
    open("testing-interactive.c", "w").write(self.request.get('user_script'));
    picoc = os.path.join(os.getcwd(), "picoc")
    process = subprocess.Popen([picoc, "testing-interactive.c"], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    json_str = process.stderr.read()
    json_str = json_str.strip().split("\n")

    json_obj = {"code": open("testing-interactive.c").read()}
    json_obj["trace"] = [json.loads(i) for i in json_str if i.strip()]

    json_output = json.dumps(json_obj, indent=None)
    self.response.out.write(json_output)


class StaticFileHandler(webapp2.RequestHandler):
    def get(self, path):
        abs_path = os.path.abspath(path)
        if os.path.isdir(abs_path) or abs_path.find(os.getcwd()) != 0:
            self.response.set_status(403)
            return
        try:
            f = open(abs_path, 'r')
            self.response.headers['Content-Type'] = mimetypes.guess_type(abs_path)[0]
            self.response.out.write(f.read())
            f.close()
        except:
            self.response.set_status(404)


app = webapp2.WSGIApplication([('/', TutorPage),
                               ('/iframe-embed.html', IframeEmbedPage),
                               ('/lesson.html', LessonPage),
                               ('/exec', ExecScript),
                               (r'/(js/.+)', StaticFileHandler),
                               (r'/(css/.+)', StaticFileHandler),
                               (r'/(img/.+)', StaticFileHandler),
                               (r'/(example-code/.+)', StaticFileHandler),
                               (r'/(code/.+)', StaticFileHandler),
                               (r'/(lessons/.+)', StaticFileHandler),
                                ],
                               debug=True)

if __name__=="__main__":
    from paste.urlparser import StaticURLParser
    from paste.cascade import Cascade

    from paste import httpserver
    httpserver.serve(app, host='0.0.0.0', port='8001')

    """
    from wsgiref import simple_server
    http = simple_server.make_server('0.0.0.0', 8000, app)
    http.serve_forever()
    """
