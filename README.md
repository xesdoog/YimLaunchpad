# YimLaunchpad
A launchpad for YimMenu.

![ylp_splash](https://github.com/user-attachments/assets/0acf2233-078a-4cce-a0a7-d7b84d91682b)

###  

> [!WARNING]
> Avoid spamming the Lua Scripts tab *(refreshing the list multiple times, closing and relaunching the app, etc...)*. The GitHub API has a rate limit for non-authenticated requests and will put you on a timeout if you exceed that limit.
>
> If you want to increase the limit to 5k requests, you have the option to login to GitHub through YimLaunchpad using a device code.

###  

## Features

### YimMemu & Other DLLs

- Download YimMenu.
- Keep it updated (auto-checks for updates on launch and sends toast notifications).
- Enable/Disable YimMenu's debug console. **(Do not change it if the menu is already injected)**
- Add custom DLLs.
- Start the game.
- Inject DLLs into the game's process.
- Auto-Exit after injecting any DLL.
- Checks modules loaded by the game to determine if YimMenu and FSL are loaded.

### Lua Scripts

- Download Lua scripts from [YimMenu-Lua](https://github.com/YimMenu-Lua)
- Enable/Disable scripts.
- Delete scripts.
- Enable/Disable auto-reloading of Lua scripts **(Same as the option in YimMenu. Do not change it if the menu is already injected)**
- Auto-detects script updates when fetching repositories and sends toast notifications.

> [!IMPORTANT]
> To benefit from the Lua Scripts features, you have to download the scripts using the launcher. The reason for that is simply because YimLaunchpad organizes your scripts into folders named after the script's GitHub repository. This allows it to track your enabled/disabled scripts, check the last time a script was updated and compare it to the repository, etc...
>
> Loose files and folders named anything other than the script's repository name will not be recognized as installed scripts.

### Settings

- Manually check for updates and update the launchpad.
- Open the launchpad's folder which is located in `%AppData%\YimLaunchpad`.
- Add `YimLaunchpad.exe` and its parent folder to Windows Defender exclusions using Windows Powershell™.
- Enable/disable `Auto-Exit`.
- Enable/Disable YimLaunchpad's debug console.
- Login/logout to/from GitHub.

###  

## TODO

- [ ] Add themes. *meh*
- [x] Add automatic injection.
- [x] Refresh installed Lua scripts individually to check for updates or update the selected repo's star count.

###  

## Showcase

### Dashboard

![dashboard_x3](https://github.com/user-attachments/assets/f4ed5ecf-3d10-40ad-b86d-85146d071ef3)

### Lua Scripts (Without GitHub Auth)

![lua](https://github.com/user-attachments/assets/866f2dde-b743-43e7-a265-df3dd62ad08d)

### Settings (Without GitHub Auth)

![settings](https://github.com/user-attachments/assets/2c53073f-8424-4bcc-b273-9d17da324d1e)

### Lua Scripts (With GitHub Auth)

![lua_1](https://github.com/user-attachments/assets/32a292e9-54f0-4bff-aeeb-e86b11e70ec5)

### Settings (With GitHub Auth)

![settings_1](https://github.com/user-attachments/assets/95038fea-a22d-4ee4-b955-41b9ebcb6899)

###  

## Licenses

- This program uses Google's Rokkit Regular font, licensed under [SIL Open Font License V1.1](https://openfontlicense.org/open-font-license-official-text/)
- This program uses the free version of FontAwesome v4.7 also licensed under [SIL Open Font License V1.1](https://openfontlicense.org/open-font-license-official-text/)
