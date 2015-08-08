Systemd-kcm
===========

Systemd control module for KDE. Provides a graphical frontend for the systemd
daemon, which allows for viewing and controlling systemd units and logind
sessions as well as modifying configuration files.
Integrates in the System Settings dialogue in KDE Plasma.


Installation
------------
    mkdir build  
    cd build  
    cmake -DCMAKE_INSTALL_PREFIX=\`kf5-config --prefix\` ..  
    make  
    make install  


Dependencies
------------
*   Qt >= 5.2
*   KF5Auth
*   KF5ConfigWidgets
*   KF5CoreAddons
*   KF5Crash
*   KF5I18n
*   KF5KIO
*   KF5WidgetsAddons
*   Systemd

For compilers not supporting C++11, Boost >= 1.45 is needed as well.


Execution
---------
Systemd-kcm can be accessed through System Settings, or by issuing the command:
`kcmshell5 kcm_systemd`


Developed by:
* Ragnar Thomsen <rthomsen6@gmail.com>

Contributions from:
* Johan Ouwerkerk <jm.ouwerkerk@gmail.com>
* Alexandre Detiste <alexandre@detiste.be>

Website:
https://projects.kde.org/projects/playground/sysadmin/systemd-kcm/

Please report bugs and feature requests at:
https://bugs.kde.org/

Git repository can be found at:
http://quickgit.kde.org/?p=systemd-kcm.git
