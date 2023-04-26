# TetaLab test task

## ESP-IDF Install Guide

1. The easiest way to install ESP-IDF’s prerequisites is to download one of ESP-IDF Tools Installers.<br>
[https://dl.espressif.com/dl/esp-idf/?idf=4.4][https://dl.espressif.com/dl/idf-installer/espressif-ide-setup-2.8.1-with-esp-idf-4.4.4.exe]<br><br>
!! YOU NEED ESP-IDF V4.4.4 !! This is the stable version

2. Launching ESP-IDF Environment<br>
The installer will launch ESP-IDF environment in selected prompt.<br><br>
"Run ESP-IDF PowerShell Environment" or "Run ESP-IDF Command Prompt (cmd.exe)"<br><br>
Using the Command Prompt<br>
For the remaining Getting Started steps, we’re going to use the Windows Command Prompt.<br>
ESP-IDF Tools Installer also creates a shortcut in the Start menu to launch the ESP-IDF Command Prompt. This shortcut launches the Command Prompt (cmd.exe) and runs export.bat script to set up the environment variables (PATH, IDF_PATH and others). Inside this command prompt, all the installed tools are available.<br>
Note that this shortcut is specific to the ESP-IDF directory selected in the ESP-IDF Tools Installer. If you have multiple ESP-IDF directories on the computer (for example, to work with different versions of ESP-IDF), you have two options to use them:

3. Download the repository and save it in a place convenient for you

4. cd %project directory% 

5. Now connect your ESP32 board to the computer and check under which serial port the board is visible.
Serial port names start with COM in Windows.*

6. Build project <br>
>idf.py build 

7. Flash project to your board<br>
>idf.py -p %PORT% flash

8. Monitor the Output
>idf.py -p %PORT% monitor

<br><br>
### For tests on the phone, I recommend using "nRF connect" or "BLE scanner"