# YimLaunchpad
A launchpad for YimMenu.

![ylp_splash](https://github.com/user-attachments/assets/0acf2233-078a-4cce-a0a7-d7b84d91682b)

###  

> [!NOTE]
> **Work In Progress**. Uploaded for testing purposes.
> 
> General stuff like downloading and keeping YimMenu up-to-date, launching the game, injecting the dll, downloading Lua scripts all work.
>
> Further improvements will be implemented.

###  

> [!WARNING]
> Avoid spamming the Lua Scripts tab *(refreshing the list multiple times, closing and relaunching the app, etc...)*. The GitHub API has a rate limit for non-authenticated requests and will put you on a timeout if you exceed that limit.

## Features

### YimMemu & Other DLLs

- Download YimMenu.
- Keep it updated (auto-checks for updates on launch and sends toast notifications).
- Enable/Disable YimMenu's debug console. **(Do not change it if the menu is already injected)**
- Add custom DLLs
- Start the game.
- Inject DLLs into the game's process.

## Lua Scripts

- Download Lua scripts from [YimMenu-Lua](https://github.com/YimMenu-Lua)
- Enable/Disable scripts.
- Delete scripts.
- Enable/Disable auto-reloading of Lua scripts **(Same as the option in YimMenu. Do not change it if the menu is already injected)**
- Auto-detects script updates and sends toast notifications. **(Do not abuse this because the GitHub API has a rate limit of 60/h for non-authenticated requests)**

> [!IMPORTANT]
> To benefit from the Lua Scripts feature, you have to download the scripts using the launcher. The reason for that is simply because YimLaunchpad organizes your scripts into folders named after the script's GitHub repository. This allows it to track your enabled/disabled scripts, check the last time a script was updated and compare it to the repository, etc...
>
> Loose files and folders named anything other than the script's repository name will not be recognized as installed scripts.

## Settings

- For now this only has a button to check for updates/update the launchpad and a button to open the launchpad's folder which is located in `%AppData%\YimLaunchpad`. You can find the log there in case you run into some bugs.
- More stuff will be added to this tab in the future.

## Showcase

![image](https://github.com/user-attachments/assets/2868ae11-3210-451f-9e41-691a93d4e330)

![image](https://github.com/user-attachments/assets/d95f89f8-5a0c-49bd-ad06-190f652c6811)

![image](https://github.com/user-attachments/assets/55a3c25d-45f9-4722-9e18-d1a4f4a60746)
