# Writes a file into a C header as a byte array.
#   cmake -DIN=<file> -DOUT=<header> -DNAME=<symbol> [-DTEXT=ON] -P embed.cmake
#
# Copyright (c) 2026 Reda Cherif-Touil
# SPDX-License-Identifier: GPL-3.0-or-later
file(READ "${IN}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")   # "4142" -> "0x41,0x42,"
string(REGEX REPLACE "((0x..,){16})" "\\1\n    " bytes "${bytes}")     # 16 bytes per line
if(TEXT)
  string(APPEND bytes "0x00")                                         # ends like a C string
endif()
file(WRITE "${OUT}" "/* Generated from ${IN} - do not edit. */\n#pragma once\n"
                   "static const unsigned char ${NAME}[] = {\n    ${bytes}\n};\n")
