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

from distutils.core import setup
import setup as _setup

_setup.setup_dict['version'] += " (DEBUG)"
_setup.setup_dict['description'] += " (DEBUG)"

_setup.cpp_ext.library_dirs = ["../Debug"]
_setup.cpp_ext.extra_compile_args += ["-O0"]
_setup.cpp_ext.define_macros = [("DEBUG",None)]

if __name__ == '__main__':
    setup( ext_modules=[_setup.cpp_ext], **_setup.setup_dict )    


    
