#!/usr/bin/env python3
"""Generate daily Rainus CSV logs for testing."""

import argparse
import datetime as dt
import os
from pathlib import Path
import random
import sys


CHIP_ID = "CDAB207B0FBCFCE239E339E3"
HEADER = "chip_id,event_unix_utc,build_unix_utc\r\n"
SECONDS_PER_DAY = 86400


def nonnegative_int(value):
    try:
        number = int(value)
        if number < 0:
            raise ValueError
        return number
    except ValueError:
        raise argparse.ArgumentTypeError("must be a non-negative integer")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", type=nonnegative_int, help="number of files to create")
    parser.add_argument("entries", type=nonnegative_int, help="number of log entries per file")
    parser.add_argument("directory", type=Path, help="existing destination directory")
    args = parser.parse_args()

    if not args.directory.is_dir():
        parser.error("destination must be an existing directory")

    # Keep whole UTC days within the firmware's unsigned 32-bit Unix clock,
    # leaving room for a build time before all events.
    first_day = dt.date(2020, 1, 2)
    last_day = dt.date(2106, 2, 6)
    days = (last_day - first_day).days + 1
    available = [
        first_day + dt.timedelta(days=offset)
        for offset in range(days)
        if not os.path.lexists(args.directory / (first_day + dt.timedelta(days=offset)).strftime("%Y%m%d.csv"))
    ]
    if args.files > len(available):
        parser.error("only {} unused dates are available in the destination".format(len(available)))

    dates = sorted(random.sample(available, args.files))
    if not dates:
        return
    build_time = int(dt.datetime.combine(dates[0] - dt.timedelta(days=1), dt.time(), dt.timezone.utc).timestamp())

    for day in dates:
        path = args.directory / day.strftime("%Y%m%d.csv")
        day_start = int(dt.datetime.combine(day, dt.time(), dt.timezone.utc).timestamp())
        # Exclusive creation prevents overwriting a file created since the scan.
        with path.open("x", encoding="ascii", newline="", buffering=1024 * 1024) as output:
            output.write(HEADER)
            for index in range(args.entries):
                # One random position per ordered interval gives sorted events, even
                # when there are more events than seconds in a day.
                event_time = day_start + (index * SECONDS_PER_DAY + random.randrange(SECONDS_PER_DAY)) // args.entries
                output.write("{},{},{}\r\n".format(CHIP_ID, event_time, build_time))
        print("Created {} ({} entries)".format(path, args.entries), flush=True)


if __name__ == "__main__":
    try:
        main()
    except OSError as error:
        print("Error: {}".format(error), file=sys.stderr)
        sys.exit(1)
