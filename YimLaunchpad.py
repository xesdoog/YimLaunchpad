import os, sys
if getattr(sys, 'frozen', False):
    import pyi_splash # type: ignore

from pathlib import Path
from win32gui import FindWindow, SetForegroundWindow
from src import utils

PARENT_PATH    = Path(__file__).parent
ASSETS_PATH    = PARENT_PATH / Path(r"src/assets")
LAUNCHPAD_PATH = os.path.join(os.getenv("APPDATA"), "YimLaunchpad")
LOG            = utils.LOGGER()
this_window    = FindWindow(None, "YimLaunchpad")
if this_window != 0:
    LOG.warning("YimLaunchpad is aleady running! Only one instance can be launched at once.\n")
    SetForegroundWindow(this_window)
    sys.exit(0)

if not os.path.exists(LAUNCHPAD_PATH):
    os.mkdir(LAUNCHPAD_PATH)

LOG.OnStart(LAUNCHPAD_PATH)


import atexit
import imgui
import io
import psutil
import requests
import subprocess
import zipfile

from concurrent.futures      import ThreadPoolExecutor
from imgui.integrations.glfw import GlfwRenderer
from pyinjector              import inject
from src                     import gui
from time                    import sleep

Icons             = gui.Icons
threadpool        = ThreadPoolExecutor(max_workers = 3)
progress_value    = 0
process_id        = 0
pending_update    = False
ylp_update_avail  = False
yim_update_avail  = False
game_is_running   = False
ylp_ver_thread    = None
ylp_down_thread   = None
yim_ver_thread    = None
ylp_init_thread   = None
launch_thread     = None
inject_thread     = None
game_check_thread = None
yim_down_thread   = None
file_add_thread   = None
lua_repos_thread  = None
lua_downl_thread  = None
status_updt_task  = None
task_status_col   = None
task_status       = ""
YML_VERSION       = "1.0.0"
NIGHTLY_URL       = "https://github.com/Mr-X-GTA/YimMenu/releases/download/nightly/YimMenu.dll"
YIMDLL_PATH       = os.path.join(LAUNCHPAD_PATH, "YimMenu")
CONFIG_PATH       = os.path.join(LAUNCHPAD_PATH, "settings.json")
# LOG_BACKUP        = os.path.join(LAUNCHPAD_PATH, "Log Backup")
YIMDLL_FILE       = os.path.join(YIMDLL_PATH, 'YimMenu.dll')
YIM_MENU_PATH     = os.path.join(os.getenv("APPDATA"), "YimMenu")
YIM_SETTINGS      = os.path.join(YIM_MENU_PATH, "settings.json")
YIM_SCRIPTS_PATH  = os.path.join(YIM_MENU_PATH, "scripts")
dll_files         = []
repos             = []
ImRed             = [1.0, 0.0, 0.0]
ImGreen           = [0.0, 1.0, 0.0]
ImBlue            = [0.0, 0.0, 1.0]
ImYellow          = [1.0, 1.0, 0.0]
LAUNCHERS         = [
    "- Select Launcher -",
    "Epic Games",
    "Rockstar Games",
    "Steam",
]
UIThemes          = {
    "Dark": {
        "frame": [0.1, 0.1, 0.1],
        "header": [0.1, 0.1, 0.1],
        "button": [1.0, 1.0, 1.0],
    },
    "Valentines": {
        "frame": [0.1, 0.0, 0.0],
        "header": [0.1, 0.0, 0.0],
        "button": [1.0, 0.0, 0.0],
    },
    "Ganja": {
        "frame": [0.0, 0.1, 0.0],
        "header": [0.0, 0.1, 0.0],
        "button": [0.0, 1.0, 0.0],
    },
    "Blue": {
        "frame": [0.0, 0.0, 0.1],
        "header": [0.0, 0.0, 0.1],
        "button": [0.0, 0.0, 1.0],
    },
}

default_cfg      = {
    "theme": UIThemes["Dark"],
    "custom_dlls": False,
    "auto_inject": False,
    "auto_inject_delay": 0,
    "dll_files": [],
}


def res_path(path: str) -> Path:
    return ASSETS_PATH / Path(path)


def set_cursor(window: object, cursor: object):
    return gui.glfw.set_cursor(window, cursor)


def set_window_size(window: object, width: int, height: int):
    return gui.glfw.set_window_size(window, width, height)


def colored_button(label: str, color: list, hovered_color: list, active_color: list) -> bool:
    imgui.push_style_color(imgui.COLOR_BUTTON, color[0], color[1], color[2])
    imgui.push_style_color(imgui.COLOR_BUTTON_ACTIVE, hovered_color[0], hovered_color[1], hovered_color[2])
    imgui.push_style_color(imgui.COLOR_BUTTON_HOVERED, active_color[0], active_color[1], active_color[2])
    retbool = imgui.button(label)
    imgui.pop_style_color(3)
    return retbool


def tooltip(text= "", font = None, alpha = 0.75):
    if imgui.is_item_hovered():
        imgui.push_style_var(imgui.STYLE_WINDOW_ROUNDING, 10)
        imgui.set_next_window_bg_alpha(alpha)
        with imgui.begin_tooltip():
            imgui.push_text_wrap_pos(imgui.get_font_size() * 15)
            if font:
                with imgui.font(font):
                    imgui.text(text)
            else:
                imgui.text(text)
            imgui.pop_text_wrap_pos()
        imgui.pop_style_var()


def stringFind(string: str, sub: str):
    return string.lower().find(sub.lower()) != -1


def status_text(text ="", color = None):
    if color:
        imgui.text_colored(text, color[0], color[1], color[2], 1)
    else:
        imgui.text(text)


def set_task_status(msg = "", color = None, timeout = 2):
    global task_status, task_status_col
    task_status = msg; task_status_col = color
    sleep(timeout)
    task_status = ""; task_status_col = None


def run_task_status_update(msg = "", color = None, timeout = 2):
    global status_updt_task
    if status_updt_task and not status_updt_task.done():
        pass
    else:
        status_updt_task = threadpool.submit(set_task_status, msg, color, timeout)


def get_status_widget_color():
    global task_status
    global ylp_ver_thread
    global ylp_down_thread
    global yim_ver_thread 
    global yim_down_thread
    global ylp_init_thread
    global launch_thread
    global inject_thread
    global file_add_thread
    global lua_repos_thread
    global lua_downl_thread

    if task_status != "":
        if stringFind(task_status, "error") or stringFind(task_status, "failed"):
            return ImRed
        else:
            for thread in (
                ylp_init_thread,
                ylp_ver_thread,
                ylp_down_thread,
                yim_ver_thread,
                yim_down_thread,
                launch_thread,
                inject_thread,
                file_add_thread,
                lua_repos_thread,
                lua_downl_thread,
                ):
                if thread and not thread.done():
                    return ImYellow
    return ImGreen


def check_for_ylp_update():
    global task_status, task_status_col, ylp_update_avail

    task_status_col = None
    task_status = "Checking for Launchpad updates, please wait..."
    remote_version = utils.get_launchpad_version()
    LOG.info('Checking for Launchpad updates...')
    if remote_version is not None:
        try:
            if YML_VERSION < remote_version:
                LOG.info('Update available!')
                task_status_col = ImGreen
                task_status = f"Update {remote_version} is available."
                ylp_update_avail = True
            elif YML_VERSION == remote_version:
                LOG.info(f'No updates were found! {YML_VERSION} is the latest version.')
                task_status_col = None
                task_status = f"No updates were found! {YML_VERSION} is the latest version."
                ylp_update_avail = False
                sleep(3)
                task_status = ""
            elif YML_VERSION > remote_version:
                ylp_update_avail = False
                LOG.error(f'Local YimLaunchpad version is {YML_VERSION}. This is not a valid version! Are you a dev or a skid?')
                task_status_col = ImRed
                task_status = "Invalid version detected!\nPlease download YMU from the official Github repository."
                sleep(5)
                task_status_col = None
                task_status = ""
        except Exception as e:
            task_status_col = ImRed
            task_status = "An error occured while attempting to check for updates. Check the log for more details."
            LOG.error(f'An error occured! Traceback: {e}')
            ylp_update_avail = False
            sleep(5)
            task_status_col = None
            task_status = ""
    else:
        task_status_col = ImRed
        task_status = "Failed to get the latest GitHub version! Check the log for more details."
        sleep(5)
        task_status_col = None
        task_status = ""


def check_for_yim_update():
    global task_status
    global yim_update_avail
    global task_status_col

    task_status = "Checking for YimMenu updates..."
    remote_sha = utils.get_remote_checksum()
    local_sha = utils.calc_file_checksum(YIMDLL_FILE)
    if remote_sha is not None and local_sha is not None:
        try:
            if local_sha != remote_sha:
                LOG.info('Update available!')
                task_status = "Update available."
                task_status_col = ImGreen
                yim_update_avail = True
            else:
                LOG.info("No updates were found! YimMenu is up to date.")
                task_status = "No updates were found! YimMenu is up to date."
                yim_update_avail = False
        except Exception as e:
            task_status = "An error occured while attempting to check for updates! Check the log for more details."
            LOG.error(f'An error occured! Traceback: {e}')
            yim_update_avail = False
    else:
        task_status_col = ImRed
        task_status = "Failed to check for updates! See the log for more details."
    sleep(5)
    task_status_col = None
    task_status = ""


def yimlaunchapd_init():
    global yim_update_avail
    global task_status
    global default_cfg
    global dll_files
    global repos
    global task_status_col

    task_status_col = None
    task_status = "Initializing YimLaunchpad, please wait..."

    remove_self_updater()
    if not os.path.exists(CONFIG_PATH):
        utils.save_cfg(CONFIG_PATH, default_cfg)
    if os.path.isfile(YIMDLL_FILE):
        if utils.get_remote_checksum() != utils.calc_file_checksum(YIMDLL_FILE):
            yim_update_avail = True
        else:
            yim_update_avail = False
    else:
        yim_update_avail = False
    task_status_col = None
    task_status = ""

ylp_init_thread = threadpool.submit(yimlaunchapd_init)


def download_yim_menu():
    global task_status, progress_value, task_status_col, yim_update_avail
    if not os.path.exists(YIMDLL_PATH):
        os.makedirs(YIMDLL_PATH)
    try:
        task_status = "Downloading YimMenu Nightly..."
        with requests.get(NIGHTLY_URL, stream = True) as response:
            response.raise_for_status()
            total_size = int(response.headers.get("content-length", 0))
            current_size = 0
            if response.status_code == 200:
                LOG.info(f"Requesting file from {NIGHTLY_URL}")
                LOG.info(f"Total size: {"{:.2f}".format(total_size/1048576)}MB")
                with open(YIMDLL_FILE, "wb") as f:
                    LOG.info("Downloading YimMenu Nightly...")
                    for chunk in response.iter_content(131072):
                        f.write(chunk)
                        current_size += len(chunk)
                        progress_value = current_size / total_size
                LOG.info(f"Download finished. DLL location: {YIMDLL_FILE}")
                task_status = "Download complete."
                yim_update_avail = False
                sleep(5)
                if not os.path.isfile(YIMDLL_FILE):
                    task_status = "File was removed!\nMake sure to either turn off your antivirus or add YMU folder to exceptions."
                    LOG.error("The dll was removed by antivirus!")
            else:
                task_status = "Download failed! Check the log for more details."
                LOG.error(f"Download failed! The request returned status code: {response.status_code}")
    except requests.exceptions.RequestException as e:
        LOG.error(f'An error occured while trying to download YimMenu. Traceback: {e}')
        task_status = "Download failed! Check the log for more details."
        task_status_col = ImRed
    sleep(5)
    task_status = ""
    task_status_col = None
    progress_value = 0


def download_updater():
    global task_status
    global task_status_col
    global progress_value
    global pending_update

    pending_update = False
    task_status_col = None
    try:
        task_status = "Fetching updater from GitHub..."
        response = requests.get(
            "https://github.com/xesdoog/YimLaunchpad-Updater/releases/download/YLPU/ylp_updater.exe"
        )
        if response.status_code == 200:
            total_size = response.headers.get("content-length")
            LOG.info("Downloading self updater from https://github.com/xesdoog/YimLaunchpad-Updater/releases/download/YLPU/ylp_updater.exe")
            with open("ymu_self_updater.exe", "wb") as f:
                for chunk in response.iter_content(4096):
                    f.write(chunk)
                    progress_value += len(chunk) / total_size
                task_status = "Download complete."
        else:
            LOG.error(f"An error occured while trying to fetch updater's repository. Status Code: {response.status_code}")
            task_status = "Failed to download the updater. Check the log for more details."
            task_status_col = ImRed
    except Exception as e:
        LOG.error(f'An error occured! Traceback: {e}')
    
    sleep(3)
    progress_value = 0
    task_status = ""
    task_status_col = None
    if os.path.exists("./ylp_updater.exe"):
        for i in range(3):
            task_status = f"Restarting in ({3 - i}) seconds"
            sleep(1)
        pending_update = True


def remove_self_updater():
    if os.path.isfile("./ylp_updater.exe"):
        LOG.info('Updater no longer needed. Deleting the file...')
        os.remove("./ylp_updater.exe")


def run_ylp_update_check():
    global ylp_ver_thread
    if ylp_ver_thread and not ylp_ver_thread.done():
        pass
    else:
        ylp_ver_thread = threadpool.submit(check_for_ylp_update)


def run_yim_update_check():
    global yim_ver_thread
    if yim_ver_thread and not yim_ver_thread.done():
        pass
    else:
        yim_ver_thread = threadpool.submit(check_for_yim_update)


def run_yim_download():
    global yim_down_thread
    if yim_down_thread and not yim_down_thread.done():
        pass
    else:
        yim_down_thread = threadpool.submit(download_yim_menu)


def run_ylp_update():
    global ylp_down_thread
    if ylp_down_thread and not ylp_down_thread.done():
        pass
    else:
        ylp_down_thread = threadpool.submit(download_updater)


def check_lua_repos():
    global repos, task_status, task_status_col
    try:
        task_status_col = None
        task_status = "Getting repository information..."
        repos, core_remaining, _, reset_time = utils.get_lua_repos()
        if core_remaining == 0 and reset_time["seconds"] > 1:
            task_status_col = ImRed
            reset_time_str = f"{reset_time["hours"]} hours, {reset_time["minutes"]} minutes, and {reset_time["seconds"]} seconds."
            task_status = f"GitHub API rate limit exceeded! Please try again in {reset_time_str}"
        sleep(5)
        task_status_col = None
        task_status = ""
    except Exception as e:
        task_status_col = ImRed
        task_status = "An error occured! Check the log for more details."
        LOG.error(f"Failed to get repository information! Traceback: {e}")
        sleep(3)
        task_status_col = None
        task_status = ""
        pass


def run_lua_repos_check():
    global lua_repos_thread
    if lua_repos_thread and not lua_repos_thread.done():
        pass
    else:
        lua_repos_thread = threadpool.submit(check_lua_repos)


def add_file_with_short_sha(file_path):
    global dll_files
    global task_status
    global task_status_col
    try:
        task_status_col = None
        checksum  = utils.calc_file_checksum(file_path)
        file_name = f"{os.path.basename(file_path)} [{str(checksum)[:6]}]"
        if utils.is_file_saved(file_name, dll_files):
            task_status = f"File {file_name} already exists."
            sleep(3)
            task_status = ""
            return
        dll_files.append({"name": file_name, "path": file_path})
        utils.save_cfg_item(CONFIG_PATH, "dll_files", dll_files)
        task_status = f"Added file {file_name} to the list of custom DLLs."
        sleep(3)
        task_status = ""
    except Exception:
        task_status_col = ImRed
        task_status = "An error occured! Check the log for more details."
        sleep(3)
        task_status_col = None
        task_status = ""


def run_file_add_thread(file_path):
    global file_add_thread
    if file_add_thread and not file_add_thread.done():
        pass
    else:
        file_add_thread = threadpool.submit(add_file_with_short_sha, file_path)


def inject_dll(dll, process_id):
    global task_status
    global task_status_col
    global game_is_running

    task_status = "Starting injection..."
    try:
        if process_id != 0:
            if os.path.isfile(dll):
                libHanlde = inject(process_id, dll)
                task_status = f"Injecting {os.path.basename(dll)} into GTA5.exe..."
                LOG.info(f"Injecting {dll} into GTA5.exe...")
                LOG.debug(f"Injected library handle: {libHanlde}")
                sleep(3)
                task_status = "Checking if the game is still running after injection..."
                LOG.debug("Checking if the game is still running after injection...")
                sleep(5)
                if game_is_running:
                    task_status = "Done."
                    LOG.debug("Everything seems fine.")
                    sleep(3)
                    task_status = ""
                else:
                    LOG.warning("The game seems to have crashed after injection!")
                    task_status = "Uh Oh! Did your game crash?"
                    task_status_col = ImYellow
            else:
                LOG.error("YimMenu.dll not found! Did the antivirus delete it?")
                task_status = "YimMenu.dll not found! Download the latest release and make sure your anti-virus is not interfering."
                task_status_col = ImRed
        else:
            LOG.warning("Injection failed! Process not found.")
            task_status = "Process not found! Is the game running?"
            task_status_col = ImRed
        sleep(5)
        task_status = ""
        task_status_col = None
    except Exception as e:
        LOG.critical(f'An exception has occured! Traceback: {e}')
        task_status ="Injection failed! Check the log for more details."
        task_status_col = ImRed
        sleep(5)
        task_status = ""
        task_status_col = None


def run_inject_thread(path, process):
    global inject_thread
    if inject_thread and not inject_thread.done():
        pass
    else:
        inject_thread = threadpool.submit(inject_dll, path, process)


def background_thread():
    global process_id
    global game_is_running

    try:
        for p in psutil.process_iter(["name", "exe", "cmdline"]):
            sleep(0.01)
            if (
                "GTA5.exe" == p.info["name"]
                or p.info["exe"]
                and os.path.basename(p.info["exe"]) == "GTA5.exe"
                or p.info["cmdline"]
                and p.info["cmdline"][0] == "GTA5.exe"
            ):
                pid = p.pid
                break
            else:
                pid = 0
        if pid is not None and pid != 0:
            process_id = pid
            game_is_running = True
        else:
            process_id = 0
            game_is_running = False
    except Exception as e:
        LOG.error(f"An error has occured while trying to find the game's process. Traceback: {e}")
    sleep(1)
    # # move log backups to the backup folder
    # for i in range(5):
    #     if os.path.exists(os.path.join(LAUNCHPAD_PATH, f"ylp.log.{i}")):
    #         if not os.path.exists(LOG_BACKUP):
    #             os.makedirs(LOG_BACKUP)
    #         if os.path.exists(os.path.join(LOG_BACKUP, f"ylp.log.{i}")):
    #             os.remove(os.path.join(LOG_BACKUP, f"ylp.log.{i}"))
    #         os.rename(os.path.join(LAUNCHPAD_PATH, f"ylp.log.{i}"), os.path.join(LOG_BACKUP, f"ylp.log.{i}"))


def run_background_thread():
    global game_check_thread
    if game_check_thread and not game_check_thread.done():
        pass
    else:
        game_check_thread = threadpool.submit(background_thread)


def start_gta(idx: int):
    global task_status, task_status_col

    try:
        if idx == 0:
            task_status = "Please select your lancher from the dropdown list!"
            task_status_col = ImYellow
        elif idx == 1:
            task_status = "Attempting to launch GTA V though Epic Games Launcher, please wait..."
            subprocess.run("cmd /c start com.epicgames.launcher://apps/9d2d0eb64d5c44529cece33fe2a46482?action=launch&silent=true")
        elif idx == 2:
            task_status = "Attempting to launch GTA V though Rockstar Games Launcher, please wait..."
            rgl_path = utils.get_rgl_path()
            if rgl_path is not None:
                try:
                    os.startfile(rgl_path + r'PlayGTAV.exe')
                except OSError as err:
                    LOG.error(f'Failed to run GTA5! Traceback: {err}')
                    task_status = "Failed to run GTAV using Rockstar Games Launcher!"
                    task_status_col = ImRed
            else:
                task_status = "Failed to find Rockstar Games version of GTA V!"
                task_status_col = ImRed
        elif idx == 3:
            task_status = "Attempting to launch GTA V though Steam, please wait..."
            subprocess.run("cmd /c start steam://run/271590")
        sleep(10)
        task_status = ""
        task_status_col = None
    except Exception as e:
        task_status = f"Failed to find GTA V executable!"
        task_status_col = ImRed
        sleep(5)
        task_status = ""
        task_status_col = None


def run_launch_thread(idx: int):
    global launch_thread
    if launch_thread and not launch_thread.done():
        pass
    else:
        launch_thread = threadpool.submit(start_gta, idx)


def lua_download_loose_files(repo, path = "", base_path = ""):
    global task_status, progress_value

    contents = repo.get_contents(path)
    for content in contents:
        progress_value += 1 / len(contents)
        if content.type == "dir":
            lua_download_loose_files(repo, content.path, base_path if base_path else content.path)
        elif content.type == "file" and content.name.endswith(".lua"):
            relative_path = os.path.relpath(content.path, base_path) if base_path else content.path
            save_path = os.path.join(YIM_SCRIPTS_PATH, repo.name, relative_path)
            os.makedirs(os.path.dirname(save_path), exist_ok = True)
            task_status  = f"Downloading {content.path} from {repo.name}"
            lua_content  = requests.get(content.download_url).text
            with open(save_path, "w", encoding = "utf-8") as f:
                f.write(lua_content)


def lua_download_release(repo):
    global task_status, progress_value

    task_status = f"Downloading {repo.name}"
    releases = repo.get_releases()
    if releases.totalCount == 0:
        return False

    latest_release = releases[0]
    task_status = f"Found release: {releases[0].title}. Downloading..."
    for asset in latest_release.get_assets():
        if str(asset.name).endswith(".zip"):
            response = requests.get(asset.browser_download_url)
            if response.status_code == 200:
                with zipfile.ZipFile(io.BytesIO(response.content)) as zip_file:
                    for file in zip_file.namelist():
                        progress_value += 1 / len(zip_file.namelist())
                        if file.endswith(".lua"):
                            extract_path = os.path.join(YIM_SCRIPTS_PATH, repo.name, file)
                            os.makedirs(os.path.dirname(extract_path), exist_ok=True)
                            with open(extract_path, "wb") as f:
                                f.write(zip_file.read(file))
                    return True

    task_status = f"No suitable ZIP found in the latest release of {repo.name}"
    return False


def lua_download(repo):
    global task_status, progress_value
    if not lua_download_release(repo):
        task_status = f"No suitable release found for {repo.name}! Trying to manually download the repo..."
        sleep(1)
        lua_download_loose_files(repo)
    task_status = "Download complete."
    sleep(3)
    progress_value = 0
    task_status = ""


def is_script_installed(repo):
    return os.path.exists(os.path.join(YIM_SCRIPTS_PATH, repo.name))


def is_script_disabled(repo):
    return os.path.exists(os.path.join(os.path.join(YIM_SCRIPTS_PATH, "disabled"), repo.name))


def run_lua_download(repo):
    global lua_downl_thread
    if lua_downl_thread and not lua_downl_thread.done():
        pass
    else:
        lua_downl_thread = threadpool.submit(lua_download, repo)


def OnDraw():
    global task_status
    global dll_files
    global process_id
    global game_is_running
    global repos

    custom_dlls     = utils.read_cfg_item(CONFIG_PATH, "custom_dlls")
    dll_files       = utils.read_cfg_item(CONFIG_PATH, "dll_files") or []
    debug_settings  = utils.read_cfg_item(YIM_SETTINGS, "debug")
    lua_settings    = utils.read_cfg_item(YIM_SETTINGS, "lua")
    dll_index       = 0
    launcher_index  = 0
    repo_index      = 0
    disable_console = debug_settings is not None and not debug_settings["external_console"] or False
    lua_auto_reload = lua_settings is not None and lua_settings["enable_auto_reload_changed_scripts"] or False
    repos_checked   = False
    selected_dll    = []
    selected_repo   = []
    imgui.create_context()
    window, _ = gui.new_window("YimLaunchpad", 400, 480, False)
    impl = GlfwRenderer(window)
    font_scaling_factor = gui.fb_to_window_factor(window)
    io = imgui.get_io()
    io.fonts.clear()
    io.font_global_scale = 1.0 / font_scaling_factor
    font_config = imgui.core.FontConfig(merge_mode = True)
    icons_range = imgui.core.GlyphRanges(
        [
            0xF004,
            0xF005,
            0xF00C,
            0xF00D,
            0xF013,
            0xF014,
            0xF01A,
            0xF01B,
            0xF019,
            0xF021,
            0xF03A,
            0xF03B,
            0xF04A,
            0xF04B,
            0xF055,
            0xF056,
            0xF07C,
            0xF07D,
            0xF09B,
            0xF09C,
            0xF0C7,
            0xF0C8,
            0xF0E3,
            0xF0E4,
            0xF110,
            0xF111,
            0xF120,
            0xF121,
            0xF134,
            0xF135,
            0xF1C9,
            0xF1CA,
            0xF1F7,
            0xF1F8,
            0,
        ]
    )
    
    title_font = io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/Rokkitt-Regular.ttf")), 25 * font_scaling_factor,
    )

    small_font = io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/Rokkitt-Regular.ttf")), 16.0 * font_scaling_factor,
    )

    main_font = io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/Rokkitt-Regular.ttf")), 20 * font_scaling_factor,
    )

    io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/fontawesome-webfont.ttf")), 16 * font_scaling_factor,
        font_config, icons_range
    )

    impl.refresh_font_texture()

    while not gui.glfw.window_should_close(window):
        gui.glfw.poll_events()
        impl.process_inputs()
        imgui.new_frame()
        win_w, win_h = gui.glfw.get_window_size(window)
        imgui.set_next_window_size(win_w, win_h)
        imgui.set_next_window_position(0, 0)
        imgui.push_style_color(imgui.COLOR_FRAME_BACKGROUND, 0.1, 0.1, 0.1)
        imgui.push_style_color(imgui.COLOR_FRAME_BACKGROUND_ACTIVE, 0.3, 0.3, 0.3)
        imgui.push_style_color(imgui.COLOR_FRAME_BACKGROUND_HOVERED, 0.5, 0.5, 0.5)
        imgui.push_style_color(imgui.COLOR_TAB, 0.097, 0.097, 0.097)
        imgui.push_style_color(imgui.COLOR_TAB_ACTIVE, 0.075, 0.075, 0.075)
        imgui.push_style_color(imgui.COLOR_TAB_HOVERED, 0.085, 0.085, 0.085)
        imgui.push_style_color(imgui.COLOR_HEADER, 0.1, 0.1, 0.1)
        imgui.push_style_color(imgui.COLOR_HEADER_ACTIVE, 0.3, 0.3, 0.3)
        imgui.push_style_color(imgui.COLOR_HEADER_HOVERED, 0.5, 0.5, 0.5)
        imgui.push_style_color(imgui.COLOR_BUTTON, 0.075, 0.075, 0.075)
        imgui.push_style_color(imgui.COLOR_BUTTON_ACTIVE, 0.085, 0.085, 0.085)
        imgui.push_style_color(imgui.COLOR_BUTTON_HOVERED, 0.1, 0.1, 0.1)
        imgui.push_style_var(imgui.STYLE_CHILD_ROUNDING, 5)
        imgui.push_style_var(imgui.STYLE_FRAME_ROUNDING, 5)
        imgui.push_style_var(imgui.STYLE_ITEM_SPACING, (5, 5))
        imgui.push_style_var(imgui.STYLE_ITEM_INNER_SPACING, (5, 5))
        imgui.push_style_var(imgui.STYLE_FRAME_PADDING, (5, 5))
        imgui.push_font(main_font)
        imgui.begin("Main Window", flags =
                    imgui.WINDOW_NO_TITLE_BAR |
                    imgui.WINDOW_NO_RESIZE |
                    imgui.WINDOW_NO_MOVE
                    )
        with imgui.begin_child("##YLP", 0, 420):
            if ylp_init_thread and ylp_init_thread.done():
                if imgui.begin_tab_bar("ylp"):
                    if imgui.begin_tab_item(f" {Icons.Dashboard}  Dashboard  ").selected:
                        run_background_thread()
                        with imgui.begin_child("##yimmenu", border = True):
                            with imgui.font(title_font):
                                text_size = imgui.calc_text_size("- YimMenu -").x
                                imgui.dummy(win_w / 2 - text_size / 1.6, 1); imgui.same_line(); imgui.text("- YimMenu -")
                            imgui.dummy(1, 10)
                            if os.path.isfile(YIMDLL_FILE):
                                yim_update_btn_label = f"{Icons.Refresh}  Check for Updates" if not yim_update_avail else f"{Icons.Download}  Update YimMenu"
                                yim_update_btn_callback = run_yim_update_check if not yim_update_avail else run_yim_download
                                if imgui.button(yim_update_btn_label):
                                    yim_update_btn_callback()
                            else:
                                if imgui.button(f"{Icons.Download}  Download YimMenu"):
                                    run_yim_download()
                            imgui.dummy(1, 5)
                            with imgui.font(small_font):
                                imgui.bullet_text("Game Status:"); imgui.same_line()
                            if game_is_running:
                                with imgui.font(small_font):
                                    imgui.text_colored("Running.", 0, 1, 0, 0.88)
                                imgui.dummy(1, 5)
                                if imgui.button(f"{Icons.Rocket}  Inject YimMenu"):
                                    run_inject_thread(YIMDLL_FILE, process_id)
                                imgui.same_line()
                                dc_clicked, disable_console = imgui.checkbox("Disable Console", disable_console)
                                tooltip("If you want to hide/show YimMenu's debug console, do it before injecting.")
                                if dc_clicked:
                                    debug_settings["external_console"] = not disable_console
                                    utils.save_cfg_item(YIM_SETTINGS, "debug", debug_settings)
                            else:
                                with imgui.font(small_font):
                                    imgui.text_colored("Not running.", 1, 0, 0, 0.88)
                                imgui.dummy(1, 5)
                                imgui.set_next_item_width(200)
                                _, launcher_index = imgui.combo("##launchers", launcher_index, LAUNCHERS)
                                imgui.same_line()
                                if imgui.button(f" {Icons.Play}  Run "):
                                    run_launch_thread(launcher_index)

                            imgui.dummy(1, 5)
                            imgui.separator()
                            imgui.dummy(1, 5)
                            cdll_clicked, custom_dlls = imgui.checkbox("Enable Custom DLLs", custom_dlls)
                            if cdll_clicked:
                                utils.save_cfg_item(CONFIG_PATH, "custom_dlls", custom_dlls)
                            if custom_dlls:
                                with imgui.begin_child("##dll_list", 330, 150, True):
                                    imgui.text(f"{Icons.List}  Custom DLLs")
                                    imgui.separator()
                                    if len(dll_files) > 0:
                                        for i in range(len(dll_files)):
                                            file_selected = (dll_index == i)
                                            clicked, file_selected = imgui.selectable(dll_files[i]["name"], file_selected, 0, 0)
                                            if imgui.is_item_hovered():
                                                rect_x, _ = imgui.get_item_rect_size()
                                                if rect_x > 330:
                                                    tooltip(dll_files[i]["name"])
                                                if imgui.is_mouse_double_clicked():
                                                    utils.open_folder(dll_files[i]["path"])
                                            if clicked:
                                                dll_index = i
                                                imgui.set_item_default_focus()
                                        selected_dll = dll_files[dll_index]
                                imgui.same_line(spacing=10)
                                with imgui.begin_child("##buttons"):
                                    imgui.dummy(1, 20)
                                    if imgui.button(Icons.Plus):
                                        file_path = gui.start_file_dialog("DLL\0*.dll;\0", False)
                                        if file_path is not None:
                                            run_file_add_thread(file_path)
                                    tooltip("Add custom DLL.")
                                    if len(dll_files) > 0:
                                        if imgui.button(Icons.Minus):
                                            dll_files.remove(selected_dll)
                                            utils.save_cfg_item(CONFIG_PATH, "dll_files", dll_files)
                                            if dll_index > 0:
                                                dll_index -= 1
                                        tooltip("Remove custom DLL.")
                                    if game_is_running:
                                        if imgui.button(Icons.Rocket):
                                            run_inject_thread(selected_dll["path"], process_id)
                                        tooltip("Inject custom DLL into GTA5.exe")
                        imgui.end_tab_item()

                    if imgui.begin_tab_item(f"  {Icons.Code}  Lua Scripts  ").selected:
                        if len(repos) == 0 and not repos_checked:
                            run_lua_repos_check()
                            repos_checked = True
                        with imgui.begin_child("##Lua", border = True):
                            if lua_repos_thread and lua_repos_thread.done():
                                if imgui.button(f"{Icons.Refresh}  Refresh List"):
                                    run_lua_repos_check()
                            else:
                                imgui.button(f"{Icons.Spinner}  Please wait!")
                            imgui.same_line()
                            lar_clicked, lua_auto_reload = imgui.checkbox("Auto-Reload Lua Scripts", lua_auto_reload)
                            if lar_clicked:
                                lua_settings["enable_auto_reload_changed_scripts"] = lua_auto_reload
                                utils.save_cfg_item(YIM_SETTINGS, "lua", lua_settings)
                            imgui.dummy(1, 10)
                            imgui.text(f"{Icons.GitHub}  Lua Repositories"); imgui.separator()
                            with imgui.begin_child("##repos", 0, 235):
                                if len(repos) > 0:
                                    for i in range(len(repos)):
                                        repo_selected = repo_index == i
                                        clicked, repo_selected = imgui.selectable(repos[i].name, repo_selected, 0, 0)
                                        if imgui.is_item_hovered() and imgui.is_mouse_double_clicked():
                                            utils.visit_url(repos[i].html_url)
                                        if clicked:
                                            repo_index = i
                                            imgui.set_item_default_focus()
                                        item_width = imgui.calc_text_size(repos[i].name).x
                                        imgui.same_line(spacing = 400 - item_width - 100)
                                        imgui.text_colored(Icons.Star, 1, 1, 0, 0.7); imgui.same_line()
                                        with imgui.font(small_font):
                                            imgui.text(str(repos[i].stargazers_count))
                                    selected_repo = repos[repo_index]
                            imgui.separator()
                            imgui.dummy(1, 5)
                            if len(repos) > 0:
                                if imgui.button(f"{Icons.Download} Download"):
                                    run_lua_download(selected_repo)
                                if is_script_installed(selected_repo) or is_script_disabled(selected_repo):
                                    imgui.same_line()
                                    if imgui.button(f"{Icons.Trash} Delete"):
                                        try:
                                            os.remove(os.path.join(YIM_SCRIPTS_PATH, selected_repo.name))
                                        except OSError:
                                            run_task_status_update("Access denied! Try running YimLaunchpad as admin.", ImRed, 5)
                                if is_script_installed(selected_repo):
                                    imgui.same_line()
                                    if imgui.button(f"{Icons.Close} Disable"):
                                        try:
                                            old_path = os.path.join(YIM_SCRIPTS_PATH, selected_repo.name)
                                            disabled_path = os.path.join(os.path.join(YIM_SCRIPTS_PATH, "disabled"), selected_repo.name)
                                            os.rename(old_path, disabled_path)
                                        except OSError:
                                            run_task_status_update("This script already exists in the disabled folder.", ImYellow, 3)
                                if is_script_disabled(selected_repo):
                                    imgui.same_line()
                                    if imgui.button(f"{Icons.Checkmark} Enable"):
                                        try:
                                            old_path = os.path.join(os.path.join(YIM_SCRIPTS_PATH, "disabled"), selected_repo.name)
                                            enabled_path = os.path.join(YIM_SCRIPTS_PATH, selected_repo.name)
                                            os.rename(old_path, enabled_path)
                                        except OSError:
                                            run_task_status_update("This script already exists in the scripts folder.", ImYellow, 3)
                        imgui.end_tab_item()

                    if imgui.begin_tab_item(f"  {Icons.Gear}  Settings  ").selected:
                        with imgui.begin_child("##settings", border = True):
                            ylp_update_btn_label = f"{Icons.Refresh}  Check for Updates" if not ylp_update_avail else f"{Icons.Download}  Download Update"
                            ylp_update_btn_callback = run_ylp_update_check if not ylp_update_avail else run_ylp_update
                            if imgui.button(ylp_update_btn_label):
                                ylp_update_btn_callback()
                            if imgui.button(f"{Icons.Folder}  Open YimLaunchpad Folder"):
                                utils.open_folder(LAUNCHPAD_PATH)
                            # if imgui.button(f"{Icons.List}  Print All Script Folders"):
                            #     print(utils.get_installed_scripts(YIM_SCRIPTS_PATH))
                        imgui.end_tab_item()

                    imgui.end_tab_bar()

        imgui.spacing()
        with imgui.begin_child("##feedback"):
            status_col = get_status_widget_color()
            imgui.text_colored(Icons.Circle, status_col[0], status_col[1], status_col[2], 0.8)
            imgui.push_text_wrap_pos(win_w - 15)
            with imgui.font(small_font):
                imgui.same_line()
                status_text(task_status, task_status_col)
            imgui.pop_text_wrap_pos()
            if progress_value > 0:
                imgui.progress_bar(progress_value, (380, 5))



        imgui.pop_font()
        imgui.pop_style_var(5)
        imgui.pop_style_color(12)
        imgui.end()

        gui.gl.glClearColor(1.0, 1.0, 1.0, 1)
        gui.gl.glClear(gui.gl.GL_COLOR_BUFFER_BIT)
        imgui.render()
        impl.render(imgui.get_draw_data())
        gui.glfw.swap_buffers(window)

    impl.shutdown()
    gui.glfw.terminate()


@atexit.register
def OnExit():
    LOG.info("Closing YimLaunchpad...\n\nFarewell!\n")


if __name__ == "__main__":
    if getattr(sys, 'frozen', False):
        pyi_splash.close()
    OnDraw()
    try:
        if pending_update:
            os.execvp("./ylp_updater.exe", ["ylp_updater"])
    except NameError:
        pass
