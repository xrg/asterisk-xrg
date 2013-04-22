#!/usr/bin/env python
# Asterisk -- An open source telephony toolkit.
#
# Copyright (C) 2013, Digium, Inc.
#
# David M. Lee, II <dlee@digium.com>
#
# See http://www.asterisk.org for more information about
# the Asterisk project. Please do not directly contact
# any of the maintainers of this project for assistance;
# the project provides a web site, mailing lists and IRC
# channels for your use.
#
# This program is free software, distributed under the terms of
# the GNU General Public License Version 2. See the LICENSE file
# at the top of the source tree.
#

try:
    import pystache
except ImportError:
    print >> sys.stderr, "Pystache required. Please sudo pip install pystache."

import os.path
import pystache
import sys

from asterisk_processor import AsteriskProcessor
from optparse import OptionParser
from swagger_model import *
from transform import Transform

TOPDIR = os.path.dirname(os.path.abspath(__file__))


def rel(file):
    """Helper to get a file relative to the script's directory

    @parm file: Relative file path.
    """
    return os.path.join(TOPDIR, file)

API_TRANSFORMS = [
    Transform(rel('res_stasis_http_resource.c.mustache'),
              'res_stasis_http_{{name}}.c'),
    Transform(rel('stasis_http_resource.h.mustache'),
              'stasis_http/resource_{{name}}.h'),
    Transform(rel('stasis_http_resource.c.mustache'),
              'stasis_http/resource_{{name}}.c', False),
]

RESOURCES_TRANSFORMS = [
    Transform(rel('stasis_http.make.mustache'), 'stasis_http.make'),
]


def main(argv):
    parser = OptionParser(usage="Usage %prog [resources.json] [destdir]")

    (options, args) = parser.parse_args(argv)

    if len(args) != 3:
        parser.error("Wrong number of arguments")

    source = args[1]
    dest_dir = args[2]
    renderer = pystache.Renderer(search_dirs=[TOPDIR], missing_tags='strict')
    processor = AsteriskProcessor()

    # Build the models
    base_dir = os.path.dirname(source)
    resources = ResourceListing().load_file(source, processor)
    for api in resources.apis:
        api.load_api_declaration(base_dir, processor)

    # Render the templates
    for api in resources.apis:
        for transform in API_TRANSFORMS:
            transform.render(renderer, api, dest_dir)
    for transform in RESOURCES_TRANSFORMS:
        transform.render(renderer, resources, dest_dir)

if __name__ == "__main__":
    sys.exit(main(sys.argv) or 0)
