#!/usr/bin/env python
#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Outputs the warnings that are common to all builders.

Suppressed tests that are nonetheless passing are output as warnings
by vogar.  Any tests that generate warnings in every builder are good
candidates for no longer being suppressed, since they're passing on
a regular basis."""

import collections
import json
import requests

# The number of recent builds to check for each builder
NUM_BUILDS = 5
# The buildbot step to check for warnings
BUILDBOT_STEP = 'test libcore'


def main():
    # Dict from builder+build_num combination to the list of warnings
    # in that build
    warnings = collections.defaultdict(list)
    r = requests.get('https://build.chromium.org/p/client.art/json/builders')
    if r.status_code != 200:
        print r.text
        return
    builders = json.loads(r.text)
    for builder_name in sorted(builders):
        # Build -1 is the currently-running build (if there is one), so we
        # start with -2, which should be the most or second-most
        # recently-completed build.
        for build_num in range(-2, -2 - NUM_BUILDS, -1):
            print ('Loading data for %s, build %d...'
                   % (builder_name, build_num))
            r = requests.get(
                'https://build.chromium.org/p/client.art'
                '/json/builders/%s/builds/%d' % (
                builder_name, build_num))
            if r.status_code != 200:
                print r.text
                return
            builder = json.loads(r.text)
            libcore_steps = [x for x in builder['steps']
                             if x['name'] == BUILDBOT_STEP]
            for ls in libcore_steps:
                stdio_logs = [x for x in ls['logs'] if x[0] == 'stdio']
                for sl in stdio_logs:
                    # The default link is HTML, so append /text to get the
                    # text version
                    r = requests.get(sl[1] + '/text')
                    if r.status_code != 200:
                        print r.text
                        return
                    stdio = r.text.splitlines()

                    # Walk from the back of the list to find the start of the
                    # warnings summary
                    i = -1
                    try:
                        while not stdio[i].startswith('Warnings summary:'):
                            i -= 1
                        i += 1   # Ignore the "Warnings summary:" line
                        while i < -1:
                            warnings['%s:%d' % (builder_name, build_num)].append(stdio[i])
                            i += 1
                    except IndexError:
                        # Some builds don't have any
                        print '  No warnings section found.'
    # sharedwarnings will build up the intersection of all the lists of
    # warnings.  We seed it with an arbitrary starting point (which is fine
    # since intersection is commutative).
    sharedwarnings = set(warnings.popitem()[1])
    for warning_list in warnings.itervalues():
        sharedwarnings = sharedwarnings & set(warning_list)
    print 'Warnings shared across all builders:'
    for warning in sorted(list(sharedwarnings)):
        print warning


if __name__ == '__main__':
    main()
