Systemd-kcm
===========

Systemd control module for KDE. Provides a graphical frontend for the systemd 
daemon, which allows for viewing and controlling systemd units and logind sessions 
as well as modifying configuration files.
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
*   Qt >= 5.0
*   KF5ConfigWidgets
*   KF5CoreAddons
*   KF5I18n
*   KF5Auth
*   Boost >= 1.45
*   Systemd


Execution
---------
Systemd-kcm can be accessed through System Settings, or by issuing the command:    
`kcmshell5 kcm_systemd`


Developed by: Ragnar Thomsen (rthomsen6@gmail.com)  

Website:
https://projects.kde.org/projects/playground/sysadmin/systemd-kcm/

Please report bugs and feature requests at:
https://bugs.kde.org/

Git repository can be found at:
http://quickgit.kde.org/?p=systemd-kcm.git
