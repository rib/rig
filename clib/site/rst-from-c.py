#!/usr/bin/env python

# Copyright (C) 2015 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys
import clang.cindex
import argparse
import re

def pad_non_ptr(type):
    if type[-1] != "*":
        return type + " "
    else:
        return type

def format_arg(type, name):
    #XXX: Pretty crude way of handling function pointers...
    if "(*)(" in type:
        return type.replace("(*)(", "(*" + name + ")(")
    return pad_non_ptr(type) + name


parser = argparse.ArgumentParser()
parser.add_argument("files", nargs="+", help="Code to parse for gtk-doc comments")
parser.add_argument("--symbol-regex", default=".*", help="Filter what symbols to output reStructuredText templates for")
parser.add_argument("--cflags", default="-I../clib -std=c11", help="Compiler flags while parsing header")
parser.add_argument("--verbose", action="store_true", help="Show compiler diagnostics")

args = parser.parse_args()

cflags = args.cflags.split()

index = clang.cindex.Index.create()

gtk_doc_start = re.compile(r'^/\*\*')

functions = {}
macros = {}

sections = {}
comments = {}

try:
    filter = re.compile(args.symbol_regex)
except:
    print "Failed to parse symbol filter \" + args.symbol_filter + \""
    sys.exit(1)


def parse(file):
    if args.verbose:
        print ".. Running:"
        print "..   clang " + args.cflags + " " + file
        print ".. "

    tu = index.parse(file, args=cflags, options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

    if args.verbose:
        for diag in tu.diagnostics:
            print ".. " + str(diag)

    return tu

def super_subst(regex, reformatter, text, flags=0, identifier=""):
    while True:
        match = re.search(regex, text, flags)
        if match == None:
            return text
        replacement = reformatter(match, identifier)
        text = re.sub(regex, replacement, text, 1, flags)

def reformat_section(name, reformatter, text):
    #XXX: beware the hairy (?=...) lookahead for a newline
    #followed by a non-space char or the end of the string
    regex = "^" + name + ":(.*?(?=(\n\S|\Z)))"
    return super_subst(regex, reformatter, text, re.MULTILINE|re.DOTALL, name)

def empty(match, identifier):
    return ""

def remove_section(name, text):
    #XXX: beware the hairy (?=...) lookahead for a newline
    #followed by a non-space char or the end of the string
    regex = "^" + name + ":(.*?(?=(\n\S|\Z)))"
    return super_subst(regex, empty, text, re.MULTILINE|re.DOTALL)

def force_indent(comment, level):
    text = ""
    for line in comment.split('\n'):
        line = line.strip()
        text = text + "".rjust(level) + line + "\n"
    return text

def rshift(comment, level):
    text=""
    for line in comment.split('\n'):
        text = text + "".rjust(level) + line + "\n"
    return text

def ensure_indent(comment):
    for line in comment.split('\n'):
        if len(line) >= 1 and line[0] != " ":
            return rshift(comment, 2)
    return comment

def ensure_para_indent(comment):
    text = ""
    lines = comment.split('\n')
    if len(lines) > 1:
        m = re.match(r'^\s+', lines[1])
        if m != None:
            lines[0] = m.group(0) + lines[0].lstrip()
        for line in lines:
            text = text + line + "\n"
        return text
    else:
        return "  " + comment

def reformat_code(match, identifier):
    code = ensure_indent(match.group(1))
    return "::\n" + code

def reformat_note(match, identifier):
    note = force_indent(match.group(1), 8)
    return "\n.. note::\n" + note + "\n"

def reformat_return_value(match, identifier):
    return ":returns: " + ensure_indent(match.group(1))

def reformat_member(match, identifier):
    name = identifier[1:]
    return "\n.. c:member:: " + name + "\n\n" + ensure_para_indent(match.group(1))

def reformat_argument(match, identifier):
    name = identifier[1:]
    return ":param " + name + ":" + ensure_indent(match.group(1)) + "\n"

def reformat_short_description(match, identifier):
    return match.group(1).lstrip()

def strip_c_comment_markers(comment):
    plain=""
    for line in comment.split('\n')[2:]:
        line = line[3:]
        plain = plain + line + "\n"
    return plain

def subst_notes(comment):
    comment = super_subst(r'<note>(.*?)</note>', reformat_note, comment, re.MULTILINE|re.DOTALL)
    comment = super_subst("Note: (.*?)(\n\n|\Z)", reformat_note, comment, re.MULTILINE|re.DOTALL)
    return comment

def subst_code(comment):
    return super_subst(r'\|\[(.*?)\]\|', reformat_code, comment, re.MULTILINE|re.DOTALL)

def normalize_empty_lines(comment):
    text = ""
    for line in comment.split('\n'):
        text = text + line.rstrip() + "\n"
    return text

def reformat_itemized_list(match, identifier):
    rst = match.group(1)
    for i in range(1, 100):
        rst = re.sub(r'<listitem>(.+?)</listitem>', str(i) + ". \\1", rst, 1, re.MULTILINE|re.DOTALL)
    return rst

def subst_common(rst):
    rst = re.sub(r'<emphasis>(.*?)</emphasis>', "*\\1*", rst)
    rst = re.sub(r'<literal>(.*?)</literal>', "``\\1``", rst)
    rst = re.sub(r'%([a-zA-Z_]+[a-zA-Z0-9_]*)', "``\\1``", rst)
    rst = re.sub(r'#([a-zA-Z_]+[a-zA-Z0-9_]*)', ":c:type:`\\1`", rst)

    rst = re.sub(r'<ulink\s+url="(.+?)"\s*/>', r'\1', rst)
    rst = re.sub(r'<ulink\s+url="(.+?)">(.+?)</ulink>', r'`\2 <\1>`_', rst)
    rst = super_subst(r'<itemizedlist>(.*?)</itemizedlist>', reformat_itemized_list, rst, re.MULTILINE|re.DOTALL)

    rst = subst_notes(rst)
    rst = subst_code(rst)

    return rst

def parse_section_comment(section, comment):
    rst = strip_c_comment_markers(comment)

    rst = reformat_section("@short_description", reformat_short_description, rst)

    rst = subst_common(rst)

    rst = normalize_empty_lines(rst)

    print rst


def parse_type_comment(type, comment):

    rst = strip_c_comment_markers(comment)

    rst = subst_common(rst)

    while True:
        match = re.search(r'^(@[a-zA-Z_]+[a-zA-Z0-9_]*):', rst, re.MULTILINE)
        if match == None:
            break;
        name = match.group(1)
        rst = reformat_section(name, reformat_member, rst)

    rst = ".. c:type:: " + type + "\n" + rshift(rst, 8)

    rst = normalize_empty_lines(rst)

    print rst

def parse_comment(comment):
    if not gtk_doc_start.match(comment):
        return

    lines = comment.split('\n')
    sym = lines[1].split(':')[0][3:]

    if sym == "SECTION":
        parse_section_comment(lines[1].split(':')[1], comment)
        return

    if sym[-2:] == "_t":
        parse_type_comment(sym, comment)
        return

    if not filter.match(sym):
        return

    comment = strip_c_comment_markers(comment)

    rst = comment

    #Replace @foos that are function arguments
    while True:
        match = re.search(r'^(@[a-zA-Z_]+[a-zA-Z0-9_]*):', rst, re.MULTILINE)
        if match == None:
            break;
        name = match.group(1)
        rst = reformat_section(name, reformat_argument, rst)

    #Assume remaining @foo tokens are :c:data
    rst = re.sub(r'@([a-zA-Z_]+[a-zA-Z0-9_]*)', ":c:data:`\\1`", rst)

    rst = subst_common(rst)

    rst = reformat_section("Return value", reformat_return_value, rst)
    rst = reformat_section("Returns", reformat_return_value, rst)

    rst = remove_section("Stability", rst)

    #Finally indent everything by 8
    rst = rshift(rst, 8)
    rst = normalize_empty_lines(rst)


    comments[sym] = rst


print "\n.. highlight:: c\n"


# First index gtk-doc comments
for file in args.files:
    tu = parse(file)
    for tok in tu.cursor.get_tokens():
        if tok.kind == clang.cindex.TokenKind.COMMENT:
            parse_comment(tok.spelling)

for file in args.files:
    tu = parse(file)

    for child in tu.cursor.get_children():
        if child.kind == clang.cindex.CursorKind.FUNCTION_DECL:
            func = child
            name = func.spelling
            if not filter.match(name):
                continue
            line = "\n\n.. c:function:: " + pad_non_ptr(func.result_type.spelling) + func.spelling + "("
            args = []
            for child2 in func.get_children():
                try:
                    if child2.kind == clang.cindex.CursorKind.PARM_DECL:
                        args.append(child2)
                except:
                    pass
            for arg in args[:-1]:
                line = line + format_arg(arg.type.spelling, arg.spelling) + ", "
            if len(args):
                line = line + format_arg(args[-1].type.spelling, args[-1].spelling)
            if func.type.is_function_variadic():
                line = line + ", ..."
            line = line + ")"
            print line
            print ""
            if name in comments:
                print comments[name]
            else:
                for arg in args:
                    print "        :param " + arg.spelling + ": "
                    print "        :type " + arg.spelling + ": " + arg.type.spelling
                if func.result_type.spelling != "void":
                    print "        :rtype: " + func.result_type.spelling

        elif child.kind == clang.cindex.CursorKind.MACRO_DEFINITION:
            macro = child
            name = macro.spelling
            if not filter.match(name):
                continue
            line = "\n\n.. c:macro:: "
            for tok in macro.get_tokens():
                line = line + tok.spelling
                if tok.spelling == ")":
                    break
            print line

