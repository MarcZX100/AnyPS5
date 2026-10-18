import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

DEVICE_LIMITATION = re.compile(r"skipped,.*(subgroup|the device (has no|reports|lacks|does not|imports no|flushes|binds fewer)|no usable Vulkan device|host imports are unavailable)", re.IGNORECASE)

NO_DISPLAY = re.compile(r"skipped,.*no display", re.IGNORECASE)

HIGH_REFRESH_LIMITATION = re.compile(r"current display does not provide 120 Hz", re.IGNORECASE)
HIGH_REFRESH_STARTUP_LOGS = (
    re.compile(r"Surface capabilities drawable=\d+x\d+ min=\d+x\d+ max=\d+x\d+ minImages=\d+ maxImages=\d+ usage=0x[0-9a-f]+", re.IGNORECASE),
    re.compile(r"\[src/VulkanDevice\.cpp:\d+ VulkanDevice\] Swapchain created swapchain=0x[0-9a-f]+ extent=\d+x\d+ images=\d+", re.IGNORECASE),
    re.compile(r"\[shader-disk-cache\] .+ \(source version [0-9a-f]+, format \d+\)", re.IGNORECASE),
)
SKIP_RETURN_CODE = re.compile(r"SKIP_RETURN_CODE=\d+")

WINDOWS_PREFIX = "agc"


def high_refresh_limitation(output):
    found_limitation = False
    for line in output.splitlines():
        message = line.strip()
        if not message:
            continue
        if message.lower().startswith("skipped,"):
            message = message[len("skipped,"):].strip()
        if HIGH_REFRESH_LIMITATION.fullmatch(message):
            found_limitation = True
        elif not any(pattern.fullmatch(message) for pattern in HIGH_REFRESH_STARTUP_LOGS):
            return False
    return found_limitation


def skips(path):
    for case in ET.parse(path).getroot().iter("testcase"):
        if case.get("status") == "disabled":
            raise ValueError(f"{path}: testcase {case.get('name')!r} is disabled and was not run")
        skipped = case.find("skipped")
        if skipped is None:
            continue
        name = case.get("name")
        if not name:
            raise ValueError(f"{path}: skipped testcase has no name")
        output = ["skipped, " + reason.strip() for reason in (skipped.get("message", ""), skipped.text or "")
                  if reason.strip() and not SKIP_RETURN_CODE.fullmatch(reason.strip())]
        output += [case.findtext(channel) or "" for channel in ("system-out", "system-err")]
        yield name, "\n".join(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="fail when a test is skipped that the job's device is not expected to skip")
    parser.add_argument("junit", type=Path, help="the file written by ctest --output-junit")
    parser.add_argument("--job", choices=("linux", "windows"), default="linux")
    parser.add_argument("--list", action="store_true", help="print the skipped tests and why the file says they skipped")
    args = parser.parse_args()
    try:
        found = sorted(skips(args.junit))
    except (OSError, ET.ParseError, ValueError) as error:
        parser.error(str(error))
    if args.list:
        for name, output in found:
            reason = next((line.strip() for line in output.splitlines() if line.startswith("skipped")), "")
            print(f"{name}: {reason}")
        raise SystemExit
    if args.job == "windows":
        unexpected = [name for name, output in found if not name.startswith(WINDOWS_PREFIX) and not NO_DISPLAY.search(output) and not high_refresh_limitation(output)]
        expected = f"any {WINDOWS_PREFIX}* test, which the runner has no Vulkan driver for, or a test that reports no display or Vulkan device or a display without a 120 Hz mode"
    else:
        unexpected = [name for name, output in found if not DEVICE_LIMITATION.search(output) and not high_refresh_limitation(output)]
        expected = "any test that reports the device limitation it needs, such as narrow subgroups, a missing extension or a display without a 120 Hz mode"
    for name in unexpected:
        print(f"error: {name} skipped")
    if unexpected:
        print(f"{len(unexpected)} test(s) skipped for a reason that is not expected: the job lost that coverage and ctest still reports every test as passed")
        raise SystemExit(1)
    print(f"{len(found)} test(s) skipped, all expected ({expected})")
