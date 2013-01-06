/*

Online Python Tutor
https://github.com/pgbovine/OnlinePythonTutor/

Copyright (C) 2010-2012 Philip J. Guo (philip@pgbovine.net)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/


// Pre-reqs: pytutor.js and jquery.ba-bbq.min.js should be imported BEFORE this file


// backend scripts to execute (Python 2 and 3 variants, if available)
//var python2_backend_script = 'web_exec_py2.py';
//var python3_backend_script = 'web_exec_py3.py';

// uncomment below if you're running on Google App Engine using the built-in app.yaml
var python2_backend_script = 'exec';
var python3_backend_script = null;

var appMode = 'edit'; // 'edit' or 'display'

var preseededCode = null;     // if you passed in a 'code=<code string>' in the URL, then set this var
var preseededCurInstr = null; // if you passed in a 'curInstr=<number>' in the URL, then set this var


var myVisualizer = null; // singleton ExecutionVisualizer instance

var keyStuckDown = false;

function enterEditMode() {
  $.bbq.pushState({ mode: 'edit' }, 2 /* completely override other hash strings to keep URL clean */);
}


var pyInputCodeMirror; // CodeMirror object that contains the input text

function setCodeMirrorVal(dat) {
  pyInputCodeMirror.setValue(dat.rtrim() /* kill trailing spaces */);
  $('#urlOutput,#embedCodeOutput').val('');

  // also scroll to top to make the UI more usable on smaller monitors
  $(document).scrollTop(0);
}


$(document).ready(function() {

  $("#embedLinkDiv").hide();

  pyInputCodeMirror = CodeMirror(document.getElementById('codeInputPane'), {
    mode: 'text/x-csrc',
    lineNumbers: true,
    tabSize: 4,
    indentUnit: 4,
    // convert tab into four spaces:
    extraKeys: {Tab: function(cm) {cm.replaceSelection("    ", "end");}}
  });

  pyInputCodeMirror.setSize(null, '420px');



  // be friendly to the browser's forward and back buttons
  // thanks to http://benalman.com/projects/jquery-bbq-plugin/
  $(window).bind("hashchange", function(e) {
    appMode = $.bbq.getState('mode'); // assign this to the GLOBAL appMode

    if (appMode === undefined || appMode == 'edit') {
      $("#pyInputPane").show();
      $("#pyOutputPane").hide();
      $("#embedLinkDiv").hide();

      // destroy all annotation bubbles (NB: kludgy)
      if (myVisualizer) {
        myVisualizer.destroyAllAnnotationBubbles();
      }
    }
    else if (appMode == 'display') {
      $("#pyInputPane").hide();
      $("#pyOutputPane").show();

      $("#embedLinkDiv").show();

      $('#executeBtn').html("Visualize execution");
      $('#executeBtn').attr('disabled', false);


      // do this AFTER making #pyOutputPane visible, or else
      // jsPlumb connectors won't render properly
      myVisualizer.updateOutput();

      // customize edit button click functionality AFTER rendering (NB: awkward!)
      $('#pyOutputPane #editCodeLinkDiv').show();
      $('#pyOutputPane #editBtn').click(function() {
        enterEditMode();
      });
    }
    else {
      assert(false);
    }

    $('#urlOutput,#embedCodeOutput').val(''); // clear to avoid stale values
  });


  $("#executeBtn").attr('disabled', false);
  $("#executeBtn").click(function() {

    var backend_script = null;
    if ($('#pythonVersionSelector').val() == '2') {
        backend_script = python2_backend_script;
    }
    else if ($('#pythonVersionSelector').val() == '3') {
        backend_script = python3_backend_script;
    }

    if (!backend_script) {
      alert('Error: This server is not configured to run Python ' + $('#pythonVersionSelector').val());
      return;
    }

    $('#executeBtn').html("Please wait ... processing your code");
    $('#executeBtn').attr('disabled', true);
    $("#pyOutputPane").hide();
    $("#embedLinkDiv").hide();


    $.get(backend_script,
          {user_script : pyInputCodeMirror.getValue(),
           cumulative_mode: $('#cumulativeModeSelector').val()},
          function(dataFromBackend) {
            var trace = dataFromBackend.trace;
            
            // don't enter visualize mode if there are killer errors:
            if (!trace ||
                (trace.length == 0) ||
                (trace[trace.length - 1].event == 'uncaught_exception')) {

              if (trace.length == 1) {
                var errorLineNo = trace[0].line - 1; /* CodeMirror lines are zero-indexed */
                if (errorLineNo !== undefined) {
                  // highlight the faulting line in pyInputCodeMirror
                  pyInputCodeMirror.focus();
                  pyInputCodeMirror.setCursor(errorLineNo, 0);
                  pyInputCodeMirror.setLineClass(errorLineNo, null, 'errorLine');

                  pyInputCodeMirror.setOption('onChange', function() {
                    pyInputCodeMirror.setLineClass(errorLineNo, null, null); // reset line back to normal
                    pyInputCodeMirror.setOption('onChange', null); // cancel
                  });
                }

                alert(trace[0].exception_msg);
              }
              else if (trace[trace.length - 1].exception_msg) {
                alert(trace[trace.length - 1].exception_msg);
              }
              else {
                alert("Whoa, unknown error! Reload to try again, or report a bug to philip@pgbovine.net\n\n(Click the 'Generate URL' button to include a unique URL in your email bug report.)");
              }

              $('#executeBtn').html("Visualize execution");
              $('#executeBtn').attr('disabled', false);
            }
            else {
              var startingInstruction = 0;

              // only do this at most ONCE, and then clear out preseededCurInstr
              if (preseededCurInstr && preseededCurInstr < trace.length) { // NOP anyways if preseededCurInstr is 0
                startingInstruction = preseededCurInstr;
                preseededCurInstr = null;
              }

              myVisualizer = new ExecutionVisualizer('pyOutputPane',
                                                     dataFromBackend,
                                                     {startingInstruction:  startingInstruction,
                                                      updateOutputCallback: function() {$('#urlOutput,#embedCodeOutput').val('');},
                                                      //allowEditAnnotations: true,
                                                     });


              // set keyboard bindings
              $(document).keydown(function(k) {
                if (!keyStuckDown) {
                  if (k.keyCode == 37) { // left arrow
                    if (myVisualizer.stepBack()) {
                      k.preventDefault(); // don't horizontally scroll the display
                      keyStuckDown = true;
                    }
                  }
                  else if (k.keyCode == 39) { // right arrow
                    if (myVisualizer.stepForward()) {
                      k.preventDefault(); // don't horizontally scroll the display
                      keyStuckDown = true;
                    }
                  }
                }
              });

              $(document).keyup(function(k) {
                keyStuckDown = false;
              });


              // also scroll to top to make the UI more usable on smaller monitors
              $(document).scrollTop(0);

              $.bbq.pushState({ mode: 'display' }, 2 /* completely override other hash strings to keep URL clean */);
            }
          },
          "json");
  });

  /*
  $('#nonlocalLink').click(function() {
    $.get("example-code/nonlocal.txt", setCodeMirrorVal);
    return false;
  });
  */

  var fragment = $.deparam.fragment();
  if (fragment.filename){
    $.get("code/"+fragment.filename, setCodeMirrorVal);
    return false;
  }

  // handle hash parameters passed in when loading the page
  preseededCode = $.bbq.getState('code');
  if (preseededCode) {
    setCodeMirrorVal(preseededCode);
  }
  else {
    // select a canned example on start-up:
    $("#aliasExampleLink").trigger('click');
  }

  // ugh, ugly tristate due to the possibility of them being undefined
  var cumulativeState = $.bbq.getState('cumulative');
  if (cumulativeState !== undefined) {
    $('#cumulativeModeSelector').val(cumulativeState);
  }
  var pyState = $.bbq.getState('py');
  if (pyState !== undefined) {
    $('#pythonVersionSelector').val(pyState);
  }

  appMode = $.bbq.getState('mode'); // assign this to the GLOBAL appMode
  if ((appMode == "display") && preseededCode /* jump to display only with pre-seeded code */) {
    preseededCurInstr = Number($.bbq.getState('curInstr'));
    $("#executeBtn").trigger('click');
  }
  else {
    if (appMode === undefined) {
      // default mode is 'edit', don't trigger a "hashchange" event
      appMode = 'edit';
    }
    else {
      // fail-soft by killing all passed-in hashes and triggering a "hashchange"
      // event, which will then go to 'edit' mode
      $.bbq.removeState();
    }
  }

  
  // log a generic AJAX error handler
  $(document).ajaxError(function() {
    alert("Server error (possibly due to memory/resource overload). Report a bug to philip@pgbovine.net\n\n(Click the 'Generate URL' button to include a unique URL in your email bug report.)");

    $('#executeBtn').html("Visualize execution");
    $('#executeBtn').attr('disabled', false);
  });


  // redraw connector arrows on window resize
  $(window).resize(function() {
    if (appMode == 'display') {
      myVisualizer.redrawConnectors();
    }
  });

  $('#genUrlBtn').bind('click', function() {
    var myArgs = {code: pyInputCodeMirror.getValue(),
                  mode: appMode,
                  cumulative: $('#cumulativeModeSelector').val(),
                  py: $('#pythonVersionSelector').val()};

    if (appMode == 'display') {
      myArgs.curInstr = myVisualizer.curInstr;
    }

    var urlStr = $.param.fragment(window.location.href, myArgs, 2 /* clobber all */);
    $('#urlOutput').val(urlStr);
  });


  $('#genEmbedBtn').bind('click', function() {
    assert(appMode == 'display');
    var myArgs = {code: pyInputCodeMirror.getValue(),
                  cumulative: $('#cumulativeModeSelector').val(),
                  py: $('#pythonVersionSelector').val(),
                  curInstr: myVisualizer.curInstr,
                 };

    var embedUrlStr = $.param.fragment('http://pythontutor.com/iframe-embed.html', myArgs, 2 /* clobber all */);
    var iframeStr = '<iframe width="800" height="500" frameborder="0" src="' + embedUrlStr + '"> </iframe>';
    $('#embedCodeOutput').val(iframeStr);
  });
});

