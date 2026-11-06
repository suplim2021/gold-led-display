# PlatformIO pre-build script: OTA password handling.
#
# The OTA password is never stored in the repository or in the firmware.
#   * Source: environment variable GOLD_OTA_PASSWORD, or the first line of
#     ota_password.txt in the project root (git-ignored).
#   * The firmware only receives the MD5 hash (ArduinoOTA.setPasswordHash),
#     which is what ArduinoOTA's challenge/response protocol needs.
#   * No password / weak password: firmware builds with OTA DISABLED, and the
#     esp32dev_ota upload environment refuses to run.
#   * For the esp32dev_ota env the plain password is passed to espota.py
#     (--auth) only at upload time.
# Keep the rules in sync with gold::otaPasswordProblem() (lib/GoldCore).

import hashlib
import os

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

BANNED = {"goldled-ota", "goldprice", "password", "1234567890", "0123456789",
          "qwertyuiop", "gold-led-display"}


def read_password(project_dir):
    pw = os.environ.get("GOLD_OTA_PASSWORD", "")
    if pw:
        return pw.strip(), "environment variable GOLD_OTA_PASSWORD"
    path = os.path.join(project_dir, "ota_password.txt")
    if not os.path.isfile(path):
        return "", None
    raw = open(path, "rb").read()
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):  # PowerShell Out-File (UTF-16)
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8-sig")
    lines = text.splitlines()
    return (lines[0].strip() if lines else ""), "ota_password.txt"


def problem(pw):
    if not pw:
        return "empty"
    if len(pw) < 10:
        return "shorter than 10 characters"
    if len(pw) > 64:
        return "longer than 64 characters"
    if pw.lower() in BANNED:
        return "a well-known default"
    for ch in pw:
        if ord(ch) < 0x21 or ord(ch) > 0x7E or ch in '"\\':
            return "contains spaces, quotes, backslash or non-ASCII"
    return None


project_dir = env.subst("$PROJECT_DIR")
is_ota_upload = env.subst("$UPLOAD_PROTOCOL") == "espota"
password, source = read_password(project_dir)
reason = problem(password)

if reason is None:
    digest = hashlib.md5(password.encode("ascii")).hexdigest()
    env.Append(CPPDEFINES=[("OTA_PASSWORD_MD5", env.StringifyMacro(digest))])
    print("OTA: enabled (password from %s, firmware stores MD5 only)" % source)
    if is_ota_upload:
        env.Append(UPLOAD_FLAGS=["--auth=" + password])
else:
    msg = ("OTA: DISABLED - password %s. Create ota_password.txt (one line, "
           ">= 10 chars, no spaces/quotes) or set GOLD_OTA_PASSWORD, then flash "
           "once over USB." % (reason if source else "not set"))
    print(msg)
    if is_ota_upload:
        def _refuse(*_args, **_kwargs):
            print("ERROR: " + msg)
            env.Exit(1)  # noqa: F821
        env.AddPreAction("upload", _refuse)
