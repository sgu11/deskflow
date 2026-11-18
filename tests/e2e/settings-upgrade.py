#!/usr/bin/env python3
"""Verify settings migration through real GUI startup in an isolated profile.

The GUI's --version path initializes settings before exiting. Offscreen mode
and private XDG directories avoid launching a core or changing the desktop.
"""

import argparse
import configparser
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def read_settings(path):
    settings = configparser.RawConfigParser(strict=False)
    settings.optionxform = str
    settings.read(path)
    return {section: dict(settings[section]) for section in settings.sections()}


def verify(gui, root, label, contents, expected):
    profile = root / label
    config = profile / "config" / "Deskflow" / "Deskflow.conf"
    config.parent.mkdir(parents=True)
    config.write_text("[core]\ncomputerName=settings-e2e\nlanguage=en\n" + contents)
    environment = os.environ.copy()
    environment.update(QT_QPA_PLATFORM="offscreen", XDG_CONFIG_HOME=str(profile / "config"),
                       XDG_STATE_HOME=str(profile / "state"))
    for attempt in range(2):
        result = subprocess.run([str(gui), "--version"], env=environment, capture_output=True,
                                text=True, timeout=15)
        if result.returncode:
            raise RuntimeError(f"{label}: GUI startup failed ({result.returncode})\n{result.stderr}")
        actual = read_settings(config)
        assert not any(section.startswith("screen_") for section in actual), (label, actual)
        for section, values in expected.items():
            for key, value in values.items():
                assert actual.get(section, {}).get(key) == value, (label, section, key, value, actual)
        if attempt:
            assert actual == previous, (label, "second startup changed migrated settings", previous, actual)
        previous = actual
    print(f"PASS {label}: startup migration and repeat startup", flush=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("gui", type=Path, help="built Deskflow GUI executable")
args = parser.parse_args()
if sys.platform not in ("darwin", "linux"):
    parser.error("This check requires macOS or Linux with XDG settings isolation")
gui = args.gui.resolve(strict=True)
with tempfile.TemporaryDirectory(prefix="deskflow-settings-e2e-") as directory:
    root = Path(directory)
    verify(gui, root, "screen-groups", "[screen_screen_study]\nname=screen_study\nswitchCornerSize=29\n",
           {"computer_screen_study": {"name": "screen_study", "switchCornerSize": "29"}})
    verify(gui, root, "existing-computer", "[screen_alpha]\nname=alpha\nswitchCornerSize=17\n"
           "[computer_alpha]\nname=alpha\nswitchCornerSize=31\n",
           {"computer_alpha": {"switchCornerSize": "31"}})
    verify(gui, root, "legacy-first-and-partial", "[internalConfig]\nscreens\\size=2\n"
           "screens\\1\\name=alpha\nscreens\\1\\switchCornerSize=23\n"
           "screens\\1\\fixArray\\size=1\nscreens\\1\\fixArray\\1\\fix=true\n"
           "screens\\2\\name=beta\n",
           {"computer_alpha": {"name": "alpha", "switchCornerSize": "23", "halfDuplexCapsLock": "true",
                               "halfDuplexNumLock": "false", "modifierShift": "shift"},
            "computer_beta": {"name": "beta", "switchCornerSize": "0", "modifierCtrl": "ctrl"}})
    verify(gui, root, "legacy-aliases", "[internalConfig]\nscreens\\size=1\n"
           "screens\\1\\name=alpha\nscreens\\1\\aliasArray\\size=2\n"
           "screens\\1\\aliasArray\\1\\alias=alpha-backup\n"
           "screens\\1\\aliasArray\\2\\alias=alpha-backup\n",
           {"computer_alpha": {"name": "alpha", "aliases": "alpha-backup"}})
