#
# Copyright (C) 2017 Jonathon Ogden < jeog.dev@gmail.com >
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNE A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see http://www.gnu.org/licenses. 
#

from distutils.core import setup, Extension

NAME = 'simpleorderbook'

setup_dict = {
    "name":NAME,
    "version":'0.4',
    "description": "financial-market orderbook and matching engine",
    "author":"Jonathon Ogden",
    "author_email":"jeog.dev@gmail.com"
} 

_cpp_sources = [
    "src/module_py.cpp",
    "src/orderbook_py.cpp",    
    "src/order_info_py.cpp",
    "src/callback_py.cpp", 
    "src/argparse_py.cpp",
    "src/strings_py.cpp",
    "src/advanced/advanced_order_py.cpp",
    "src/advanced/one_cancels_other_py.cpp",
    "src/advanced/one_triggers_other_py.cpp",
    "src/advanced/fill_or_kill_py.cpp",
    "src/advanced/bracket_py.cpp",
    "src/advanced/trailing_stop_py.cpp",
    "src/advanced/trailing_bracket_py.cpp"
]

_cpp_includes = [
    "../include", 
    "./include"
]

cpp_ext = Extension(
    NAME,
    sources = _cpp_sources,
    include_dirs = _cpp_includes,
    libraries=['SimpleOrderbook'],
    library_dirs=["../bin/release"],
    extra_compile_args=["-std=c++11", "-Wno-invalid-offsetof"],    
    undef_macros=["NDEBUG"], #internal DEBUG/NDEBUG checks should handle this
)

if __name__ == '__main__':
    setup( ext_modules=[cpp_ext], **setup_dict )    

   
