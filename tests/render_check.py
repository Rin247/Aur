#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Render every editor page at the estate's sizes and sweep the zoom.

`python tests/render_check.py` writes one PNG per (page, size) into
`build/renders/` and prints a colour histogram for each, so the picture can be
looked at rather than assumed.

Derived from `Prometheus/tools/grab_window.py` (AGPL-3.0): the window
enumeration, the DPI-aware resize, the `PrintWindow` capture with the desktop
grab as its fallback and the flat-image retry came from there; the §5(a) notice
is the SPDX line above.

The app is driven through its own controls — `--tab`, `--zoom`, `--size` — so
the sweep changes what a person would change, not what the harness reaches
into. The zoom is a factor of the canvas's own fit, so 1.0 is the fit view.

    python tests/render_check.py            # the whole matrix
    python tests/render_check.py --quick    # one page, one size
"""
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time
from collections import Counter

from PIL import Image, ImageGrab

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(REPO, "build", "Release", "Aur.exe")
PROJECT = os.path.join(REPO, "projects", "sylvi")
OUT_DIR = os.path.join(REPO, "build", "renders")

# The estate's work area (DESIGN.md 12): 16:9 and 16:10.
SIZES = [(1280, 690), (1280, 770)]
# The pages that own a canvas, and the zoom control as a factor of the fit.
PAGES = [("map", "Map Editor"), ("world", "World"),
         ("pixel", "Pixel & Sprite Editor"), ("tileset", "Tileset Editor")]
ZOOM_SWEEP = [0.75, 1.5]

SETTLE = 4.0
user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
kernel32 = ctypes.windll.kernel32
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
TH32CS_SNAPPROCESS = 0x00000002
WM_NULL = 0x0000
SMTO_ABORTIFHUNG = 0x0002
PW_RENDERFULLCONTENT = 0x00000002
DIB_RGB_COLORS = 0

user32.SendMessageTimeoutW.restype = ctypes.c_size_t
user32.SendMessageTimeoutW.argtypes = [wt.HWND, wt.UINT, ctypes.c_size_t,
                                       ctypes.c_size_t, wt.UINT, wt.UINT,
                                       ctypes.POINTER(ctypes.c_size_t)]


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wt.DWORD), ("szExeFile", wt.WCHAR * 260)]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wt.DWORD), ("biWidth", ctypes.c_long), ("biHeight", ctypes.c_long),
        ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
        ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.DWORD),
        ("biYPelsPerMeter", wt.DWORD), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.DWORD * 3)]


def dpi_aware():
    DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ctypes.c_void_p(-4)
    try:
        if user32.SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2):
            return
        user32.SetProcessDPIAware()
    except AttributeError:
        user32.SetProcessDPIAware()


def find_window(pid):
    """The app's own window: visible, and the largest one that qualifies.

    A Qt app owns several helper windows (theme and screen change observers,
    a tooltip shadow) that carry a real rect but are not the window a person
    sees, so visibility and area are both checked.
    """
    found = []

    @WNDENUMPROC
    def cb(hwnd, _lparam):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            r = wt.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(r))
            w, h = r.right - r.left, r.bottom - r.top
            if w > 200 and h > 200:
                name = ctypes.create_unicode_buffer(256)
                user32.GetClassNameW(hwnd, name, 256)
                found.append((bool(user32.IsWindowVisible(hwnd)), w * h, hwnd,
                              "%s %dx%d" % (name.value, w, h)))
        return True

    user32.EnumWindows(cb, 0)
    for entry in found:
        print("render_check:   candidate %s visible=%s" % (entry[3], entry[0]))
    if not found:
        return None
    visible = [f for f in found if f[0]]
    pool = visible if visible else found
    return max(pool, key=lambda f: f[1])[2]


def resize(hwnd, w, h):
    HWND_TOP = 0
    SWP_NOMOVE = 0x0002
    SWP_NOZORDER = 0x0004
    SWP_SHOWWINDOW = 0x0040
    return bool(user32.SetWindowPos(hwnd, HWND_TOP, 0, 0, w, h,
                                    SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW))


def capture(hwnd, width, height):
    """Ask the window for its own content."""
    screen_dc = user32.GetDC(0)
    mem_dc = gdi32.CreateCompatibleDC(screen_dc)
    info = BITMAPINFO()
    info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    info.bmiHeader.biWidth = width
    info.bmiHeader.biHeight = -height          # negative: top-down, no flip
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = 0           # BI_RGB
    bits = ctypes.c_void_p()
    bmp = gdi32.CreateDIBSection(screen_dc, ctypes.byref(info), DIB_RGB_COLORS,
                                 ctypes.byref(bits), None, 0)
    if not bmp:
        gdi32.DeleteDC(mem_dc)
        user32.ReleaseDC(0, screen_dc)
        return None
    old = gdi32.SelectObject(mem_dc, bmp)
    ok = user32.PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT)
    buf = ctypes.string_at(bits, width * height * 4)
    img = Image.frombuffer("RGBA", (width, height), buf, "raw", "BGRA", 0, 1)
    gdi32.SelectObject(mem_dc, old)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(0, screen_dc)
    return img if ok else None


def distinct_colours(img):
    colours = img.convert("RGB").getcolors(maxcolors=1 << 20)
    return 1 << 20 if colours is None else len(colours)


def shoot(page, tab, size, zoom, sprite=False, preview=False):
    """One launch, one capture. Returns the PNG path or None."""
    w, h = size
    out = os.path.join(OUT_DIR, "%s_%dx%d_z%s.png" % (page, w, h, zoom))
    args = [EXE, PROJECT, "--tab", tab, "--size", "%dx%d" % (w, h), "--no-state"]
    if zoom != 1.0:
        args += ["--zoom", str(zoom)]
    if sprite:
        args += ["--sprite"]
    if preview:
        args += ["--preview"]
    proc = subprocess.Popen(args, cwd=os.path.dirname(EXE),
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("render_check: launch pid=%d %s" % (proc.pid, " ".join(args[1:])))
    hwnd = None
    try:
        time.sleep(SETTLE)
        deadline = time.time() + 60.0
        while time.time() < deadline:
            hwnd = find_window(proc.pid)
            if hwnd is not None:
                break
            time.sleep(1.0)
        if hwnd is None:
            print("render_check: no window for %s - the app may have failed to start" % tab)
            return None
        before = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(before))
        print("render_check: %s opened at %dx%d"
              % (tab, before.right - before.left, before.bottom - before.top))
        try:
            dpi = user32.GetDpiForWindow(hwnd) or 96
        except AttributeError:
            dpi = 96
        scale = dpi / 96.0
        if not resize(hwnd, int(round(w * scale)), int(round(h * scale))):
            print("render_check: SetWindowPos failed for %s" % tab)
        time.sleep(2.5)
        frame = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(frame))
        ox, oy = frame.left, frame.top
        cw, ch = frame.right - frame.left, frame.bottom - frame.top
        alive = False
        for _ in range(12):
            probe = ctypes.c_size_t()
            if user32.SendMessageTimeoutW(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG,
                                          2000, ctypes.byref(probe)):
                alive = True
                break
            time.sleep(0.25)
        if not alive:
            print("render_check: %s does not answer WM_NULL" % tab)
            return None
        img = capture(hwnd, cw, ch)
        if img is None:
            img = ImageGrab.grab(bbox=(ox, oy, ox + cw, oy + ch))
        img = img.convert("RGB").crop((0, 0, cw, ch))
        colours = distinct_colours(img)
        for _ in range(3):
            if colours >= 100:
                break
            time.sleep(3.0)
            again = capture(hwnd, cw, ch)
            if again is None:
                again = ImageGrab.grab(bbox=(ox, oy, ox + cw, oy + ch))
            img = again.convert("RGB").crop((0, 0, cw, ch))
            colours = distinct_colours(img)
        img.save(out)
        counts = Counter(img.getdata())
        total = sum(counts.values())
        print("render_check: %s %dx%d zoom=%s -> %s (%d colours, %d bytes)"
              % (tab, cw, ch, zoom, os.path.relpath(out, REPO), colours,
                 os.path.getsize(out)))
        for rgb, n in counts.most_common(4):
            print("    #%02x%02x%02x %7d %5.1f%%" % (rgb[0], rgb[1], rgb[2], n,
                                                    100.0 * n / total))
        if colours < 100:
            print("render_check: %s looks FLAT - the window did not paint" % tab)
            return None
        return out
    finally:
        if hwnd is not None:
            user32.PostMessageW(hwnd, 0x0010, 0, 0)
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()


def main():
    quick = "--quick" in sys.argv
    if not os.path.exists(EXE):
        print("render_check: no build at %s" % EXE)
        return 2
    os.makedirs(OUT_DIR, exist_ok=True)
    dpi_aware()
    shots = []
    if quick:
        shots.append(("map", "Map Editor", (1280, 690), 1.0, False, False))
        shots.append(("sprite", "Pixel & Sprite Editor", (1280, 690), 1.0, True, True))
        shots.append(("tileset", "Tileset Editor", (1280, 690), 1.0, False, False))
    else:
        for page, tab in PAGES:
            for size in SIZES:
                shots.append((page, tab, size, 1.0, False, False))
        # The zoom is swept on its own, not crossed with the sizes.
        for zoom in ZOOM_SWEEP:
            shots.append(("map", "Map Editor", SIZES[0], zoom, False, False))
        # The sprite page is rendered in sprite mode, where its tools live,
        # and once with the preview running.
        shots.append(("sprite", "Pixel & Sprite Editor", SIZES[0], 1.0, True, False))
        shots.append(("sprite_preview", "Pixel & Sprite Editor", SIZES[0], 1.0, True, True))
    failed = 0
    for page, tab, size, zoom, sprite, preview in shots:
        if shoot(page, tab, size, zoom, sprite, preview) is None:
            failed += 1
    print()
    print("render_check: %d captures in %s - OPEN THE PNGs" % (len(shots) - failed, OUT_DIR))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
