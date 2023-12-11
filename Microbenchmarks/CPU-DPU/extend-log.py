#!/usr/bin/env python3

import dfatool.utils
import json
import sys


def read_logfile(filename):
    lf = dfatool.utils.Logfile()

    if filename.endswith("xz"):
        import lzma

        with lzma.open(filename, "rt") as f:
            return lf.load(f)
    with open(filename, "r") as f:
        return lf.load(f)


def write_logfile(filename, data):
    lf = dfatool.utils.Logfile()
    with open(filename, "w") as f:
        lf.dump(data, f)


entry_seen = dict()


def mpstat_to_dfatool(mpstat, in_param, in_attr):
    ret = list()

    try:
        cpu_attr = list(
            mpstat["sysstat"]["hosts"][0]["statistics"][0]["cpu-load"][0].keys()
        )
    except IndexError:
        return None

    cpu_attr.pop(cpu_attr.index("cpu"))

    entries = mpstat["sysstat"]["hosts"][0]["statistics"][1:]
    ncpu = mpstat["sysstat"]["hosts"][0]["number-of-cpus"]

    for entry in entries:
        nle99 = 0
        nle95 = 0
        nle90 = 0
        total = dict()
        for attr in cpu_attr:
            total[attr] = 0
        for cpu in entry["cpu-load"]:
            if cpu["cpu"] == "all":
                continue
            if cpu["idle"] <= 99:
                nle99 += 1
            if cpu["idle"] <= 95:
                nle95 += 1
            if cpu["idle"] <= 90:
                nle90 += 1
            for attr in cpu_attr:
                total[attr] += cpu[attr] / 100
        rp = {
            "nle99": nle99,
            "nle95": nle95,
            "nle90": nle90,
        }
        rp.update(total)
        ret.append(
            {"name": "transfer overhead UPMEM", "param": in_param, "attribute": rp}
        )

    return ret


def extend_entry(entry, indir):
    e_mode = entry["param"]["e_mode"]
    n_dpus = entry["param"]["n_dpus"]
    n_elements = entry["param"]["n_elements"]
    mpstat_filename = (
        f"{indir}/n_dpus={n_dpus}-e_mode={e_mode}-n_elements={n_elements}.json"
    )

    seen_key = (e_mode, n_dpus, n_elements)

    if entry["name"] == "NMC reconfiguration":
        return None

    if seen_key in entry_seen:
        return None

    entry_seen[seen_key] = True

    try:
        with open(mpstat_filename, "r") as f:
            strdata = f.read()
    except FileNotFoundError:
        return None

    try:
        data = json.loads(strdata)
        return mpstat_to_dfatool(data, entry["param"], entry["attribute"])
    except json.decoder.JSONDecodeError:
        strdata += "]}]}}"

    try:
        data = json.loads(strdata)
        entry_seen[seen_key] = True
        return mpstat_to_dfatool(data, entry["param"], entry["attribute"])
    except json.decoder.JSONDecodeError:
        print(f"Invalid JSON in {mpstat_filename}")
        return None


def main(infile, indir, outfile):
    indata = read_logfile(infile)
    outdata = list()

    for entry in indata:
        if extended_entry := extend_entry(entry, indir):
            outdata.extend(extended_entry)

    write_logfile(outfile, outdata)


if __name__ == "__main__":
    main(*sys.argv[1:])
