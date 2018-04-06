#!/usr/bin/python

import os

os.setgid(99)
os.setgroups([99])
os.setuid(99)
os.system("/bin/bash")
