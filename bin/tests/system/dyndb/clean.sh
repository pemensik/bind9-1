#!/bin/sh
#
# Copyright (C) 2015, 2016  Internet Systems Consortium, Inc. ("ISC")
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Clean up after dyndb tests.
#
rm -f ns1/named.memstats
rm -f ns1/update.txt
rm -f added.a.out.*
rm -f added.ptr.out.*
rm -f deleted.a.out.*
rm -f deleted.ptr.out.*
