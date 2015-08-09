/*******************************************************************************
 * Copyright (C) 2013-2015 Ragnar Thomsen <rthomsen6@gmail.com>                *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation, either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program. If not, see <http://www.gnu.org/licenses/>.              *
 *******************************************************************************/

#include <QThread>

#include <KLocalizedString>

#include "confparms.h"
#include "fsutil.h"

QList<confOption> getConfigParms(const int systemdVersion)
{
  // Creates an instance of confOption for each option in the systemd
  // configuration files and adds them to list which gets returned.
  // Arguments are passed as a QVariantMap.

  qulonglong partPersSizeMB = getPartitionSize(QStringLiteral("/var/log"), NULL) / 1024 / 1024;
  qulonglong partVolaSizeMB = getPartitionSize(QStringLiteral("/run/log"), NULL) / 1024 / 1024;

  QList<confOption> list;
  QVariantMap map;

  // system.conf

  map.clear();
  map["name"] = "LogLevel";
  map["file"] = SYSTEMD;
  map["type"] = LIST;
  map["defVal"] = "info";
  map["possibleVals"] = QStringList() << "emerg" << "alert" << "crit" << "err" << "warning" << "notice" << "info" << "debug";
  map["toolTip"] = i18n("<p>Set the level of log entries in systemd.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "LogTarget";
  map["file"] = SYSTEMD;
  map["type"] = LIST;
  map["defVal"] = "journal-or-kmsg";
  map["possibleVals"] = QStringList() <<  "console" << "journal" << "syslog" << "kmsg" << "journal-or-kmsg" << "syslog-or-kmsg" << "null";
  map["toolTip"] = i18n("<p>Set target for logs.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "LogColor";
  map["file"] = SYSTEMD;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Use color to highlight important log messages.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "LogLocation";
  map["file"] = SYSTEMD;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Include code location in log messages.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "DumpCore";
  map["file"] = SYSTEMD;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Dump core on systemd crash.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "CrashShell";
  map["file"] = SYSTEMD;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Spawn a shell when systemd crashes. Note: The shell is not password-protected.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "ShowStatus";
  map["file"] = SYSTEMD;
  map["type"] = LIST;
  map["defVal"] = "yes";
  map["possibleVals"] = QStringList() << "yes" << "no" << "auto";
  map["toolTip"] = i18n("<p>Show terse service status information while booting.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "CrashChVT";
  map["file"] = SYSTEMD;
  map["type"] = LIST;
  map["defVal"] = "-1";
  map["possibleVals"] = QStringList() << "-1" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8";
  map["toolTip"] = i18n("<p>Activate the specified virtual terminal when systemd crashes (-1 to deactivate).</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "CPUAffinity";
  map["file"] = SYSTEMD;
  map["type"] = MULTILIST;
  QStringList CPUAffinityList;
  for (int i = 1; i <= QThread::idealThreadCount(); ++i)
    CPUAffinityList << QString::number(i);
  map["possibleVals"] = CPUAffinityList;
  map["toolTip"] = i18n("<p>The initial CPU affinity for the systemd init process.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "JoinControllers";
  map["file"] = SYSTEMD;
  map["type"] = STRING;
  map["defVal"] = "cpu,cpuacct net_cls,net_prio";
  map["toolTip"] = i18n("<p>Controllers that shall be mounted in a single hierarchy. Takes a space-separated list of comma-separated controller names, in order to allow multiple joined hierarchies. Pass an empty string to ensure that systemd mounts all controllers in separate hierarchies.</p>");
  list.append(confOption(map));

  if (systemdVersion < 208)
  {
    map.clear();
    map["name"] = "DefaultControllers";
    map["file"] = SYSTEMD;
    map["type"] = STRING;
    map["defVal"] = "cpu";
    map["toolTip"] = i18n("<p>Configures in which control group hierarchies to create per-service cgroups automatically, in addition to the name=systemd named hierarchy. Takes a space-separated list of controller names. Pass the empty string to ensure that systemd does not touch any hierarchies but its own.</p>");
    list.append(confOption(map));
  }

  map.clear();
  map["name"] = "RuntimeWatchdogSec";
  map["file"] = SYSTEMD;
  map["type"] = TIME;
  map["defVal"] = 0;
  map["toolTip"] = i18n("<p>The watchdog hardware (/dev/watchdog) will be programmed to automatically reboot the system if it is not contacted within the specified timeout interval. The system manager will ensure to contact it at least once in half the specified timeout interval. This feature requires a hardware watchdog device.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "ShutdownWatchdogSec";
  map["file"] = SYSTEMD;
  map["type"] = TIME;
  map["defVal"] = 10;
  map["defUnit"] = confOption::min;
  map["toolTip"] = i18n("<p>This setting may be used to configure the hardware watchdog when the system is asked to reboot. It works as a safety net to ensure that the reboot takes place even if a clean reboot attempt times out. This feature requires a hardware watchdog device.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "CapabilityBoundingSet";
  map["file"] = SYSTEMD;
  map["type"] = MULTILIST;
  map["possibleVals"] = confOption::capabilities;
  map["toolTip"] = i18n("<p>Capabilities for the systemd daemon and its children.</p>");
  list.append(confOption(map));

  if (systemdVersion >= 209)
  {
    map.clear();
    map["name"] = "SystemCallArchitectures";
    map["file"] = SYSTEMD;
    map["type"] = MULTILIST;
    map["possibleVals"] = QStringList() << "native" << "x86" << "x86-64" << "x32" << "arm";
    map["toolTip"] = i18n("<p>Architectures of which system calls may be invoked on the system.</p>");
    list.append(confOption(map));
  }

  map.clear();
  map["name"] = "TimerSlackNSec";
  map["file"] = SYSTEMD;
  map["type"] = TIME;
  map["defVal"] = 0;
  map["defUnit"] = confOption::ns;
  map["defReadUnit"] = confOption::ns;
  map["hasNsec"] = true;
  map["toolTip"] = i18n("<p>Sets the timer slack for PID 1 which is then inherited to all executed processes, unless overridden individually. The timer slack controls the accuracy of wake-ups triggered by timers.</p>");
  list.append(confOption(map));

  if (systemdVersion >= 212)
  {
    map.clear();
    map["name"] = "DefaultTimerAccuracySec";
    map["file"] = SYSTEMD;
    map["type"] = TIME;
    map["defVal"] = 60;
    map["toolTip"] = i18n("<p>The default accuracy of timer units. Note that the accuracy of timer units is also affected by the configured timer slack for PID 1.</p>");
    list.append(confOption(map));
  }

  map.clear();
  map["name"] = "DefaultStandardOutput";
  map["file"] = SYSTEMD;
  map["type"] = LIST;
  map["defVal"] = "journal";
  map["possibleVals"] = QStringList() << "inherit" << "null" << "tty" << "journal"
                                      << "journal+console" << "syslog" << "syslog+console"
                                      << "kmsg" << "kmsg+console";
  map["toolTip"] = i18n("<p>Sets the default output for all services and sockets.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "DefaultStandardError";
  map["file"] = SYSTEMD;
  map["type"] = LIST;
  map["defVal"] = "inherit";
  map["possibleVals"] = QStringList() << "inherit" << "null" << "tty" << "journal"
                                      << "journal+console" << "syslog" << "syslog+console"
                                      << "kmsg" << "kmsg+console";
  map["toolTip"] = i18n("<p>Sets the default error output for all services and sockets.</p>");
  list.append(confOption(map));

  if (systemdVersion >= 209)
  {
    map.clear();
    map["name"] = "DefaultTimeoutStartSec";
    map["file"] = SYSTEMD;
    map["type"] = TIME;
    map["defVal"] = 90;
    map["toolTip"] = i18n("<p>The default timeout for starting of units.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "DefaultTimeoutStopSec";
    map["file"] = SYSTEMD;
    map["type"] = TIME;
    map["defVal"] = 90;
    map["toolTip"] = i18n("<p>The default timeout for stopping of units.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "DefaultRestartSec";
    map["file"] = SYSTEMD;
    map["type"] = TIME;
    map["defVal"] = 100;
    map["defUnit"] = confOption::ms;
    map["toolTip"] = i18n("<p>The default time to sleep between automatic restart of units.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "DefaultStartLimitInterval";
    map["file"] = SYSTEMD;
    map["type"] = TIME;
    map["defVal"] = 10;
    map["toolTip"] = i18n("<p>Time interval used in start rate limit for services.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "DefaultStartLimitBurst";
    map["file"] = SYSTEMD;
    map["type"] = INTEGER;
    map["defVal"] = 5;
    map["toolTip"] = i18n("<p>Services are not allowed to start more than this number of times within the time interval defined in DefaultStartLimitInterval.</p>");
    list.append(confOption(map));
  }

  if (systemdVersion >= 205)
  {
    map.clear();
    map["name"] = "DefaultEnvironment";
    map["file"] = SYSTEMD;
    map["type"] = STRING;
    map["defVal"] = QString("");
    map["toolTip"] = i18n("<p>Manager environment variables passed to all executed processes. Takes a space-separated list of variable assignments.</p>");
    list.append(confOption(map));
  }

  if (systemdVersion >= 211)
  {
    map.clear();
    map["name"] = "DefaultCPUAccounting";
    map["file"] = SYSTEMD;
    map["type"] = BOOL;
    map["defVal"] = false;
    map["toolTip"] = i18n("<p>Default CPU usage accounting.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "DefaultBlockIOAccounting";
    map["file"] = SYSTEMD;
    map["type"] = BOOL;
    map["defVal"] = false;
    map["toolTip"] = i18n("<p>Default block IO accounting.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "DefaultMemoryAccounting";
    map["file"] = SYSTEMD;
    map["type"] = BOOL;
    map["defVal"] = false;
    map["toolTip"] = i18n("<p>Default process and kernel memory accounting.</p>");
    list.append(confOption(map));
  }

  // Loop through all RESLIMITs
  QStringList resLimits = QStringList() << "DefaultLimitCPU" << "DefaultLimitFSIZE" << "DefaultLimitDATA"
                                        << "DefaultLimitSTACK" << "DefaultLimitCORE" << "DefaultLimitRSS"
                                        << "DefaultLimitNOFILE" << "DefaultLimitAS" << "DefaultLimitNPROC"
                                        << "DefaultLimitMEMLOCK" << "DefaultLimitLOCKS" << "DefaultLimitSIGPENDING"
                                        << "DefaultLimitMSGQUEUE" << "DefaultLimitNICE" << "DefaultLimitRTPRIO"
                                        << "DefaultLimitRTTIME";
  foreach (const QString &s, resLimits)
  {
    map.clear();
    map["name"] = s;
    map["file"] = SYSTEMD;
    map["type"] = RESLIMIT;
    map["minVal"] = -1;
    map["toolTip"] = i18n("<p>Default resource limit for units. Set to -1 for no limit.</p>");
    list.append(confOption(map));
  }

  // journald.conf

  map.clear();
  map["name"] = "Storage";
  map["file"] = JOURNALD;
  map["type"] = LIST;
  map["defVal"] = "auto";
  map["possibleVals"] = QStringList() << "volatile" << "persistent" << "auto" << "none";
  map["toolTip"] = i18n("<p>Where to store log files.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "Compress";
  map["file"] = JOURNALD;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Compress log files.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "Seal";
  map["file"] = JOURNALD;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Enable Forward Secure Sealing (FSS) for all persistent journal files.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "SplitMode";
  map["file"] = JOURNALD;
  map["type"] = LIST;
  if (systemdVersion >= 215)
    map["defVal"] = "uid";
  else
    map["defVal"] = "login";
  map["possibleVals"] = QStringList() << "login" << "uid" << "none";
  map["toolTip"] = i18n("<p>Whether and how to split log files. If <i>uid</i>, all users will get each their own journal files regardless of whether they possess a login session or not, however system users will log into the system journal. If <i>login</i>, actually logged-in users will get each their own journal files, but users without login session and system users will log into the system journal. If <i>none</i>, journal files are not split up by user and all messages are instead stored in the single system journal.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "SyncIntervalSec";
  map["file"] = JOURNALD;
  map["type"] = TIME;
  map["defVal"] = 5;
  map["defUnit"] = confOption::min;
  map["toolTip"] = i18n("<p>The timeout before synchronizing journal files to disk.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "RateLimitInterval";
  map["file"] = JOURNALD;
  map["type"] = TIME;
  map["defVal"] = 30;
  map["toolTip"] = i18n("<p>Time interval for rate limiting of log messages. Set to 0 to turn off rate-limiting.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "RateLimitBurst";
  map["file"] = JOURNALD;
  map["type"] = INTEGER;
  map["defVal"] = 1000;
  map["toolTip"] = i18n("<p>Maximum number of messages logged for a unit in the interval specified in RateLimitInterval. Set to 0 to turn off rate-limiting.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "SystemMaxUse";
  map["file"] = JOURNALD;
  map["type"] = SIZE;
  map["defVal"] = QVariant(0.1 * partPersSizeMB).toULongLong();
  map["maxVal"] = partPersSizeMB;
  // xgettext:no-c-format
  map["toolTip"] = i18n("<p>Maximum disk space the persistent journal may take up. Defaults to 10% of file system size.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "SystemKeepFree";
  map["file"] = JOURNALD;
  map["type"] = SIZE;
  map["defVal"] = QVariant(0.15 * partPersSizeMB).toULongLong();
  map["maxVal"] = partPersSizeMB;
  // xgettext:no-c-format
  map["toolTip"] = i18n("<p>Minimum disk space the persistent journal should keep free for other uses. Defaults to 15% of file system size.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "SystemMaxFileSize";
  map["file"] = JOURNALD;
  map["type"] = SIZE;
  map["defVal"] = QVariant(0.125 * 0.1 * partPersSizeMB).toULongLong();
  map["maxVal"] = partPersSizeMB;
  map["toolTip"] = i18n("<p>Maximum size of individual journal files on persistent storage. Defaults to 1/8 of SystemMaxUse.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "RuntimeMaxUse";
  map["file"] = JOURNALD;
  map["type"] = SIZE;
  map["defVal"] = QVariant(0.1 * partVolaSizeMB).toULongLong();
  map["maxVal"] = partVolaSizeMB;
  // xgettext:no-c-format
  map["toolTip"] = i18n("<p>Maximum disk space the volatile journal may take up. Defaults to 10% of file system size.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "RuntimeKeepFree";
  map["file"] = JOURNALD;
  map["type"] = SIZE;
  map["defVal"] = QVariant(0.15 * partVolaSizeMB).toULongLong();
  map["maxVal"] = partVolaSizeMB;
  // xgettext:no-c-format
  map["toolTip"] = i18n("<p>Minimum disk space the volatile journal should keep free for other uses. Defaults to 15% of file system size.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "RuntimeMaxFileSize";
  map["file"] = JOURNALD;
  map["type"] = SIZE;
  map["defVal"] = QVariant(0.125 * 0.1 * partVolaSizeMB).toULongLong();
  map["maxVal"] = partVolaSizeMB;
  map["toolTip"] = i18n("<p>Maximum size of individual journal files on volatile storage. Defaults to 1/8 of RuntimeMaxUse.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "MaxRetentionSec";
  map["file"] = JOURNALD;
  map["type"] = TIME;
  map["defVal"] = 0;
  map["defUnit"] = confOption::d;
  map["toolTip"] = i18n("<p>Maximum time to store journal entries. Set to 0 to disable.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "MaxFileSec";
  map["file"] = JOURNALD;
  map["type"] = TIME;
  map["defVal"] = 30;
  map["defUnit"] = confOption::d;
  map["toolTip"] = i18n("<p>Maximum time to store entries in a single journal file before rotating to the next one. Set to 0 to disable.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "ForwardToSyslog";
  map["file"] = JOURNALD;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Forward journal messages to syslog.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "ForwardToKMsg";
  map["file"] = JOURNALD;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Forward journal messages to kernel log buffer</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "ForwardToConsole";
  map["file"] = JOURNALD;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Forward journal messages to the system console. The console can be changed with TTYPath.</p>");
  list.append(confOption(map));

  if (systemdVersion >= 212)
  {
    map.clear();
    map["name"] = "ForwardToWall";
    map["file"] = JOURNALD;
    map["type"] = BOOL;
    map["defVal"] = true;
    map["toolTip"] = i18n("<p>Forward journal messages as wall messages to all logged-in users.</p>");
    list.append(confOption(map));
  }

  map.clear();
  map["name"] = "TTYPath";
  map["file"] = JOURNALD;
  map["type"] = STRING;
  map["defVal"] = "/dev/console";
  map["toolTip"] = i18n("<p>Forward journal messages to this TTY if ForwardToConsole is set.</p>");
  list.append(confOption(map));

  QStringList listLogLevel = QStringList() << "MaxLevelStore" << "MaxLevelSyslog"
                                           << "MaxLevelKMsg" << "MaxLevelConsole";
  if (systemdVersion >= 212)
    listLogLevel << "MaxLevelWall";

  foreach (const QString &s, listLogLevel)
  {
    map.clear();
    map["name"] = s;
    map["file"] = JOURNALD;
    map["type"] = LIST;
    if (s == "MaxLevelKMsg")
      map["defVal"] = "notice";
    else if (s == "MaxLevelConsole")
      map["defVal"] = "info";
    else if (s == "MaxLevelWall")
      map["defVal"] = "emerg";
    else
      map["defVal"] = "debug";
    map["possibleVals"] = QStringList() << "emerg" << "alert" << "crit" << "err"
                                        << "warning" << "notice" << "info" << "debug";
    map["toolTip"] = i18n("<p>Max log level to forward/store to the specified target.</p>");
    list.append(confOption(map));
  }
  
  // logind.conf

  map.clear();
  map["name"] = "NAutoVTs";
  map["file"] = LOGIND;
  map["type"] = INTEGER;
  map["defVal"] = 6;
  map["maxVal"] = 12;
  map["toolTip"] = i18n("<p>Number of virtual terminals (VTs) to allocate by default and which will autospawn. Set to 0 to disable.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "ReserveVT";
  map["file"] = LOGIND;
  map["type"] = INTEGER;
  map["defVal"] = 6;
  map["maxVal"] = 12;
  map["toolTip"] = i18n("<p>Virtual terminal that shall unconditionally be reserved for autospawning. Set to 0 to disable.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "KillUserProcesses";
  map["file"] = LOGIND;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Kill the processes of a user when the user completely logs out.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "KillOnlyUsers";
  map["file"] = LOGIND;
  map["type"] = STRING;
  map["toolTip"] = i18n("<p>Space-separated list of usernames for which only processes will be killed if KillUserProcesses is enabled.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "KillExcludeUsers";
  map["file"] = LOGIND;
  map["type"] = STRING;
  map["defVal"] = "root";
  map["toolTip"] = i18n("<p>Space-separated list of usernames for which processes will be excluded from being killed if KillUserProcesses is enabled.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "InhibitDelayMaxSec";
  map["file"] = LOGIND;
  map["type"] = TIME;
  map["defVal"] = 5;
  map["toolTip"] = i18n("<p>Specifies the maximum time a system shutdown or sleep request is delayed due to an inhibitor lock of type delay being active before the inhibitor is ignored and the operation executes anyway.</p>");
  list.append(confOption(map));

  QStringList listPowerActions = QStringList() << "ignore" << "poweroff" << "reboot" << "halt" << "kexec"
                                               << "suspend" << "hibernate" << "hybrid-sleep" << "lock";
  QStringList listPower = QStringList() << "HandlePowerKey" << "HandleSuspendKey"
                                           << "HandleHibernateKey" << "HandleLidSwitch";
  if (systemdVersion >= 217)
    listPower << "HandleLidSwitchDocked";
  foreach (const QString &s, listPower)
  {
    map.clear();
    map["name"] = s;
    map["file"] = LOGIND;
    map["type"] = LIST;
    if (s == "HandlePowerKey")
      map["defVal"] = "poweroff";
    else if (s == "HandleHibernateKey")
      map["defVal"] = "hibernate";
    else if (s == "HandleLidSwitchDocked")
      map["defVal"] = "ignore";
    else
      map["defVal"] = "suspend";
    map["possibleVals"] = listPowerActions;
    map["toolTip"] = i18n("<p>Controls whether logind shall handle the system power and sleep keys and the lid switch to trigger power actions.</p>");
    list.append(confOption(map));
  }

  map.clear();
  map["name"] = "PowerKeyIgnoreInhibited";
  map["file"] = LOGIND;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Controls whether actions triggered by the power key are subject to inhibitor locks.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "SuspendKeyIgnoreInhibited";
  map["file"] = LOGIND;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Controls whether actions triggered by the suspend key are subject to inhibitor locks.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "HibernateKeyIgnoreInhibited";
  map["file"] = LOGIND;
  map["type"] = BOOL;
  map["defVal"] = false;
  map["toolTip"] = i18n("<p>Controls whether actions triggered by the hibernate key are subject to inhibitor locks.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "LidSwitchIgnoreInhibited";
  map["file"] = LOGIND;
  map["type"] = BOOL;
  map["defVal"] = true;
  map["toolTip"] = i18n("<p>Controls whether actions triggered by the lid switch are subject to inhibitor locks.</p>");
  list.append(confOption(map));

  if (systemdVersion >= 220)
  {
    map.clear();
    map["name"] = "HoldoffTimeoutSec";
    map["file"] = LOGIND;
    map["type"] = TIME;
    map["defVal"] = 30;
    map["toolTip"] = i18n("<p>Timeout after system startup or system resume in which systemd will hold off on reacting to lid events.</p>");
    list.append(confOption(map));
  }

  map.clear();
  map["name"] = "IdleAction";
  map["file"] = LOGIND;
  map["type"] = LIST;
  map["defVal"] = "ignore";
  map["possibleVals"] = listPowerActions;
  map["toolTip"] = i18n("<p>Configures the action to take when the system is idle.</p>");
  list.append(confOption(map));

  map.clear();
  map["name"] = "IdleActionSec";
  map["file"] = LOGIND;
  map["type"] = TIME;
  map["defVal"] = 0;
  map["toolTip"] = i18n("<p>Configures the delay after which the action configured in IdleAction is taken after the system is idle.</p>");
  list.append(confOption(map));

  if (systemdVersion >= 211)
  {
    map.clear();
    map["name"] = "RuntimeDirectorySize";
    map["file"] = LOGIND;
    map["type"] = SIZE;
    map["defVal"] = 10;
    map["maxVal"] = 100;
    map["toolTip"] = i18n("<p>Sets the size limit on the $XDG_RUNTIME_DIR runtime directory for each user who logs in. Percentage of physical RAM.</p>");
    list.append(confOption(map));
  }

  if (systemdVersion >= 212)
  {
    map.clear();
    map["name"] = "RemoveIPC";
    map["file"] = LOGIND;
    map["type"] = BOOL;
    map["defVal"] = true;
    map["toolTip"] = i18n("<p>Controls whether System V and POSIX IPC objects belonging to the user shall be removed when the user fully logs out.</p>");
    list.append(confOption(map));
  }

  // coredump.conf
  if (systemdVersion >= 215)
  {
    map.clear();
    map["name"] = "Storage";
    map["file"] = COREDUMP;
    map["type"] = LIST;
    map["defVal"] = "external";
    map["possibleVals"] = QStringList() << "none" << "external" << "journal" << "both";
    map["toolTip"] = i18n("<p>Controls where to store cores. When <i>none</i>, the coredumps will be logged but not stored permanently. When <i>external</i>, cores will be stored in /var/lib/systemd/coredump. When <i>journal</i>, cores will be stored in the journal and rotated following normal journal rotation patterns.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "Compress";
    map["file"] = COREDUMP;
    map["type"] = BOOL;
    map["defVal"] = true;
    map["toolTip"] = i18n("<p>Controls compression for external storage.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "ProcessSizeMax";
    map["file"] = COREDUMP;
    map["type"] = SIZE;
    map["defVal"] = 2*1024;
    map["maxVal"] = partPersSizeMB;
    map["toolTip"] = i18n("<p>The maximum size of a core which will be processed. Coredumps exceeding this size will be logged, but the backtrace will not be generated and the core will not be stored.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "ExternalSizeMax";
    map["file"] = COREDUMP;
    map["type"] = SIZE;
    map["defVal"] = 2*1024;
    map["maxVal"] = partPersSizeMB;
    map["toolTip"] = i18n("<p>The maximum (uncompressed) size of a core to be saved.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "JournalSizeMax";
    map["file"] = COREDUMP;
    map["type"] = SIZE;
    map["defVal"] = 767;
    map["maxVal"] = partPersSizeMB;
    map["toolTip"] = i18n("<p>The maximum (uncompressed) size of a core to be saved.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "MaxUse";
    map["file"] = COREDUMP;
    map["type"] = SIZE;
    map["defVal"] = QVariant(0.1 * partPersSizeMB).toULongLong();
    map["maxVal"] = partPersSizeMB;
    // xgettext:no-c-format
    map["toolTip"] = i18n("<p>Old coredumps are removed as soon as the total disk space taken up by coredumps grows beyond this limit. Defaults to 10% of the total disk size.</p>");
    list.append(confOption(map));

    map.clear();
    map["name"] = "KeepFree";
    map["file"] = COREDUMP;
    map["type"] = SIZE;
    map["defVal"] = QVariant(0.15 * partPersSizeMB).toULongLong();
    map["maxVal"] = partPersSizeMB;
    // xgettext:no-c-format
    map["toolTip"] = i18n("<p>Minimum disk space to keep free. Defaults to 15% of the total disk size.</p>");
    list.append(confOption(map));
  }

  /* We can dismiss these options now...
  if (systemdVersion < 207)
  {
    list.append(confOption(LOGIND, "Controllers", STRING, ""));
    list.append(confOption(LOGIND, "ResetControllers", STRING, "cpu"));
  }
  */

  return list;
}
