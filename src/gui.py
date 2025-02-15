from pathlib import Path
from PIL import Image
from pywintypes import error as pywinErr
from win32gui import GetOpenFileNameW
import glfw
import numpy as np
import OpenGL.GL as gl
import os
import win32con

PARENT_PATH = Path(__file__).parent
ASSETS_PATH = PARENT_PATH / Path(r"assets")


def relative_path(path: str):
    return ASSETS_PATH / Path(path)


def start_file_dialog(extension: str, multiselect: bool) -> list | str:
    try:
        fnames = []
        customfilter = "All Files\0*.*\0"
        fname, customfilter, _ = GetOpenFileNameW(
            InitialDir=os.getcwd(),
            Flags=multiselect
            and win32con.OFN_EXPLORER | win32con.OFN_ALLOWMULTISELECT
            or win32con.OFN_EXPLORER,
            DefExt=None,
            Title="Select DLL",
            File="",
            MaxFile=65535,
            Filter=extension,
            CustomFilter=customfilter,
            FilterIndex=0,
        )

        if str(fname).find("\x00"):
            split_n = str(fname).split("\x00")
            fnames.append(split_n)

        if len(fnames[0]) > 1:
            filePath_lst = []
            parent_path = fnames[0][0]
            for i in range(1, len(fnames[0])):
                filePaths = os.path.join(parent_path, fnames[0][i])
                filePath_lst.append(filePaths)

            final_list = filePath_lst

        else:
            final_list = fnames[0][0]

        return final_list

    except pywinErr:
        return None


def fb_to_window_factor(window):
    """
    Frame buffer to window factor.
    """
    win_w, win_h = glfw.get_window_size(window)
    fb_w, fb_h = glfw.get_framebuffer_size(window)

    return max(float(fb_w) / win_w, float(fb_h) / win_h)


def new_window(
    title: str, width: int, height: int, resizable: bool
) -> tuple[object, object]:

    if not glfw.init():
        raise Exception("Failed to initialize OpenGL context!")

    glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
    glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
    glfw.window_hint(glfw.RESIZABLE, gl.GL_TRUE if resizable else gl.GL_FALSE)
    glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    glfw.window_hint(glfw.OPENGL_FORWARD_COMPAT, gl.GL_TRUE)

    monitor = glfw.get_primary_monitor()
    vidMode = glfw.get_video_mode(monitor)
    pos_x = vidMode.size.width
    pos_y = vidMode.size.height

    window = glfw.create_window(int(width), int(height), title, None, None)
    ibeam_cursor = glfw.create_standard_cursor(glfw.IBEAM_CURSOR)
    icon = Image.open(relative_path("img/ylp_icon.ico"))
    icon = icon.convert("RGBA")
    icon_data = np.array(icon, dtype=np.uint8)
    icon_struct = [icon.width, icon.height, icon_data]
    glfw.set_window_pos(window, int(pos_x / 2 - width / 2), int(pos_y / 2 - height / 2))
    glfw.set_window_icon(window, 1, icon_struct)
    glfw.make_context_current(window)

    if not window:
        glfw.terminate()
        raise Exception("Failed to initialize window!")

    return window, ibeam_cursor


class Icons:
    Star = "\uf005"
    Checkmark = "\uf00c"
    Close = "\uf00d"
    Gear = "\uf013"
    Down = "\uf01a"
    Up = "\uf01b"
    Download = "\uf019"
    Refresh = "\uf021"
    List = "\uf03a"
    Play = "\uf04b"
    Plus = "\uf055"
    Minus = "\uf056"
    Folder = "\uf07c"
    GitHub = "\uf09b"
    Save = "\uf0c7"
    Dashboard = "\uf0e4"
    Spinner = "\uf110"
    Circle = "\uf111"
    Code = "\uf121"
    Rocket = "\uf135"
    File_c = "\uf1c9"
    Trash = "\uf1f8"
