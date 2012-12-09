#!/usr/bin/env python


import os
import subprocess
import json

# import pprint


class Variable:
    def __init__ (self, frame, name, type_, address, unit_size,
                  value):
        self.frame = frame
        self.name = name
        self.type_ = type_
        self.address = address
        self.unit_size = unit_size
        if type(value) == type([]):
            if value[-1] == '_dummy':
                self.value = value[: -1]
            else:
                self.value = list(value)
        else:
            self.value = value
    def close (self):
        self.frame = None
        self.value = None
    def repr (self):
        if isinstance(self.value, Pointer):
            r = self.value.repr()
        else:
            r = str(self.value)
        return r
    def __cmp__ (self, other):
        if self.type_ != other.type_ or \
           type(self.value) != type(other.value):
            return 1
        return self.value != other.value


class Pointer:
    def __init__ (self, address, var=None, index=None):
        self.address = address
        self.var = var
        self.index = index
    def repr (self):
        if self.address == 0:
            r = 'NULL'
        elif self.var is not None:
            r = '&%s.%s' % (self.var.frame.name(), self.var.name)
            if self.index is not None:
                r += '[%d]' % self.index
        else:
            r = '0x%x' % self.address
        return r
    def __cmp__ (self, other):
        if self.address != other.address:
            return 1
        if self.var is None and other.var is None:
            return 0
        if self.var is None or other.var is None:
            return 1
        if self.var.name != other.var.name:
            return 1
        if self.index != other.index:
            return 1
        return 0


class Frame:
    def __init__ (self, stack, function):
        self.stack = stack
        self.function = function
        self.variables = {}
    def close (self):
        self.stack = None
        for v in self.variables.itervalues():
            v.close()
        self.variables = {}
    def add_variable (self, var):
        self.variables[var.name] = var
    def name (self):
        if self.function == '':
            n = '__GLOBALS__'
        else:
            n = '%s()' % self.function
        return n
    def repr (self, old_frame):
        r = []
        var_names = self.variables.keys()
        var_names.sort()
        for vn in var_names:
            var = self.variables[vn]
            changed = old_frame is None or \
                      vn not in old_frame.variables or \
                      var != old_frame.variables[vn]
            prefix = [' ', '*'][changed]
            r.append('%s   %-32s %-16s %s' %
                     (prefix, vn, var.type_, var.repr()))
        return r


class CStack:
    def __init__ (self, picoc_data):
        self.frames = []
        frame = Frame(self, '')
        for v in picoc_data.get('vars', [])[:-1]:
            if v['function'] != frame.function:
                frame = Frame(self, v['function'])
                self.frames.insert(0, frame)
            if frame.function == '' and v['name'] == '__exit_value':
                continue
            var = Variable(frame, v['name'], v['type'], v['address'],
                                  v['unit_size'], v['value'])
            frame.add_variable(var)
        self._resolve_pointers()
    def close (self):
        for f in self.frames:
            f.close()
        self.frames = []
    def _find_pointer (self, p):
        for frame in self.frames:
            for var in frame.variables.itervalues():
                if type(var.value) == type([]):
                    q, r = divmod(p - var.address, var.unit_size)
                    if q >= 0 and q < len(var.value):
                        if r == 0:
                            return Pointer(p, var=var, index=q)
                        else:
                            return Pointer(p)
                else:
                    if p == var.address:
                        return Pointer(p, var=var)
        return Pointer(p)
    def _resolve_pointers (self):
        for frame in self.frames:
            for var in frame.variables.itervalues():
                if var.type_ == 'Pointer':
                    if type(var.value) == type([]):
                        for i in xrange(len(var.value)):
                            var.value[i] = \
                              self._find_pointer(var.value[i])
                    else:
                        var.value = self._find_pointer(var.value)
    def repr (self, old_stack):
        r = []
        functions_match = True
        for i, frame in enumerate(self.frames):
            functions_match = \
                functions_match and \
                i < len(old_stack.frames) and \
                frame.function == old_stack.frames[i].function
            prefix = ['*', ' '][functions_match]
            r.append('%s %s' % (prefix, frame.name()))
            old_frame = None
            if functions_match:
                old_frame = old_stack.frames[i]
            r.extend(frame.repr(old_frame))
        return r


class CCode:
    def __init__ (self, filename):
        f = open(filename, 'r')
        self.c_code = f.readlines()
        f.close()
    def close (self):
        pass
    def context (self, line_num, column, n_context_lines=6):
        context_lines = []
        begin = line_num - n_context_lines
        end = line_num + n_context_lines + 1
        for i in xrange(begin, end):
            if i < 0 or i >= len(self.c_code):
                context_lines.append('\n')
            else:
                context_lines.append('  ' + self.c_code[i])
                if i == line_num:
                    context_lines.append('>-' + '-' * column + '^\n')
        return context_lines


def main (argv=None):

    import optparse

    if argv is None:
        argv = ['-']

    # default_boolean = False
    default_picoc_exe = './picoc'

    usage = '%prog [options] file.c'
    parser = optparse.OptionParser(usage=usage)
    # parser.add_option('-b', '--boolean',
    #                   action='store_true',
    #                   help='this boolean (default: %default)')
    parser.add_option('-p', '--picoc-exe',
                      help='picoc exe path (default: %default)')
    parser.set_defaults(
        # boolean=default_boolean
        picoc_exe=default_picoc_exe
    )
    options, args = parser.parse_args(args=argv[1:])

    if len(args) != 1:
        parser.print_help()
        return 1

    picoc_exe = options.picoc_exe
    c_filename = args[0]

    c_code = CCode(c_filename)

    p = subprocess.Popen([picoc_exe, c_filename],
                         stdin=subprocess.PIPE,
                         stdout=None,
                         stderr=subprocess.PIPE)

    old_stack = CStack({})
    while True:
        line = p.stderr.readline()
        try:
            data = json.loads(line)
        except ValueError:
            break
        stack = CStack(data)
        os.system('clear')
        print ''.join(c_code.context(data['line'], data['column']))
        print '\n'.join(stack.repr(old_stack))
        old_stack.close()
        old_stack = stack
        print
        raw_input('Hit [return] to continue...')
        try:
            p.stdin.write('\n')
        except IOError:
            break
    old_stack.close()

    p.wait()

    c_code.close()

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))
