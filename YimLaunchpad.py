import os, sys

if getattr(sys, "frozen", False):
    import pyi_splash  # type: ignore


from pathlib import Path
from win32gui import FindWindow, SetForegroundWindow
from src import utils, gui
from src.memory import get_error_message, Scanner, psutil, PTRN_GS, PTRN_LT
from src.logger import LOGGER


APP_NAME = "YimLaunchpad"
APP_VERSION = "1.0.0.7"
PARENT_PATH = Path(__file__).parent
ASSETS_PATH = PARENT_PATH / Path(r"src/assets")
LAUNCHPAD_PATH = os.path.join(os.getenv("APPDATA"), APP_NAME)
AVATAR_PATH = os.path.join(LAUNCHPAD_PATH, "avatar.png")
UPDATE_PATH = os.path.join(LAUNCHPAD_PATH, "update")
LOG = LOGGER(APP_VERSION)


launchpad_window = FindWindow(None, APP_NAME)
if launchpad_window != 0:
    LOG.warning(
        f"{APP_NAME} is aleady running! Only one instance can be launched at once.\n"
    )
    SetForegroundWindow(launchpad_window)
    sys.exit(0)

if not os.path.exists(LAUNCHPAD_PATH):
    os.mkdir(LAUNCHPAD_PATH)

LOG.on_init()


import atexit
import io
import requests
import zipfile

from concurrent.futures import ThreadPoolExecutor
from imgui.integrations.glfw import GlfwRenderer
from pyinjector import inject
from threading import Thread
from time import sleep, time


Icons = gui.Icons
Scanner = Scanner()
ImGui = gui.imgui
Repository = utils.Repository
threadpool = ThreadPoolExecutor(max_workers=3)
progress_value = 0
process_id = 0
git_requests_left = 0
should_exit = False
pending_update = False
ylp_update_avail = False
ylp_update_active = False
yim_update_avail = False
yim_update_active = False
game_is_running = False
is_menu_injected = False
is_fsl_enabled = False
game_state_checked = False
can_auto_inject = False
window = None
task_status_col = None
ylp_remote_ver = None
gs_addr = None
glt_addr = None
task_status = ""
busy_icon = ""
LAUNCHPAD_URL = "https://github.com/xesdoog/YimLaunchpad"
NIGHTLY_URL = (
    "https://github.com/Mr-X-GTA/YimMenu/releases/download/nightly/YimMenu.dll"
)
YIMDLL_PATH = os.path.join(LAUNCHPAD_PATH, "YimMenu")
CONFIG_PATH = os.path.join(LAUNCHPAD_PATH, "settings.json")
YIMDLL_FILE = os.path.join(YIMDLL_PATH, "YimMenu.dll")
YIM_MENU_PATH = os.path.join(os.getenv("APPDATA"), "YimMenu")
YIM_SCRIPTS_PATH = os.path.join(YIM_MENU_PATH, "scripts")
YIM_SETTINGS = os.path.join(YIM_MENU_PATH, "settings.json")
auto_exit = utils.read_cfg_item(CONFIG_PATH, "auto_exit_after_injection")
auto_inject = utils.read_cfg_item(CONFIG_PATH, "auto_inject")
launchpad_console = utils.read_cfg_item(CONFIG_PATH, "launchpad_console")
is_dev = os.path.isfile(os.path.join(os.getcwd(), "3asba"))
GitOAuth = utils.GitHubOAuth()
threads = {}
updatable_luas = []
starred_luas = []
dll_files = []
repos = []
ImRed = [1.0, 0.0, 0.0]
ImGreen = [0.0, 1.0, 0.0]
ImBlue = [0.0, 0.0, 1.0]
ImYellow = [1.0, 1.0, 0.0]

LAUNCHERS = [
    "- Select Launcher -",
    "Epic Games",
    "Rockstar Games",
    "Steam",
]

UIThemes = {
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

default_cfg = {
    "theme": UIThemes["Dark"],
    "custom_dlls": False,
    "auto_exit_after_injection": False,
    "launchpad_console": False,
    "auto_inject": False,
    "git_logged_in": False,
    "git_username": "",
    "dll_files": [],
}


def res_path(path: str) -> Path:
    return ASSETS_PATH / Path(path)


def is_any_thread_alive() -> bool:
    return any(thread and not thread.done() for thread in threads.values())


def is_thread_alive(thread_name: str) -> bool:
    return (
        thread_name in threads
        and threads[thread_name]
        and not threads[thread_name].done()
    )


def start_thread(thread_name, target_func, *args):
    if is_thread_alive(thread_name):
        return
    threads[thread_name] = threadpool.submit(target_func, *args)


def set_task_status(msg="", color=None, timeout=2):
    global task_status, task_status_col

    task_status = msg
    task_status_col = color
    sleep(timeout)
    task_status = ""
    task_status_col = None


def dummy_progress():
    global progress_value

    progress_value = 0
    for i in range(101):
        progress_value = i / 100
        sleep(0.01)
    sleep(1)
    progress_value = 0


def fetch_luas_progress():
    global progress_value
    global task_status_col

    progress_value = 0

    while is_thread_alive("lua_repos_thread") and task_status_col != ImRed:
        progress_value = progress_value + 0.001
        sleep(0.1)

    if progress_value < 1:
        progress_value = 1
        sleep(0.5)
    progress_value = 0


def add_to_defender_exclusions():
    global task_status, task_status_col

    exe_path = os.path.abspath(sys.argv[0])
    try:
        task_status_col = None
        task_status = f"Executing Powershell to add exclusions..."
        utils.add_exclusion([LAUNCHPAD_PATH, exe_path])
    except Exception:
        task_status_col = ImRed
        task_status = f"Failed to add Windows Defender exlusions."
        pass
    task_status_col = None
    task_status = "Done."
    sleep(2)
    task_status = ""


def get_status_widget_color(task_status):
    if task_status:
        if any(sub in task_status for sub in ("error", "failed")):
            return ImRed, "Error"
        if is_any_thread_alive():
            return ImYellow, "Busy"
    return ImGreen, "Ready"


def animate_icon():
    global busy_icon

    while True:
        sleep(0.1)
        if is_any_thread_alive():
            busy_icon = Icons.hourglass_1
            sleep(0.1)
            busy_icon = Icons.hourglass_2
            sleep(0.1)
            busy_icon = Icons.hourglass_3
            sleep(0.1)
            busy_icon = Icons.hourglass_4
            sleep(0.1)
            busy_icon = Icons.hourglass_5


Thread(target=animate_icon, daemon=True).start()


def check_for_ylp_update():
    global task_status
    global task_status_col
    global ylp_update_avail
    global ylp_remote_ver
    global ylp_update_active

    task_status_col = None
    task_status = "Checking for Launchpad updates, please wait..."
    ylp_update_active = True
    remote_version = utils.get_launchpad_version()
    LOG.info("Checking for Launchpad updates...")
    if remote_version is not None:
        ylp_remote_ver = remote_version
        try:
            if APP_VERSION < remote_version:
                LOG.info("Update available!")
                task_status_col = ImGreen
                task_status = f"Update {remote_version} is available."
                gui.toast(f"YimLaunchpad v{remote_version} is available.")
                ylp_update_avail = True
            elif APP_VERSION == remote_version:
                LOG.info(
                    f"No updates were found! v{APP_VERSION} is the latest version."
                )
                task_status_col = None
                task_status = (
                    f"No updates were found! v{APP_VERSION} is the latest version."
                )
                ylp_update_avail = False
            elif APP_VERSION > remote_version:
                ylp_update_avail = False
                if is_dev:
                    task_status_col = None
                    task_status = "Dev branch."
                    LOG.debug(f"Superior version numbers are ignored in dev branches.")
                else:
                    LOG.error(
                        f"Local YimLaunchpad version is {APP_VERSION}. This is not a valid version! Are you a dev or a skid?"
                    )
                    task_status_col = ImRed
                    task_status = "Invalid version detected! Please download YimLaunchpad from the official Github repository."
        except Exception:
            task_status_col = ImRed
            ylp_update_active = False
            task_status = "An error occured while attempting to check for updates. Check the log for more details."
            LOG.error(f"An error occured!")
            ylp_update_avail = False
    else:
        task_status_col = ImRed
        task_status = (
            "Failed to get the latest GitHub version! Check the log for more details."
        )
    sleep(3)
    task_status_col = None
    task_status = ""
    ylp_update_active = False


def check_for_yim_update():
    global task_status
    global yim_update_avail
    global task_status_col
    global yim_update_active

    task_status = "Checking for YimMenu updates..."
    yim_update_active = True
    remote_sha = utils.get_remote_checksum()
    local_sha = utils.calc_file_checksum(YIMDLL_FILE)
    if remote_sha is not None and local_sha is not None:
        try:
            if local_sha != remote_sha:
                LOG.info("Update available!")
                task_status = "Update available."
                gui.toast("A new update for YimMenu is available.")
                task_status_col = ImGreen
                yim_update_avail = True
            else:
                LOG.info("No updates were found! YimMenu is up to date.")
                task_status = "No updates were found! YimMenu is up to date."
                yim_update_avail = False
        except Exception:
            task_status = "An error occured while attempting to check for updates! Check the log for more details."
            LOG.error(f"An error occured!")
            yim_update_avail = False
    else:
        task_status_col = ImRed
        task_status = "Failed to check for updates! See the log for more details."
    sleep(3)
    task_status_col = None
    yim_update_active = False
    task_status = ""


def check_saved_config():
    saved_config: dict = utils.read_cfg(CONFIG_PATH)
    if len(saved_config) != len(default_cfg):
        try:
            if len(saved_config) < len(default_cfg):
                for key in default_cfg:
                    if key not in saved_config:
                        saved_config.update({key: default_cfg[key]})
                        LOG.info(f'Added missing config key: "{key}".')
            elif len(saved_config) > len(default_cfg):
                for key in saved_config.copy():
                    if key not in default_cfg:
                        del saved_config[key]
                        LOG.info(f'Removed stale config key: "{key}".')
            utils.save_cfg(CONFIG_PATH, saved_config)
        except Exception:
            LOG.error("An error occured!")
    dummy_progress()


def check_saved_dlls():
    global task_status

    if utils.read_cfg_item(CONFIG_PATH, "custom_dlls"):
        dll_files: list = utils.read_cfg_item(CONFIG_PATH, "dll_files")
        if len(dll_files) > 0:
            for entry in dll_files.copy():
                if not os.path.exists(entry["path"]):
                    dll_files.remove(entry)
                    LOG.info(
                        f"Removed {os.path.basename(entry["path"])} because it no longer exists."
                    )
            utils.save_cfg_item(CONFIG_PATH, "dll_files", dll_files)


def yimlaunchapd_init():
    global yim_update_avail
    global task_status
    global task_status_col
    global default_cfg
    global dll_files

    task_status = "Initializing YimLaunchpad, please wait..."
    if not os.path.exists(CONFIG_PATH):
        utils.save_cfg(CONFIG_PATH, default_cfg)

    if not pending_update:
        task_status_col = None
        task_status = "Checking for YimLaunchpad updates..."
        check_for_ylp_update()
    task_status_col = None
    if os.path.exists(YIMDLL_FILE):
        task_status = "Checking for YimMenu updates..."
        check_for_yim_update()
    task_status = "Verifying YimLaunchpad config..."
    LOG.info("Verifying YimLaunchpad config...")
    check_saved_config()
    check_saved_dlls()
    task_status_col = None
    task_status = ""
    LOG.info("Initialization complete.")


start_thread("ylp_init_thread", yimlaunchapd_init)


def download_yim_menu():
    global task_status
    global progress_value
    global task_status_col
    global yim_update_avail
    global yim_update_active

    if not os.path.exists(YIMDLL_PATH):
        os.makedirs(YIMDLL_PATH)
    try:
        task_status = "Downloading YimMenu Nightly..."
        yim_update_active = True
        with requests.get(NIGHTLY_URL, stream=True) as response:
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
                    task_status = "File was removed!\nMake sure to either turn off your antivirus or add YimLaunchpad to exceptions in your anti-virus."
                    LOG.error("The dll was removed by antivirus!")
            else:
                task_status = "Download failed! Check the log for more details."
                LOG.error(
                    f"Download failed! The request returned status code: {response.status_code}"
                )
    except Exception:
        LOG.error(f"An error occured while trying to download YimMenu.")
        task_status_col = ImRed
        task_status = "Download failed! Check the log for more details."
    sleep(5)
    task_status_col = None
    task_status = ""
    progress_value = 0
    yim_update_active = False


def download_update():
    global task_status
    global task_status_col
    global progress_value
    global pending_update
    global ylp_remote_ver
    global ylp_remote_ver
    global ylp_update_active

    task_status_col = None
    if ylp_remote_ver is not None:
        try:
            task_status = "Fetching GitHub repository..."

            if not os.path.exists(UPDATE_PATH):
                os.mkdir(UPDATE_PATH)
                os.chmod(UPDATE_PATH, 0o777)

            with requests.get(
                f"{LAUNCHPAD_URL}/releases/download/{ylp_remote_ver}/YimLaunchpad.exe",
                stream=True,
            ) as response:
                response.raise_for_status()
                total_size = int(response.headers.get("content-length", 0))
                current_size = 0
                if response.status_code == 200:
                    LOG.info(f"Downloading YimLaunchpad {ylp_remote_ver}")
                    task_status = f"Downloading YimLaunchpad v{ylp_remote_ver}"
                    ylp_update_active = True
                    with open(f"{UPDATE_PATH}\\YimLaunchpad.exe", "wb") as f:
                        for chunk in response.iter_content(131072):
                            if not gui.glfw.window_should_close(window):
                                f.write(chunk)
                                current_size += len(chunk)
                                progress_value = int(current_size) / int(total_size)
                            else:
                                LOG.warning("Update canceled by the user.")
                                utils.delete_folder(UPDATE_PATH)
                        task_status = "Download complete."
                        for i in range(3):
                            task_status = f"Restarting in ({3 - i}) seconds"
                            sleep(1)
                        pending_update = True
                else:
                    LOG.error(
                        f"An error occured while trying to fetch the update. Status Code: {response.status_code}"
                    )
                    task_status_col = ImRed
                    task_status = (
                        "Failed to download the update. Check the log for more details."
                    )
        except Exception:
            task_status_col = ImRed
            task_status = "An error occured! Check the log for more details"
            LOG.error(f"An error occured!")
            utils.delete_folder(UPDATE_PATH)
            sleep(3)
    progress_value = 0
    task_status = ""
    task_status_col = None
    ylp_update_active = False


def check_lua_repos():
    global repos
    global updatable_luas
    global starred_luas
    global task_status
    global task_status_col
    global git_requests_left

    try:
        task_status_col = None
        task_status = "Fetching repository information from YimMenu-Lua..."
        repos, updatable_luas, starred_luas, git_requests_left = utils.get_lua_repos()

        if git_requests_left == 0:
            task_status_col = ImRed
            task_status = (
                "GitHub API rate limit exceeded! Please try again in a few minutes."
            )

        if len(updatable_luas) > 0:
            gui.toast("Updates are available for some of your installed Lua scripts.")

    except Exception:
        task_status_col = ImRed
        task_status = "An error occured! Check the log for more details."
        LOG.error(f"Failed to get repository information!")
        pass

    sleep(5)
    task_status = ""
    task_status_col = None


def add_file_with_short_sha(file_path: str):
    global dll_files
    global task_status
    global task_status_col

    try:
        task_status_col = None
        file_name = utils.append_short_sha(file_path)

        if utils.is_file_saved(file_name, dll_files):
            task_status = f"File {file_name} already exists."
            sleep(3)
            task_status = ""
            return

        dll_files.append({"name": file_name, "path": file_path})
        utils.save_cfg_item(CONFIG_PATH, "dll_files", dll_files)
        task_status = f"Added file {file_name} to the list of custom DLLs."
    except Exception:
        task_status_col = ImRed
        task_status = "An error occured! Check the log for more details."
    sleep(3)
    task_status_col = None
    task_status = ""


def inject_dll(dll, process_id):
    global task_status
    global task_status_col
    global game_is_running
    global auto_inject
    global auto_exit
    global should_exit

    task_status = "Starting injection..."
    try:
        if process_id != 0:
            if os.path.isfile(dll):
                libHanlde = inject(process_id, dll)
                task_status = f"Injecting {os.path.basename(dll)} into GTA5.exe..."
                LOG.info(f"Injecting {dll} into GTA5.exe...")
                LOG.debug(f"Injected library handle: 0x{libHanlde:X}")
                dummy_progress()
                sleep(1)
                task_status = "Checking if the game is still running after injection..."
                LOG.debug("Checking if the game is still running after injection...")
                try:
                    gta_exit_code = psutil.Process(Scanner.pid).wait(5)
                except psutil.TimeoutExpired:
                    gta_exit_code = 0
                sleep(5)
                if gta_exit_code == 0:
                    task_status = "Done."
                    LOG.debug("Everything seems fine.")
                    sleep(3)
                    task_status = ""
                    if auto_exit:
                        for i in range(3):
                            task_status = f"YimLaunchpad will automatically exit in ({3 - i}) seconds."
                            sleep(1)
                        should_exit = True
                else:
                    str_exit_code = utils.to_hex(gta_exit_code)
                    ntstatus = get_error_message(gta_exit_code)
                    task_status_col = ImRed
                    LOG.warning(
                        f"The game crashed after injection with exit code: {str_exit_code} ({ntstatus})"
                    )
                    task_status = f"The game crashed after injection: {ntstatus}"
            else:
                LOG.error(
                    f"Failed to inject DLL: {os.path.basename(dll)} was not found!"
                )
                task_status = (
                    f"Failed to inject DLL: {os.path.basename(dll)} was not found!"
                )
                task_status_col = ImRed
        else:
            task_status_col = ImRed
            LOG.warning("Injection failed! Process GTA5.exe was not found.")
            task_status = "Process not found! Is the game running?"
    except Exception:
        task_status_col = ImRed
        LOG.critical(f"An exception has occured!")
        task_status = "Injection failed! Check the log for more details."

    if not should_exit:
        sleep(3)
        task_status = ""
        task_status_col = None


def background_worker():
    global task_status
    global task_status_col
    global process_id
    global game_is_running
    global is_menu_injected
    global is_fsl_enabled
    global should_exit
    global game_state_checked
    global can_auto_inject
    global glt_addr
    global gs_addr

    try:
        Scanner.procmon("GTA5.exe", 0.01)
        game_is_running = Scanner.is_process_running()
        if game_is_running:
            process_id = Scanner.pid
            if not is_menu_injected:
                is_menu_injected = Scanner.is_module_loaded("YimMenu.dll")
            if not is_fsl_enabled:
                is_fsl_enabled = Scanner.is_module_loaded("version.dll")
            if auto_inject and not is_menu_injected and not game_state_checked:
                if not glt_addr:
                    glt_addr = Scanner.find_pattern(PTRN_LT)
                if not gs_addr:
                    gs_addr = Scanner.find_pattern(PTRN_GS)

                if gs_addr:
                    try:
                        can_auto_inject = True
                        lifetime = glt_addr.add(0x2).rip().get_dword()
                        gs = gs_addr.add(0x2).rip().add(0x1).get_byte()
                        if lifetime < 10000:
                            task_status_col = None
                            task_status = "Auto-Inject: Waiting for the game to load..."
                            sleep(1)
                        else:
                            if gs == 0 or gs > 5:
                                task_status_col = ImYellow
                                task_status = "Too late to inject YimMenu! You can still manually inject it at your own risk."
                                game_state_checked = True
                                can_auto_inject = False

                            if 0 < gs < 5:
                                task_status_col = None
                                task_status = (
                                    "Auto-Inject: Waiting for the landing page..."
                                )
                                sleep(0.5)
                            elif gs == 5:
                                task_status_col = None
                                for i in range(3):
                                    task_status = f"YimMenu will be automatically injected in ({3 - i})"
                                    sleep(1)
                                if not is_menu_injected and gs == 5:
                                    inject_dll(YIMDLL_FILE, process_id)
                                    is_menu_injected = True  # don't wait for the thread to find YimMenu's module
                                    game_state_checked = True
                                    can_auto_inject = False
                    except Exception:
                        LOG.error("An error occured!")
                        game_state_checked = True
                        can_auto_inject = False
        else:
            is_menu_injected = False
            is_fsl_enabled = False
            game_state_checked = False
            can_auto_inject = False
    except Exception:
        LOG.critical(f"Failed to find the game's process!")


def start_gta(idx: int):
    global task_status, task_status_col

    exit_code = 0
    try:
        LOG.info(f"Attempting to launch GTA V through {LAUNCHERS[idx]}.")
        task_status = (
            f"Attempting to launch GTA V through {LAUNCHERS[idx]}, please wait..."
        )
        if idx == 1:
            exit_code = utils.run_detached_process(utils.EGS_LAUNCH_CMD)
        elif idx == 2:
            rgl_path = utils.get_rgl_path()
            if rgl_path is not None:
                exec_file = os.path.abspath(os.path.join(rgl_path, "PlayGTAV.exe"))
                exit_code = utils.run_detached_process(exec_file)
            else:
                task_status = "Failed to find Rockstar Games version of GTA V!"
                task_status_col = ImRed
        elif idx == 3:
            exit_code = utils.run_detached_process(utils.STEAM_LAUNCH_CMD)

        sleep(3)
        if exit_code == 0:
            timeout = time() + 60
            task_status = "Waiting for the game to start..."
            while not game_is_running:
                sleep(0.1)
                if timeout <= time():
                    task_status = "Timed out while waiting for the game to start."
                    break
        else:
            LOG.error(f"Failed to start the game!")
            task_status_col = ImYellow
            task_status = f"Unable to start the game!"
    except Exception:
        LOG.error(f"Failed to start the game!")
        task_status_col = ImYellow
        task_status = f"Unable to start the game!"

    if not auto_inject:
        sleep(3)
        task_status = ""
        task_status_col = None


def lua_download_regular(repo: Repository, path="", base_path=""):
    global task_status
    global task_status_col
    global progress_value

    contents = repo.get_contents(path)
    is_empty = True
    for content in contents:
        progress_value += 1 / len(contents)
        if content.type == "dir":
            lua_download_regular(
                repo, content.path, base_path if base_path else content.path
            )
        elif content.type == "file" and content.name.endswith(".lua"):
            is_empty = False
            rel_path = (
                os.path.relpath(content.path, base_path) if base_path else content.path
            )
            save_path = os.path.join(YIM_SCRIPTS_PATH, repo.name, rel_path)
            os.makedirs(os.path.dirname(save_path), exist_ok=True)
            task_status = f"Downloading {content.path} from {repo.name}"
            lua_content = requests.get(content.download_url).text
            with open(save_path, "w", encoding="utf-8") as f:
                f.write(lua_content)

    if is_empty:
        task_status_col = ImRed
        task_status = "This repository doesn't have any Lua files."
        sleep(3)
        task_status_col = None
        task_status = ""


def lua_download_release(repo: Repository):
    global task_status
    global task_status_col
    global progress_value

    is_empty = True
    releases = repo.get_releases()
    if releases.totalCount == 0:
        return False

    latest_release = releases[0]
    for asset in latest_release.get_assets():
        if str(asset.name).endswith(".zip"):
            response = requests.get(asset.browser_download_url)
            if response.status_code == 200:
                task_status = f"Downloading {repo.name} {latest_release.title}"
                with zipfile.ZipFile(io.BytesIO(response.content)) as zip_file:
                    for file in zip_file.namelist():
                        progress_value += 1 / len(zip_file.namelist())
                        if file.endswith(".lua"):
                            is_empty = False
                            extract_path = os.path.join(
                                YIM_SCRIPTS_PATH, repo.name, file
                            )
                            os.makedirs(os.path.dirname(extract_path), exist_ok=True)
                            with open(extract_path, "wb") as f:
                                f.write(zip_file.read(file))
                    if is_empty:
                        task_status_col = ImRed
                        task_status = "This release doesn't have any Lua files."
                        sleep(3)
                        task_status_col = None
                        task_status = ""
                        return False

                    return True
    return False


def lua_download(repo: Repository):
    global task_status, progress_value

    if not lua_download_release(repo):
        task_status = f"No suitable release found for {repo.name}! Trying to manually download the script..."
        sleep(2)
        lua_download_regular(repo)
    task_status = "Download complete."
    sleep(3)
    progress_value = 0
    task_status = ""


def refresh_lua_repo(repo: Repository, index: int):
    global task_status
    global git_requests_left
    global repos

    task_status = f"Refreshing {repo.name}, please wait..."
    refreshed_repo, requests_remaining = utils.refresh_repository(repo)
    git_requests_left = requests_remaining
    repos[index] = refreshed_repo
    installed_scripts = utils.get_installed_scripts()
    dummy_progress()
    task_status = "Done."
    if utils.lua_script_needs_update(refreshed_repo, installed_scripts):
        gui.toast(f"An update is available for {refreshed_repo.name}")
    sleep(1)
    task_status = ""


def run_task_status_update(msg="", color=None, timeout=2):
    start_thread(
        "status_update_thread",
        set_task_status,
        msg,
        color,
        timeout,
    )


def run_ylp_update_check():
    start_thread("ylp_ver_thread", check_for_ylp_update)


def run_ylp_update():
    start_thread("ylp_down_thread", download_update)


def run_yim_update_check():
    start_thread("yim_ver_thread", check_for_yim_update)


def run_yim_download():
    start_thread("yim_down_thread", download_yim_menu)


def run_lua_repos_check():
    start_thread("lua_repos_thread", check_lua_repos)


def run_inject_thread(path, process):
    start_thread("inject_thread", inject_dll, path, process)


def run_lua_download(repo: Repository):
    start_thread(
        "lua_download_thread",
        lua_download,
        repo,
    )


def OnDraw():
    global window
    global task_status
    global dll_files
    global process_id
    global game_is_running
    global repos
    global updatable_luas
    global auto_inject
    global auto_exit
    global launchpad_console

    custom_dlls = utils.read_cfg_item(CONFIG_PATH, "custom_dlls")
    dll_files = utils.read_cfg_item(CONFIG_PATH, "dll_files")
    git_logged_in = utils.read_cfg_item(CONFIG_PATH, "git_logged_in")
    git_username = utils.read_cfg_item(CONFIG_PATH, "git_username")
    yim_debug_settings = utils.read_cfg_item(YIM_SETTINGS, "debug")
    yim_lua_settings = utils.read_cfg_item(YIM_SETTINGS, "lua")
    dll_index = 0
    launcher_index = 0
    repo_index = 0
    yim_debug_console = (
        yim_debug_settings is not None
        and yim_debug_settings["external_console"]
        or False
    )
    lua_auto_reload = (
        yim_lua_settings is not None
        and yim_lua_settings["enable_auto_reload_changed_scripts"]
        or False
    )
    repos_checked = False
    git_login_active = False
    selected_dll = []
    selected_repo = []
    username_alpha = 0.7

    ImGui.create_context()
    window = gui.new_window(APP_NAME, 400, 555, False)
    impl = GlfwRenderer(window)
    font_scaling_factor = gui.fb_to_window_factor(window)
    io = ImGui.get_io()
    io.fonts.clear()
    io.font_global_scale = 1.0 / font_scaling_factor
    font_config = ImGui.core.FontConfig(merge_mode=True)
    icons_range = ImGui.core.GlyphRanges(
        [
            0xF005,
            0xF007,
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
            0xF0EA,
            0xF0EB,
            0xF110,
            0xF111,
            0xF120,
            0xF121,
            0xF128,
            0xF129,
            0xF134,
            0xF135,
            0xF14C,
            0xF14D,
            0xF1C9,
            0xF1CA,
            0xF1F7,
            0xF1F8,
            0xF250,
            0xF254,
            0,
        ]
    )

    title_font = io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/Rokkitt-Regular.ttf")),
        25 * font_scaling_factor,
    )

    small_font = io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/Rokkitt-Regular.ttf")),
        16.0 * font_scaling_factor,
    )

    main_font = io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/Rokkitt-Regular.ttf")),
        20 * font_scaling_factor,
    )

    io.fonts.add_font_from_file_ttf(
        str(res_path("fonts/fontawesome-webfont.ttf")),
        16 * font_scaling_factor,
        font_config,
        icons_range,
    )

    impl.refresh_font_texture()

    if launchpad_console:
        LOG.show_console()

    if git_logged_in:
        if not os.path.exists(AVATAR_PATH):
            GitOAuth.save_avatar(git_username)
        avatar_texture, _, _ = gui.draw_image(AVATAR_PATH)
    else:
        avatar_texture = 0

    while (
        not gui.glfw.window_should_close(window)
        and not pending_update
        and not should_exit
    ):
        gui.glfw.poll_events()
        impl.process_inputs()
        ImGui.new_frame()
        win_w, win_h = gui.glfw.get_window_size(window)
        ImGui.set_next_window_size(win_w, win_h)
        ImGui.set_next_window_position(0, 0)
        ImGui.push_style_color(ImGui.COLOR_FRAME_BACKGROUND, 0.1, 0.1, 0.1)
        ImGui.push_style_color(ImGui.COLOR_FRAME_BACKGROUND_ACTIVE, 0.3, 0.3, 0.3)
        ImGui.push_style_color(ImGui.COLOR_FRAME_BACKGROUND_HOVERED, 0.5, 0.5, 0.5)
        ImGui.push_style_color(ImGui.COLOR_TAB, 0.097, 0.097, 0.097)
        ImGui.push_style_color(ImGui.COLOR_TAB_ACTIVE, 0.075, 0.075, 0.075)
        ImGui.push_style_color(ImGui.COLOR_TAB_HOVERED, 0.085, 0.085, 0.085)
        ImGui.push_style_color(ImGui.COLOR_HEADER, 0.1, 0.1, 0.1)
        ImGui.push_style_color(ImGui.COLOR_HEADER_ACTIVE, 0.3, 0.3, 0.3)
        ImGui.push_style_color(ImGui.COLOR_HEADER_HOVERED, 0.5, 0.5, 0.5)
        ImGui.push_style_color(ImGui.COLOR_BUTTON, 0.075, 0.075, 0.075)
        ImGui.push_style_color(ImGui.COLOR_BUTTON_ACTIVE, 0.085, 0.085, 0.085)
        ImGui.push_style_color(ImGui.COLOR_BUTTON_HOVERED, 0.1, 0.1, 0.1)
        ImGui.push_style_var(ImGui.STYLE_CHILD_ROUNDING, 5)
        ImGui.push_style_var(ImGui.STYLE_FRAME_ROUNDING, 5)
        ImGui.push_style_var(ImGui.STYLE_ITEM_SPACING, (5, 5))
        ImGui.push_style_var(ImGui.STYLE_ITEM_INNER_SPACING, (5, 5))
        ImGui.push_style_var(ImGui.STYLE_FRAME_PADDING, (5, 5))
        ImGui.push_font(main_font)
        ImGui.begin(
            "Main Window",
            flags=ImGui.WINDOW_NO_TITLE_BAR
            | ImGui.WINDOW_NO_RESIZE
            | ImGui.WINDOW_NO_MOVE,
        )
        with ImGui.begin_child("##YLP", 0, 460):
            if not is_thread_alive("ylp_init_thread"):
                start_thread("bachground_thread", background_worker)
                if ImGui.begin_tab_bar("ylp"):
                    if ImGui.begin_tab_item(
                        f" {Icons.Dashboard}  Dashboard  "
                    ).selected:
                        with ImGui.begin_child("##yimmenu", border=True):
                            with ImGui.font(title_font):
                                gui.separator_text("YimMenu")
                            with ImGui.font(small_font):
                                ImGui.bullet_text("State:")
                                ImGui.same_line()
                                ImGui.text_colored(
                                    (
                                        "Injected."
                                        if is_menu_injected
                                        else "Not Injected."
                                    ),
                                    0 if is_menu_injected else 1,
                                    1 if is_menu_injected else 0,
                                    0,
                                    0.88,
                                )
                            if not yim_update_active:
                                if os.path.isfile(YIMDLL_FILE):
                                    yim_update_btn_label = (
                                        f"{Icons.Refresh}  Check For Updates"
                                        if not yim_update_avail
                                        else f"{Icons.Download}  Update YimMenu"
                                    )
                                    yim_update_btn_callback = (
                                        run_yim_update_check
                                        if not yim_update_avail
                                        else run_yim_download
                                    )
                                    if ImGui.button(yim_update_btn_label):
                                        yim_update_btn_callback()
                                else:
                                    if ImGui.button(
                                        f"{Icons.Download}  Download YimMenu"
                                    ):
                                        run_yim_download()
                            else:
                                gui.busy_button(busy_icon, "Please Wait")
                            ImGui.dummy(1, 5)
                            with ImGui.font(title_font):
                                gui.separator_text("GTA V")
                            with ImGui.font(small_font):
                                ImGui.bullet_text("Game Status:")
                                ImGui.same_line()
                                ImGui.text_colored(
                                    "Running." if is_fsl_enabled else "Not running.",
                                    0 if is_fsl_enabled else 1,
                                    1 if is_fsl_enabled else 0,
                                    0,
                                    0.88,
                                )
                                ImGui.same_line(spacing=5)
                                ImGui.bullet_text("FSL:")
                                gui.tooltip(
                                    "Because FSL is hosted on UnknownCheats, YimLaunchpad can not download it for you. You can, however, click on the 4th icon at the bottom of the UI to directly visit FSL's unknowncheats thead in your browser.",
                                    main_font,
                                    0.9,
                                )
                                ImGui.same_line()
                                if game_is_running:
                                    fsl_label = (
                                        "Enabled." if is_fsl_enabled else "Disabled."
                                    )
                                else:
                                    fsl_label = "Unknown"
                                ImGui.text_colored(
                                    fsl_label,
                                    0 if is_fsl_enabled else 1,
                                    1 if is_fsl_enabled else 0,
                                    0,
                                    0.88,
                                )
                                ImGui.dummy(1, 5)
                            if game_is_running:
                                with gui.begin_disabled(
                                    is_menu_injected or can_auto_inject
                                ):
                                    if (
                                        ImGui.button(f"{Icons.Rocket}  Inject YimMenu")
                                        and not is_menu_injected
                                        and not can_auto_inject
                                    ):
                                        run_inject_thread(YIMDLL_FILE, process_id)
                                if is_menu_injected or can_auto_inject:
                                    gui.tooltip(
                                        "YimMenu is already injected."
                                        if is_menu_injected
                                        else "Auto-Inject is enabled. You can not manually inject YimMenu until the Auto-Inject checks are completed."
                                    )
                                else:
                                    ImGui.same_line()
                                    dc_clicked, yim_debug_console = ImGui.checkbox(
                                        "Debug Console", yim_debug_console
                                    )
                                    gui.tooltip(
                                        "If you want to hide/show YimMenu's debug console, do it before injecting."
                                    )
                                    if dc_clicked:
                                        yim_debug_settings["external_console"] = (
                                            not yim_debug_console
                                        )
                                        utils.save_cfg_item(
                                            YIM_SETTINGS, "debug", yim_debug_settings
                                        )
                            else:
                                if is_thread_alive("launch_thread"):
                                    gui.busy_button(busy_icon, "Please Wait...")
                                else:
                                    ImGui.set_next_item_width(200)
                                    _, launcher_index = ImGui.combo(
                                        "##launchers", launcher_index, LAUNCHERS
                                    )
                                    ImGui.same_line()
                                    if ImGui.button(f" {Icons.Play}  Run "):
                                        if launcher_index == 0:
                                            run_task_status_update(
                                                "Please select a launcher from the list!",
                                                ImYellow,
                                                2,
                                            )
                                        else:
                                            start_thread(
                                                "launch_thread",
                                                start_gta,
                                                launcher_index,
                                            )

                            ImGui.dummy(1, 5)
                            cdll_clicked, custom_dlls = ImGui.checkbox(
                                "Enable Custom DLLs", custom_dlls
                            )
                            if cdll_clicked:
                                utils.save_cfg_item(
                                    CONFIG_PATH, "custom_dlls", custom_dlls
                                )
                            if custom_dlls:
                                with ImGui.begin_child("##dll_list", 330, 160, True):
                                    ImGui.text(f"{Icons.List}  DLL List:")
                                    ImGui.separator()
                                    if len(dll_files) > 0:
                                        for i in range(len(dll_files)):
                                            file_selected = dll_index == i
                                            clicked, file_selected = ImGui.selectable(
                                                dll_files[i]["name"],
                                                file_selected,
                                                0,
                                                0,
                                            )
                                            if ImGui.is_item_hovered():
                                                rect_x, _ = ImGui.get_item_rect_size()
                                                if rect_x > 330:
                                                    gui.tooltip(dll_files[i]["name"])
                                                if ImGui.is_mouse_double_clicked():
                                                    utils.open_folder(
                                                        dll_files[i]["path"]
                                                    )
                                            if clicked:
                                                dll_index = i
                                                ImGui.set_item_default_focus()
                                        selected_dll = dll_files[dll_index]
                                ImGui.same_line(spacing=10)
                                with ImGui.begin_child("##buttons"):
                                    ImGui.dummy(1, 20)
                                    if ImGui.button(Icons.Plus):
                                        file_path = gui.start_file_dialog(
                                            "DLL\0*.dll;\0", False
                                        )
                                        if file_path is not None:
                                            start_thread(
                                                "file_add_thread",
                                                add_file_with_short_sha,
                                                file_path,
                                            )
                                    gui.tooltip("Add custom DLL.")
                                    if len(dll_files) > 0:
                                        if ImGui.button(Icons.Minus):
                                            dll_files.remove(selected_dll)
                                            utils.save_cfg_item(
                                                CONFIG_PATH, "dll_files", dll_files
                                            )
                                            if dll_index > 0:
                                                dll_index -= 1
                                        gui.tooltip("Remove custom DLL.")
                                    if game_is_running:
                                        if ImGui.button(Icons.Rocket):
                                            if os.path.exists(selected_dll["path"]):
                                                run_inject_thread(
                                                    selected_dll["path"], process_id
                                                )
                                            else:
                                                run_task_status_update(
                                                    f"File not found: {selected_dll["path"]}.",
                                                    ImRed,
                                                    5,
                                                )
                                                dll_files.remove(selected_dll)
                                                utils.save_cfg_item(
                                                    CONFIG_PATH, "dll_files", dll_files
                                                )
                                        gui.tooltip("Inject custom DLL into GTA5.exe")
                        ImGui.end_tab_item()

                    if ImGui.begin_tab_item(f" {Icons.Code} Lua Scripts ").selected:
                        git_logged_in = utils.read_cfg_item(
                            CONFIG_PATH, "git_logged_in"
                        )
                        git_username = utils.read_cfg_item(CONFIG_PATH, "git_username")
                        if not repos_checked:
                            run_lua_repos_check()
                            Thread(target=fetch_luas_progress, daemon=True).start()
                            repos_checked = True
                        with ImGui.begin_child("##Lua", border=True):
                            if not is_thread_alive("lua_repos_thread"):
                                if ImGui.button(f"{Icons.Refresh}  Refresh List"):
                                    run_lua_repos_check()
                                    Thread(
                                        target=fetch_luas_progress, daemon=True
                                    ).start()
                                if git_requests_left < 30:
                                    gui.tooltip(
                                        f"Please do not spam this. You currently have {git_requests_left} request left."
                                    )
                                ImGui.same_line(spacing=10)
                                ImGui.bullet_text(
                                    f"API Requests Left: [{git_requests_left}]"
                                )
                            else:
                                gui.busy_button(busy_icon, "Please Wait")

                            if not is_menu_injected:
                                lar_clicked, lua_auto_reload = ImGui.checkbox(
                                    "Auto-Reload Lua Scripts", lua_auto_reload
                                )
                                if lar_clicked:
                                    yim_lua_settings[
                                        "enable_auto_reload_changed_scripts"
                                    ] = lua_auto_reload
                                    utils.save_cfg_item(
                                        YIM_SETTINGS, "lua", yim_lua_settings
                                    )
                            else:
                                ImGui.dummy(1, 2)
                                ImGui.text_disabled(
                                    f"{Icons.Close} Auto-Reload Lua Scripts"
                                )

                            if (
                                not is_thread_alive("lua_repos_thread")
                                and len(repos) > 0
                            ):
                                ImGui.dummy(1, 10)
                                ImGui.text(f"{Icons.GitHub}  Lua Repositories")
                                ImGui.separator()
                                with ImGui.begin_child("##repos", 0, 235):
                                    for i in range(len(repos)):
                                        repo_selected = repo_index == i
                                        clicked, repo_selected = ImGui.selectable(
                                            repos[i].name, repo_selected, 0, 0
                                        )
                                        if (
                                            ImGui.is_item_hovered()
                                            and ImGui.is_mouse_double_clicked()
                                        ):
                                            utils.visit_url(repos[i].html_url)
                                        if clicked:
                                            repo_index = i
                                            ImGui.set_item_default_focus()
                                        item_width = ImGui.calc_text_size(
                                            repos[i].name
                                        ).x
                                        ImGui.same_line(spacing=400 - item_width - 135)
                                        script_has_update = (
                                            utils.does_script_have_updates(
                                                repos[i].name, updatable_luas
                                            )
                                        )
                                        ImGui.text_colored(
                                            Icons.Down,
                                            0.0,
                                            0.588,
                                            1.0,
                                            (0.7 if script_has_update else 0.0),
                                        )
                                        if script_has_update:
                                            gui.tooltip("Update available.", small_font)
                                        ImGui.same_line(spacing=20)
                                        if utils.is_repo_starred(
                                            repos[i].name, starred_luas
                                        ):
                                            ImGui.text_colored(Icons.Star, 1, 1, 0, 0.7)
                                        else:
                                            ImGui.text(Icons.Star_2)
                                        ImGui.same_line()
                                        with ImGui.font(small_font):
                                            ImGui.text(str(repos[i].stargazers_count))
                                    selected_repo: Repository = repos[repo_index]
                                ImGui.separator()
                                ImGui.dummy(1, 5)
                                if not utils.is_script_installed(
                                    selected_repo
                                ) and not utils.is_script_disabled(selected_repo):
                                    if ImGui.button(f"{Icons.Download} Download"):
                                        run_lua_download(selected_repo)
                                    ImGui.same_line()
                                else:
                                    if utils.does_script_have_updates(
                                        selected_repo.name, updatable_luas
                                    ) and not utils.is_script_disabled(selected_repo):
                                        if ImGui.button(f"{Icons.Down} Update"):
                                            run_lua_download(selected_repo)
                                        ImGui.same_line()
                                    if ImGui.button(f"{Icons.Trash} Delete"):
                                        ImGui.open_popup(f"delete {selected_repo.name}")
                                    gui.message_box(
                                        f"delete {selected_repo.name}",
                                        "Are you sure?",
                                        title_font,
                                        1,
                                        utils.delete_folder,
                                        os.path.join(
                                            YIM_SCRIPTS_PATH, selected_repo.name
                                        ),
                                        run_task_status_update,
                                        "Access denied! Try running YimLaunchpad as admin.",
                                        ImRed,
                                        5,
                                    )
                                    ImGui.same_line()
                                if utils.is_script_installed(selected_repo):
                                    if ImGui.button(f"{Icons.Close} Disable"):
                                        try:
                                            old_path = os.path.join(
                                                YIM_SCRIPTS_PATH, selected_repo.name
                                            )
                                            disabled_path = os.path.join(
                                                os.path.join(
                                                    YIM_SCRIPTS_PATH, "disabled"
                                                ),
                                                selected_repo.name,
                                            )
                                            os.rename(old_path, disabled_path)
                                        except OSError:
                                            run_task_status_update(
                                                "This script already exists in the disabled folder.",
                                                ImYellow,
                                                3,
                                            )
                                elif utils.is_script_disabled(selected_repo):
                                    if ImGui.button(f"{Icons.Checkmark} Enable"):
                                        try:
                                            old_path = os.path.join(
                                                os.path.join(
                                                    YIM_SCRIPTS_PATH, "disabled"
                                                ),
                                                selected_repo.name,
                                            )
                                            enabled_path = os.path.join(
                                                YIM_SCRIPTS_PATH, selected_repo.name
                                            )
                                            os.rename(old_path, enabled_path)
                                        except OSError:
                                            run_task_status_update(
                                                "This script already exists in the scripts folder.",
                                                ImYellow,
                                                3,
                                            )
                                ImGui.same_line()
                                if (
                                    utils.is_script_installed(selected_repo)
                                    and not utils.is_script_disabled(selected_repo)
                                    and not is_thread_alive("lua_downl_thread")
                                    and not is_thread_alive("lua_repos_thread")
                                ):
                                    if not is_thread_alive("refresh_repo_thread"):
                                        if ImGui.button(f" {Icons.Refresh} "):
                                            start_thread(
                                                "refresh_repo_thread",
                                                refresh_lua_repo,
                                                selected_repo,
                                                repo_index,
                                            )
                                        gui.tooltip("Refresh repository")
                                    else:
                                        gui.busy_button(busy_icon)
                        ImGui.end_tab_item()

                    if ImGui.begin_tab_item(f"  {Icons.Gear}  Settings  ").selected:
                        with ImGui.begin_child("##settings", border=True):
                            ImGui.push_text_wrap_pos(win_w - 20)
                            ImGui.dummy(1, 5)
                            ylp_update_btn_label = (
                                f"{Icons.Refresh}  Check For Updates"
                                if not ylp_update_avail
                                else f"{Icons.Download}  Download Update"
                            )
                            ylp_update_btn_callback = (
                                run_ylp_update_check
                                if not ylp_update_avail
                                else run_ylp_update
                            )

                            if not ylp_update_active:
                                if ImGui.button(ylp_update_btn_label):
                                    ylp_update_btn_callback()
                            else:
                                gui.busy_button(busy_icon, "Please Wait")

                            ImGui.dummy(1, 5)
                            if ImGui.button(
                                f"{Icons.Folder}  Open YimLaunchpad's Folder"
                            ):
                                utils.open_folder(LAUNCHPAD_PATH)

                            ImGui.dummy(1, 5)
                            if ImGui.button("Add YimLaunchpad To Exclusions"):
                                start_thread(
                                    "defender_exclusions_thead",
                                    add_to_defender_exclusions,
                                )
                            gui.tooltip(
                                "Uses Powershell to add YimLaunchpad.exe and YimLaunchpad's folder to Windows Defender exclusions.\n\nNOTE: If you're using third party anti-virus software, this will not work.",
                                None,
                                0.9,
                            )

                            ImGui.dummy(1, 5)
                            autoexitclicked, auto_exit = ImGui.checkbox(
                                "Auto-Exit", auto_exit
                            )
                            if autoexitclicked:
                                utils.save_cfg_item(
                                    CONFIG_PATH, "auto_exit_after_injection", auto_exit
                                )
                            gui.tooltip(
                                "Automatically close YimLaunchpad after injecting a DLL."
                            )
                            ImGui.same_line(spacing=10)
                            if not can_auto_inject:
                                autoinject_clicked, auto_inject = ImGui.checkbox(
                                    "Auto-Inject", auto_inject
                                )
                                gui.tooltip(
                                    "Automatically detects the loading stage of the game and if it's in the landing page, auto-injects YimMenu."
                                )
                                if autoinject_clicked:
                                    utils.save_cfg_item(
                                        CONFIG_PATH, "auto_inject", auto_inject
                                    )
                            else:
                                ImGui.text_disabled(f"{Icons.Close}  Auto-Inject")
                                gui.tooltip(
                                    "This option can not be interacted with until the Auto-Inject checks are completed."
                                )
                            ImGui.same_line(spacing=10)
                            ylpcon_clicked, launchpad_console = ImGui.checkbox(
                                "Console", launchpad_console
                            )
                            gui.tooltip(
                                f"{launchpad_console and "Disable" or "Enable"} YimLaunchpad's debug console."
                            )
                            if ylpcon_clicked:
                                utils.save_cfg_item(
                                    CONFIG_PATH, "launchpad_console", launchpad_console
                                )
                                if launchpad_console:
                                    LOG.show_console()
                                else:
                                    LOG.hide_console()
                            ImGui.dummy(1, 10)
                            gui.separator_text(f"{Icons.GitHub} GitHub")
                            if not git_logged_in:
                                if not git_login_active:
                                    if ImGui.button("Login To GitHub"):
                                        git_login_active = True
                                        task_status = "Logging in to GitHub..."
                                        start_thread(
                                            "git_login_thread", GitOAuth.login()
                                        )

                                    ImGui.same_line(spacing=10)
                                    if ImGui.button(f"  {Icons.Info}  "):
                                        ImGui.open_popup("git_more_info")
                                    gui.tooltip("Click for more information")
                                    gui.message_box(
                                        "git_more_info",
                                        gui.git_more_info_text,
                                        small_font,
                                        0,
                                    )
                                if is_thread_alive("git_login_thread"):
                                    gui.busy_button(busy_icon, "Logging in...")
                                    ImGui.same_line(spacing=20)
                                    if ImGui.button(f"{Icons.Close}  Abort"):
                                        GitOAuth.abort()
                                        task_status = ""
                                        git_login_active = False
                                    ImGui.dummy(1, 10)
                                    auth_status = GitOAuth.get_status()
                                    ImGui.text(auth_status)
                                    if utils.stringFind((auth_status), "Please visit"):
                                        auth_code = auth_status[-9:]
                                        if ImGui.button(
                                            f"{Icons.Clipboard} Copy Code & Open URL"
                                        ):
                                            ImGui.set_clipboard_text(auth_code)
                                            utils.visit_url(
                                                "https://github.com/login/device"
                                            )
                                        gui.tooltip(
                                            "Press to copy the code and open the url in your browser"
                                        )
                                    if utils.stringFind(auth_status, "successful"):
                                        git_username = utils.read_cfg_item(
                                            CONFIG_PATH, "git_username"
                                        )
                                        git_logged_in = True
                                        git_login_active = False
                                        run_task_status_update(
                                            "Sucessfully logged in to GitHub.",
                                            ImGreen,
                                            3,
                                        )
                                        if avatar_texture == 0:
                                            if not os.path.exists(AVATAR_PATH):
                                                GitOAuth.save_avatar(git_username)
                                            avatar_texture, _, _ = gui.draw_image(
                                                AVATAR_PATH
                                            )
                                            gui.gl.glBindTexture(
                                                gui.gl.GL_TEXTURE_2D, avatar_texture
                                            )
                                        if len(repos) > 0:
                                            repos_checked = False
                                    if utils.stringContains(
                                        auth_status, {"expired", "canceled", "failed"}
                                    ):
                                        run_task_status_update(
                                            auth_status,
                                            ImRed,
                                            3,
                                        )
                                        git_logged_in = False
                                        git_login_active = False

                            else:
                                if avatar_texture != 0 and gui.gl.glIsTexture(
                                    avatar_texture
                                ):
                                    gui.image_rounded(avatar_texture, 80)
                                ImGui.push_style_var(ImGui.STYLE_ALPHA, username_alpha)
                                ImGui.text(f"{Icons.User} {git_username}")
                                ImGui.pop_style_var()
                                if ImGui.is_item_hovered():
                                    gui.tooltip(f"{Icons.Extern_Link} Open in browser")
                                    username_alpha = 1.0
                                    if ImGui.is_item_clicked(0):
                                        utils.visit_url(
                                            f"https://github.com/{git_username}"
                                        )
                                else:
                                    username_alpha = 0.7
                                if ImGui.button(f"{Icons.Close} Logout"):
                                    GitOAuth.logout()
                                    run_task_status_update(
                                        "Disconnected from GitHub", None, 3
                                    )
                                    git_username = ""
                                    git_logged_in = False
                                    avatar_texture = 0
                                    ImGui.open_popup("git_diconnect")
                            gui.message_box(
                                "git_diconnect",
                                "You have logged out of GitHub on this app. To fully revoke access, visit https://github.com/settings/apps/authorizations.\n\nWould you like to go there now?",
                                None,
                                1,
                                utils.visit_url,
                                "https://github.com/settings/apps/authorizations",
                            )
                            ImGui.pop_text_wrap_pos()
                        ImGui.end_tab_item()
                    ImGui.end_tab_bar()

        ImGui.spacing()
        with ImGui.begin_child("##feedback", 0, 40):
            status_col, status = get_status_widget_color(task_status)
            ImGui.text_colored(
                Icons.Circle, status_col[0], status_col[1], status_col[2], 0.8
            )
            gui.tooltip(status, small_font)
            ImGui.push_text_wrap_pos(win_w - 15)
            with ImGui.font(small_font):
                ImGui.same_line()
                gui.status_text(task_status, task_status_col)
            ImGui.pop_text_wrap_pos()
            if progress_value > 0:
                ImGui.progress_bar(progress_value, (380, 5))

        clickable_icon_size = ImGui.calc_text_size(Icons.GitHub)
        padding = (win_w - clickable_icon_size.x) / 5
        gui.clickable_icon(
            Icons.GitHub,
            small_font,
            "Click to visit YimMenu's GitHub repository",
            utils.visit_url,
            "https://github.com/Mr-X-GTA/YimMenu",
        )
        ImGui.same_line(spacing=padding)
        gui.clickable_icon(
            Icons.Extern_Link,
            small_font,
            "Click to visit YimMenu's unknowncheats thread",
            utils.visit_url,
            "https://www.unknowncheats.me/forum/grand-theft-auto-v/476972-yimmenu-1-69-b3351.html",
        )
        ImGui.same_line(spacing=padding)
        gui.clickable_icon(
            Icons.GitHub,
            small_font,
            "Click to visit YimLaunchpad's GitHub repository",
            utils.visit_url,
            "https://github.com/xesdoog/YimLaunchpad",
        )
        ImGui.same_line(spacing=padding)
        gui.clickable_icon(
            Icons.Extern_Link,
            small_font,
            "Click to visit FSL's unknowncheats thread",
            utils.visit_url,
            "https://www.unknowncheats.me/forum/grand-theft-auto-v/616977-fsl-local-gtao-saves.html",
        )
        ImGui.same_line(spacing=padding)
        gui.clickable_icon(
            Icons.GitHub,
            small_font,
            "Click to visit the YimMenu-Lua GitHub organization",
            utils.visit_url,
            "https://github.com/YimMenu-Lua",
        )

        ImGui.pop_font()
        ImGui.pop_style_var(5)
        ImGui.pop_style_color(12)
        ImGui.end()

        gui.gl.glClearColor(1.0, 1.0, 1.0, 1)
        gui.gl.glClear(gui.gl.GL_COLOR_BUFFER_BIT)
        ImGui.render()
        impl.render(ImGui.get_draw_data())
        gui.glfw.swap_buffers(window)

    if status_col != ImGreen:
        if is_thread_alive("git_login_thread"):
            GitOAuth.abort()
        threadpool.shutdown()
    gui.gl.glDeleteTextures([avatar_texture])
    impl.shutdown()
    gui.glfw.terminate()


@atexit.register
def OnExit():
    LOG.info("Closing YimLaunchpad...\n\nFarewell!")


if __name__ == "__main__":
    try:
        if getattr(sys, "frozen", False):
            pyi_splash.close()
        OnDraw()
    finally:
        if pending_update:
            utils.run_updater()
