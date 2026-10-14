#!/usr/bin/env python3
"""Bounded observation of the live player's GrounderFBBIK; offline compile by default.

--launch starts a separate original with a launch-only shim. Never attaches to
an existing process. The operator arms after loading and supplies all inputs:
walk and stand on slopes, steps and ledges. The probe subscribes to the
grounder's own pre/post delegates and repeats its rays as read-only queries;
loading a component does affect process/performance.
"""
from pathlib import Path
import sys
import reference_fbbik_probe

SOURCE = Path(__file__).resolve().parent / 'ReferenceGrounderProbe.cs'


def main(argv=None):
    return reference_fbbik_probe.main(argv, source=SOURCE, label='grounder', hash_variable='PE_PROBE_SHA256', frames=3600, description=__doc__)


if __name__ == '__main__':
    sys.exit(main())
