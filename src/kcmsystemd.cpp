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

#include "config.h"
#include "kcmsystemd.h"
#include "confparms.h"
#include "fsutil.h"

#include <unistd.h>

#include <QMouseEvent>
#include <QMenu>
#include <QPlainTextEdit>

#include <KAboutData>
#include <KPluginFactory>
#include <KMessageBox>
#include <KAuth>
#include <KColorScheme>
using namespace KAuth;

K_PLUGIN_FACTORY(kcmsystemdFactory, registerPlugin<kcmsystemd>();)

kcmsystemd::kcmsystemd(QWidget *parent, const QVariantList &args) : KCModule(parent, args)
{
  KAboutData *about = new KAboutData(QStringLiteral("kcmsystemd"),
                                     i18n("systemd-kcm"),
                                     SYSTEMD_KCM_VERSION,
                                     i18n("KDE Systemd Control Module"),
                                     KAboutLicense::GPL_V3,
                                     QStringLiteral("Copyright (C) 2013-2015 Ragnar Thomsen"),
                                     QStringLiteral(),
                                     QStringLiteral("https://projects.kde.org/projects/playground/sysadmin/systemd-kcm/"));
  about->addAuthor(QStringLiteral("Ragnar Thomsen"), i18n("Main Developer"), QStringLiteral("rthomsen6@gmail.com"));
  setAboutData(about);
  ui.setupUi(this);
  setButtons(kcmsystemd::Default | kcmsystemd::Apply);
  setNeedsAuthorization(true);
  ui.leSearchUnit->setFocus();

  // See if systemd is reachable via dbus
  if (getDbusProperty(QStringLiteral("Version"), sysdMgr) != QLatin1String("invalidIface"))
  {
    systemdVersion = getDbusProperty(QStringLiteral("Version"), sysdMgr).toString().remove(QStringLiteral("systemd ")).toInt();
    qDebug() << "Detected systemd" << systemdVersion;
  }
  else
  {
    qDebug() << "Unable to contact systemd daemon!";
    ui.stackedWidget->setCurrentIndex(1);
  }

  // Search for user bus
  if (QFile("/run/user/" + QString::number(getuid()) + "/bus").exists())
    userBusPath = "unix:path=/run/user/" + QString::number(getuid()) + "/bus";
  else if (QFile("/run/user/" + QString::number(getuid()) + "/dbus/user_bus_socket").exists())
    userBusPath = "unix:path=/run/user/" + QString::number(getuid()) + "/dbus/user_bus_socket";
  else
  {
    qDebug() << "User bus not found. Support for user units disabled.";
    ui.tabWidget->setTabEnabled(1,false);
    enableUserUnits = false;
  }
  
  // Find the configuration directory
  if (QDir(QStringLiteral("/etc/systemd")).exists()) {
    etcDir = QStringLiteral("/etc/systemd");
  } else if (QDir(QStringLiteral("/usr/etc/systemd")).exists()) {
    etcDir = QStringLiteral("/usr/etc/systemd");
  } else {
    // Failed to find systemd config directory
    ui.stackedWidget->setCurrentIndex(1);    
    ui.lblFailMessage->setText(i18n("Unable to find directory with systemd configuration files."));
    return;
  }
  listConfFiles  << "system.conf"
                 << "journald.conf"
                 << "logind.conf";
  if (systemdVersion >= 215 &&
      QFile(etcDir + QStringLiteral("/coredump.conf")).exists())
    listConfFiles << QStringLiteral("coredump.conf");
  
  partPersSizeMB = getPartitionSize(QStringLiteral("/var/log"), NULL) / 1024 / 1024;
  partVolaSizeMB = getPartitionSize(QStringLiteral("/run/log"), NULL) / 1024 / 1024;
  qDebug() << "Persistent partition size found to: " << partPersSizeMB << "MB";
  qDebug() << "Volatile partition size found to: " << partVolaSizeMB << "MB";
  
  confOptList.append(getConfigParms(systemdVersion));
  setupSignalSlots();
  
  // Subscribe to dbus signals from systemd system daemon and connect them to slots
  callDbusMethod(QStringLiteral("Subscribe"), sysdMgr);
  systembus.connect(connSystemd, pathSysdMgr, ifaceMgr,
                    QStringLiteral("Reloading"), this, SLOT(slotSystemSystemdReloading(bool)));
  // systembus.connect(connSystemd, pathSysdMgr, ifaceMgr,
  //                QStringLiteral("UnitNew"), this, SLOT(slotUnitLoaded(QString, QDBusObjectPath)));
  // systembus.connect(connSystemd,pathSysdMgr, ifaceMgr,
  //                QStringLiteral("UnitRemoved"), this, SLOT(slotUnitUnloaded(QString, QDBusObjectPath)));
  systembus.connect(connSystemd, pathSysdMgr, ifaceMgr,
                    QStringLiteral("UnitFilesChanged"), this, SLOT(slotSystemUnitsChanged()));
  systembus.connect(connSystemd, "", ifaceDbusProp,
                    QStringLiteral("PropertiesChanged"), this, SLOT(slotSystemUnitsChanged()));
  // We need to use the JobRemoved signal, because stopping units does not emit PropertiesChanged signal
  systembus.connect(connSystemd, pathSysdMgr, ifaceMgr,
                    QStringLiteral("JobRemoved"), this, SLOT(slotSystemUnitsChanged()));

  // Subscribe to dbus signals from systemd user daemon and connect them to slots
  callDbusMethod("Subscribe", sysdMgr, user);
  QDBusConnection userbus = QDBusConnection::connectToBus(userBusPath, connSystemd);
  userbus.connect(connSystemd,pathSysdMgr, ifaceMgr,
                  QStringLiteral("Reloading"), this, SLOT(slotUserSystemdReloading(bool)));
  userbus.connect(connSystemd, pathSysdMgr, ifaceMgr,
                  QStringLiteral("UnitFilesChanged"), this, SLOT(slotUserUnitsChanged()));
  userbus.connect(connSystemd, "", ifaceDbusProp,
                  QStringLiteral("PropertiesChanged"), this, SLOT(slotUserUnitsChanged()));
  userbus.connect(connSystemd, pathSysdMgr, ifaceMgr,
                  QStringLiteral("JobRemoved"), this, SLOT(slotUserUnitsChanged()));

  // logind
  systembus.connect(connLogind, "", ifaceDbusProp,
                    QStringLiteral("PropertiesChanged"), this,
                    SLOT(slotLogindPropertiesChanged(QString,QVariantMap,QStringList)));
  
  // Get list of units
  slotRefreshUnitsList(true, sys);
  slotRefreshUnitsList(true, user);

  setupUnitslist();
  setupConf();
  setupSessionlist();
  setupTimerlist();
}

kcmsystemd::~kcmsystemd()
{
}

QDBusArgument &operator<<(QDBusArgument &argument, const SystemdUnit &unit)
{
  argument.beginStructure();
  argument << unit.id
     << unit.description
     << unit.load_state
     << unit.active_state
     << unit.sub_state
     << unit.following
     << unit.unit_path
     << unit.job_id
     << unit.job_type
     << unit.job_path;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SystemdUnit &unit)
{
     argument.beginStructure();
     argument >> unit.id
        >> unit.description
        >> unit.load_state
        >> unit.active_state
        >> unit.sub_state
        >> unit.following
        >> unit.unit_path
        >> unit.job_id
        >> unit.job_type
        >> unit.job_path;
     argument.endStructure();
     return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const SystemdSession &session)
{
  argument.beginStructure();
  argument << session.session_id
     << session.user_id
     << session.user_name
     << session.seat_id
     << session.session_path;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SystemdSession &session)
{
     argument.beginStructure();
     argument >> session.session_id
        >> session.user_id
        >> session.user_name
        >> session.seat_id
        >> session.session_path;
     argument.endStructure();
     return argument;
}

void kcmsystemd::setupSignalSlots()
{
  // Connect signals for unit tabs
  connect(ui.chkInactiveUnits, SIGNAL(stateChanged(int)), this, SLOT(slotChkShowUnits(int)));
  connect(ui.chkUnloadedUnits, SIGNAL(stateChanged(int)), this, SLOT(slotChkShowUnits(int)));
  connect(ui.chkInactiveUserUnits, SIGNAL(stateChanged(int)), this, SLOT(slotChkShowUnits(int)));
  connect(ui.chkUnloadedUserUnits, SIGNAL(stateChanged(int)), this, SLOT(slotChkShowUnits(int)));
  connect(ui.cmbUnitTypes, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCmbUnitTypes(int)));
  connect(ui.cmbUserUnitTypes, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCmbUnitTypes(int)));
  connect(ui.tblUnits, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotUnitContextMenu(QPoint)));
  connect(ui.tblUserUnits, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotUnitContextMenu(QPoint)));
  connect(ui.leSearchUnit, SIGNAL(textChanged(QString)), this, SLOT(slotLeSearchUnitChanged(QString)));
  connect(ui.leSearchUserUnit, SIGNAL(textChanged(QString)), this, SLOT(slotLeSearchUnitChanged(QString)));

  // Connect signals for sessions tab
  connect(ui.tblSessions, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotSessionContextMenu(QPoint)));

  // Connect signals for conf tab
  connect(ui.cmbConfFile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCmbConfFileChanged(int)));
}

void kcmsystemd::load()
{
  // Only populate comboboxes once
  if (timesLoad == 0)
  {
    QStringList allowUnitTypes = QStringList() << i18n("All") << i18n("Targets") << i18n("Services")
                                               << i18n("Devices") << i18n("Mounts") << i18n("Automounts") << i18n("Swaps")
                                               << i18n("Sockets") << i18n("Paths") << i18n("Timers") << i18n("Snapshots")
                                               << i18n("Slices") << i18n("Scopes");
    ui.cmbUnitTypes->addItems(allowUnitTypes);
    ui.cmbUserUnitTypes->addItems(allowUnitTypes);
    ui.cmbConfFile->addItems(listConfFiles);
  }
  timesLoad = timesLoad + 1;
  
  // Set all confOptions to default
  // This is needed to clear user changes when resetting the KCM
  for (int i = 0; i < confOptList.size(); ++i)
  {
    confOptList[i].setToDefault();
  }

  // Read the four configuration files
  for (int i = 0; i < listConfFiles.size(); ++i)
  {
    readConfFile(i);
  }

  // Connect signals to slots, which need to be after initializeInterface()
  connect(confModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotConfChanged(QModelIndex,QModelIndex)));
}

void kcmsystemd::readConfFile(int fileindex)
{
  QFile file (etcDir + '/' + listConfFiles.at(fileindex));
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream in(&file);
    QString line = in.readLine();

    while(!line.isNull())
    {
      if (!line.startsWith('#') && !line.startsWith('[') && !line.isEmpty())
      {
        // qDebug() << "line: " << line;
        int index = confOptList.indexOf(confOption(QString(line.section('=',0,0).trimmed() + '_' + QString::number(fileindex))));
        if (index >= 0)
        {
          if (confOptList[index].setValueFromFile(line) == -1)
            displayMsgWidget(KMessageWidget::Warning,
                             i18n("\"%1\" is not a valid value for %2. Using default value for this parameter.", line.section('=',1).trimmed(), confOptList.at(index).realName));
        }
      }
      line = in.readLine();
    } // read lines until empty

    qDebug() << QString("Successfully read " + etcDir + '/' + listConfFiles.at(fileindex));
  } // if file open
  else
    displayMsgWidget(KMessageWidget::Warning,
                     i18n("Failed to read %1/%2. Using default values.", etcDir, listConfFiles.at(fileindex)));
}

void kcmsystemd::setupConf()
{
  // Sets up the configfile model and tableview

  confModel = new ConfModel(this, &confOptList);
  proxyModelConf = new QSortFilterProxyModel(this);
  proxyModelConf->setSourceModel(confModel);

  ui.tblConf->setModel(proxyModelConf);
  
  // Use a custom delegate to enable different editor elements in the QTableView
  ConfDelegate *myDelegate;
  myDelegate = new ConfDelegate(this, &confOptList);
  ui.tblConf->setItemDelegate(myDelegate);

  ui.tblConf->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  ui.tblConf->setColumnHidden(2, true);
  ui.tblConf->resizeColumnsToContents();
}

void kcmsystemd::setupUnitslist()
{
  // Sets up the units list initially

  // Register the meta type for storing units
  qDBusRegisterMetaType<SystemdUnit>();

  QMap<filterType, QString> filters;
  filters[activeState] = "";
  filters[unitType] = "";
  filters[unitName] = "";

  // QList<SystemdUnit> *ptrUnits;
  // ptrUnits = &unitslist;

  // Setup the system unit model
  ui.tblUnits->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  systemUnitModel = new UnitModel(this, &unitslist);
  systemUnitFilterModel = new SortFilterUnitModel(this);
  systemUnitFilterModel->setDynamicSortFilter(false);
  systemUnitFilterModel->initFilterMap(filters);
  systemUnitFilterModel->setSourceModel(systemUnitModel);
  ui.tblUnits->setModel(systemUnitFilterModel);
  ui.tblUnits->sortByColumn(3, Qt::AscendingOrder);

  // Setup the user unit model
  ui.tblUserUnits->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  userUnitModel = new UnitModel(this, &userUnitslist, userBusPath);
  userUnitFilterModel = new SortFilterUnitModel(this);
  userUnitFilterModel->setDynamicSortFilter(false);
  userUnitFilterModel->initFilterMap(filters);
  userUnitFilterModel->setSourceModel(userUnitModel);
  ui.tblUserUnits->setModel(userUnitFilterModel);
  ui.tblUserUnits->sortByColumn(3, Qt::AscendingOrder);

  slotChkShowUnits(-1);
}

void kcmsystemd::setupSessionlist()
{
  // Sets up the session list initially

  // Register the meta type for storing units
  qDBusRegisterMetaType<SystemdSession>();

  // Setup model for session list
  sessionModel = new QStandardItemModel(this);

  // Install eventfilter to capture mouse move events
  ui.tblSessions->viewport()->installEventFilter(this);

  // Set header row
  sessionModel->setHorizontalHeaderItem(0, new QStandardItem(i18n("Session ID")));
  sessionModel->setHorizontalHeaderItem(1, new QStandardItem(i18n("Session Object Path"))); // This column is hidden
  sessionModel->setHorizontalHeaderItem(2, new QStandardItem(i18n("State")));
  sessionModel->setHorizontalHeaderItem(3, new QStandardItem(i18n("User ID")));
  sessionModel->setHorizontalHeaderItem(4, new QStandardItem(i18n("User Name")));
  sessionModel->setHorizontalHeaderItem(5, new QStandardItem(i18n("Seat ID")));
  ui.tblSessions->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  // Set model for QTableView (should be called after headers are set)
  ui.tblSessions->setModel(sessionModel);
  ui.tblSessions->setColumnHidden(1, true);

  // Add all the sessions
  slotRefreshSessionList();
}

void kcmsystemd::setupTimerlist()
{
  // Sets up the timer list initially

  // Setup model for timer list
  timerModel = new QStandardItemModel(this);

  // Install eventfilter to capture mouse move events
  // ui.tblTimers->viewport()->installEventFilter(this);

  // Set header row
  timerModel->setHorizontalHeaderItem(0, new QStandardItem(i18n("Timer")));
  timerModel->setHorizontalHeaderItem(1, new QStandardItem(i18n("Next")));
  timerModel->setHorizontalHeaderItem(2, new QStandardItem(i18n("Left")));
  timerModel->setHorizontalHeaderItem(3, new QStandardItem(i18n("Last")));
  timerModel->setHorizontalHeaderItem(4, new QStandardItem(i18n("Passed")));
  timerModel->setHorizontalHeaderItem(5, new QStandardItem(i18n("Activates")));
  ui.tblTimers->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  // Set model for QTableView (should be called after headers are set)
  ui.tblTimers->setModel(timerModel);
  ui.tblTimers->sortByColumn(1, Qt::AscendingOrder);

  // Setup a timer that updates the left and passed columns every 5secs
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(slotUpdateTimers()));
  timer->start(1000);

  slotRefreshTimerList();
}

void kcmsystemd::defaults()
{
  if (KMessageBox::warningYesNo(this, i18n("Load default settings for all files?")) == KMessageBox::Yes)
  { 
    //defaults for system.conf
    for (int i = 0; i < confOptList.size(); ++i)
    {
      confOptList[i].setToDefault();
    }
    emit changed (true);
  }
}

void kcmsystemd::save()
{  
  QString systemConfFileContents;
  systemConfFileContents.append("# " + etcDir + "/system.conf\n# Generated by kcmsystemd control module v" + SYSTEMD_KCM_VERSION + ".\n");
  systemConfFileContents.append("[Manager]\n");
  foreach (const confOption &i, confOptList)
  {
    if (i.file == SYSTEMD)
      systemConfFileContents.append(i.getLineForFile());
  }

  QString journaldConfFileContents;
  journaldConfFileContents.append("# " + etcDir + "/journald.conf\n# Generated by kcmsystemd control module v" + SYSTEMD_KCM_VERSION + ".\n");
  journaldConfFileContents.append("[Journal]\n");
  foreach (const confOption &i, confOptList)
  {
    if (i.file == JOURNALD)
      journaldConfFileContents.append(i.getLineForFile());
  }  
  
  QString logindConfFileContents;
  logindConfFileContents.append("# " + etcDir + "/logind.conf\n# Generated by kcmsystemd control module v" + SYSTEMD_KCM_VERSION + ".\n");
  logindConfFileContents.append("[Login]\n");
  foreach (const confOption &i, confOptList)
  {
    if (i.file == LOGIND)
      logindConfFileContents.append(i.getLineForFile());
  }
  
  QString coredumpConfFileContents;
  coredumpConfFileContents.append("# " + etcDir + "/coredump.conf\n# Generated by kcmsystemd control module v" + SYSTEMD_KCM_VERSION + ".\n");
  coredumpConfFileContents.append("[Coredump]\n");
  foreach (const confOption &i, confOptList)
  {
    if (i.file == COREDUMP)
      coredumpConfFileContents.append(i.getLineForFile());
  }
  
  // Declare a QVariantMap with arguments for the helper
  QVariantMap helperArgs;
  if (QDir(etcDir).exists()) {
    helperArgs["etcDir"] = etcDir;
  } else {
    // Failed to find systemd config directory
    KMessageBox::error(this, i18n("Unable to find directory for configuration files."));
    return;
  }
  
  QVariantMap files;
  files["system.conf"] = systemConfFileContents;
  files["journald.conf"] = journaldConfFileContents;
  files["logind.conf"] = logindConfFileContents;
  if (systemdVersion >= 215)
    files["coredump.conf"] = coredumpConfFileContents;  
  helperArgs["files"] = files;
  
  // Action saveAction("org.kde.kcontrol.kcmsystemd.save");
  Action saveAction = authAction();
  saveAction.setHelperId("org.kde.kcontrol.kcmsystemd");
  saveAction.setArguments(helperArgs);

  ExecuteJob *job = saveAction.execute();

  if (!job->exec())
    displayMsgWidget(KMessageWidget::Error,
                     i18n("Unable to authenticate/execute the action: %1", job->error()));
  else
    displayMsgWidget(KMessageWidget::Positive,
                     i18n("Configuration files successfully written to: %1", helperArgs["etcDir"].toString()));
}

void kcmsystemd::slotConfChanged(const QModelIndex &, const QModelIndex &)
{
  // qDebug() << "dataChanged emitted";
  emit changed(true);
}

void kcmsystemd::slotChkShowUnits(int state)
{
  if (state == -1 ||
      QObject::sender()->objectName() == "chkInactiveUnits" ||
      QObject::sender()->objectName() == "chkUnloadedUnits")
  {
    // System units
    if (ui.chkInactiveUnits->isChecked())
    {
      ui.chkUnloadedUnits->setEnabled(true);
      if (ui.chkUnloadedUnits->isChecked())
        systemUnitFilterModel->addFilterRegExp(activeState, "");
      else
        systemUnitFilterModel->addFilterRegExp(activeState, "active");
    }
    else
    {
      ui.chkUnloadedUnits->setEnabled(false);
      systemUnitFilterModel->addFilterRegExp(activeState, "^(active)");
    }
    systemUnitFilterModel->invalidate();
    ui.tblUnits->sortByColumn(ui.tblUnits->horizontalHeader()->sortIndicatorSection(),
                              ui.tblUnits->horizontalHeader()->sortIndicatorOrder());
  }
  if (state == -1 ||
      QObject::sender()->objectName() == "chkInactiveUserUnits" ||
      QObject::sender()->objectName() == "chkUnloadedUserUnits")
  {
    // User units
    if (ui.chkInactiveUserUnits->isChecked())
    {
      ui.chkUnloadedUserUnits->setEnabled(true);
      if (ui.chkUnloadedUserUnits->isChecked())
        userUnitFilterModel->addFilterRegExp(activeState, "");
      else
        userUnitFilterModel->addFilterRegExp(activeState, "active");
    }
    else
    {
      ui.chkUnloadedUserUnits->setEnabled(false);
      userUnitFilterModel->addFilterRegExp(activeState, "^(active)");
    }
    userUnitFilterModel->invalidate();
    ui.tblUserUnits->sortByColumn(ui.tblUserUnits->horizontalHeader()->sortIndicatorSection(),
                                  ui.tblUserUnits->horizontalHeader()->sortIndicatorOrder());
  }
  updateUnitCount();
}

void kcmsystemd::slotCmbUnitTypes(int index)
{
  // Filter unit list for a selected unit type

  if (QObject::sender()->objectName() == "cmbUnitTypes")
  {
    systemUnitFilterModel->addFilterRegExp(unitType, '(' + unitTypeSufx.at(index) + ")$");
    systemUnitFilterModel->invalidate();
    ui.tblUnits->sortByColumn(ui.tblUnits->horizontalHeader()->sortIndicatorSection(),
                              ui.tblUnits->horizontalHeader()->sortIndicatorOrder());
  }
  else if (QObject::sender()->objectName() == "cmbUserUnitTypes")
  {
    userUnitFilterModel->addFilterRegExp(unitType, '(' + unitTypeSufx.at(index) + ")$");
    userUnitFilterModel->invalidate();
    ui.tblUserUnits->sortByColumn(ui.tblUserUnits->horizontalHeader()->sortIndicatorSection(),
                                  ui.tblUserUnits->horizontalHeader()->sortIndicatorOrder());
  }
  updateUnitCount();
}

void kcmsystemd::slotRefreshUnitsList(bool initial, dbusBus bus)
{
  // Updates the unit lists

  if (bus == sys)
  {
    qDebug() << "Refreshing system units...";

    // get an updated list of system units via dbus
    unitslist.clear();
    unitslist = getUnitsFromDbus(sys);
    noActSystemUnits = 0;
    foreach (const SystemdUnit &unit, unitslist)
    {
      if (unit.active_state == "active")
        noActSystemUnits++;
    }
    if (!initial)
    {
      systemUnitModel->dataChanged(systemUnitModel->index(0, 0), systemUnitModel->index(systemUnitModel->rowCount(), 3));
      systemUnitFilterModel->invalidate();
      updateUnitCount();
      slotRefreshTimerList();
    }
  }

  else if (enableUserUnits && bus == user)
  {
    qDebug() << "Refreshing user units...";

    // get an updated list of user units via dbus
    userUnitslist.clear();
    userUnitslist = getUnitsFromDbus(user);
    noActUserUnits = 0;
    foreach (const SystemdUnit &unit, userUnitslist)
    {
      if (unit.active_state == "active")
        noActUserUnits++;
    }
    if (!initial)
    {
      userUnitModel->dataChanged(userUnitModel->index(0, 0), userUnitModel->index(userUnitModel->rowCount(), 3));
      userUnitFilterModel->invalidate();
      updateUnitCount();
      slotRefreshTimerList();
    }
  }
}

void kcmsystemd::slotRefreshSessionList()
{
  // Updates the session list
  qDebug() << "Refreshing session list...";

  // clear list
  sessionlist.clear();

  // get an updated list of sessions via dbus
  QDBusMessage dbusreply = callDbusMethod("ListSessions", logdMgr);

  // extract the list of sessions from the reply
  const QDBusArgument arg = dbusreply.arguments().at(0).value<QDBusArgument>();
  if (arg.currentType() == QDBusArgument::ArrayType)
  {
    arg.beginArray();
    while (!arg.atEnd())
    {
      SystemdSession session;
      arg >> session;
      sessionlist.append(session);
    }
    arg.endArray();
  }

  // Iterate through the new list and compare to model
  for (int i = 0;  i < sessionlist.size(); ++i)
  {
    // This is needed to get the "State" property

    QList<QStandardItem *> items = sessionModel->findItems(sessionlist.at(i).session_id, Qt::MatchExactly, 0);

    if (items.isEmpty())
    {
      // New session discovered so add it to the model
      QList<QStandardItem *> row;
      row <<
      new QStandardItem(sessionlist.at(i).session_id) <<
      new QStandardItem(sessionlist.at(i).session_path.path()) <<
      new QStandardItem(getDbusProperty("State", logdSession, sessionlist.at(i).session_path).toString()) <<
      new QStandardItem(QString::number(sessionlist.at(i).user_id)) <<
      new QStandardItem(sessionlist.at(i).user_name) <<
      new QStandardItem(sessionlist.at(i).seat_id);
      sessionModel->appendRow(row);
    } else {
      sessionModel->item(items.at(0)->row(), 2)->setData(getDbusProperty("State", logdSession, sessionlist.at(i).session_path).toString(), Qt::DisplayRole);
    }
  }

  // Check to see if any sessions were removed
  if (sessionModel->rowCount() != sessionlist.size())
  {
    QList<QPersistentModelIndex> indexes;
    // Loop through model and compare to retrieved sessionlist
    for (int row = 0; row < sessionModel->rowCount(); ++row)
    {
      SystemdSession session;
      session.session_id = sessionModel->index(row,0).data().toString();
      if (!sessionlist.contains(session))
      {
        // Add removed units to list for deletion
        // qDebug() << "Unit removed: " << systemUnitModel->index(row,3).data().toString();
        indexes << sessionModel->index(row,0);
      }
    }
    // Delete the identified units from model
    foreach (const QPersistentModelIndex &i, indexes)
      sessionModel->removeRow(i.row());
  }

  // Update the text color in model
  QColor newcolor;
  for (int row = 0; row < sessionModel->rowCount(); ++row)
  {
    QBrush newcolor;
    const KColorScheme scheme(QPalette::Normal);
    if (sessionModel->data(sessionModel->index(row,2), Qt::DisplayRole) == "active")
      newcolor = scheme.foreground(KColorScheme::PositiveText);
    else if (sessionModel->data(sessionModel->index(row,2), Qt::DisplayRole) == "closing")
      newcolor = scheme.foreground(KColorScheme::InactiveText);
    else
      newcolor = scheme.foreground(KColorScheme::NormalText);

    for (int col = 0; col < sessionModel->columnCount(); ++col)
      sessionModel->setData(sessionModel->index(row,col), QVariant(newcolor), Qt::ForegroundRole);
  }
}

void kcmsystemd::slotRefreshTimerList()
{
  // Updates the timer list
  // qDebug() << "Refreshing timer list...";

  timerModel->removeRows(0, timerModel->rowCount());

  // Iterate through system unitlist and add timers to the model
  foreach (const SystemdUnit &unit, unitslist)
  {
    if (unit.id.endsWith(QLatin1String(".timer")) &&
        unit.load_state != QLatin1String("unloaded")) {
      timerModel->appendRow(buildTimerListRow(unit, unitslist, sys));
    }
  }

  // Iterate through user unitlist and add timers to the model
  foreach (const SystemdUnit &unit, userUnitslist)
  {
    if (unit.id.endsWith(QLatin1String(".timer")) &&
        unit.load_state != QLatin1String("unloaded")) {
      timerModel->appendRow(buildTimerListRow(unit, userUnitslist, user));
    }
  }

  // Update the left and passed columns
  slotUpdateTimers();

  ui.tblTimers->resizeColumnsToContents();
  ui.tblTimers->sortByColumn(ui.tblTimers->horizontalHeader()->sortIndicatorSection(),
                             ui.tblTimers->horizontalHeader()->sortIndicatorOrder());
}

QList<QStandardItem *> kcmsystemd::buildTimerListRow(const SystemdUnit &unit, const QList<SystemdUnit> &list, dbusBus bus)
{
  // Builds a row for the timers list

  QDBusObjectPath path = unit.unit_path;
  QString unitToActivate = getDbusProperty("Unit", sysdTimer, path, bus).toString();

  QDateTime time;
  QIcon icon;
  if (bus == sys)
    icon = QIcon::fromTheme("applications-system");
  else
    icon = QIcon::fromTheme("user-identity");

  // Add the next elapsation point
  qlonglong nextElapseMonotonicMsec = getDbusProperty("NextElapseUSecMonotonic", sysdTimer, path, bus).toULongLong() / 1000;
  qlonglong nextElapseRealtimeMsec = getDbusProperty("NextElapseUSecRealtime", sysdTimer, path, bus).toULongLong() / 1000;
  qlonglong lastTriggerMSec = getDbusProperty("LastTriggerUSec", sysdTimer, path, bus).toULongLong() / 1000;

  if (nextElapseMonotonicMsec == 0)
  {
    // Timer is calendar-based
    time.setMSecsSinceEpoch(nextElapseRealtimeMsec);
  }
  else
  {
    // Timer is monotonic
    time = QDateTime().currentDateTime();
    time = time.addMSecs(nextElapseMonotonicMsec);

    // Get the monotonic system clock
    struct timespec ts;
    if (clock_gettime( CLOCK_MONOTONIC, &ts ) != 0)
      qDebug() << "Failed to get the monotonic system clock!";

    // Convert the monotonic system clock to microseconds
    qlonglong now_mono_usec = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

    // And subtract it.
    time = time.addMSecs(-now_mono_usec/1000);
  }

  QString next = time.toString("yyyy.MM.dd hh:mm:ss");

  QString last;

  // use unit object to get last time for activated service
  int index = list.indexOf(SystemdUnit(unitToActivate));
  if (index != -1)
  {
    qlonglong inactivateExitTimestampMsec =
        getDbusProperty("InactiveExitTimestamp", sysdUnit, list.at(index).unit_path, bus).toULongLong() / 1000;

    if (inactivateExitTimestampMsec == 0)
    {
      // The unit has not run in this boot
      // Use LastTrigger to see if the timer is persistent
      if (lastTriggerMSec == 0)
        last = "n/a";
      else
      {
        time.setMSecsSinceEpoch(lastTriggerMSec);
        last = time.toString("yyyy.MM.dd hh:mm:ss");
      }
    }
    else
    {
      QDateTime time;
      time.setMSecsSinceEpoch(inactivateExitTimestampMsec);
      last = time.toString("yyyy.MM.dd hh:mm:ss");
    }
  }

  // Set icon for id column
  QStandardItem *id = new QStandardItem(unit.id);
  id->setData(icon, Qt::DecorationRole);

  // Build a row from QStandardItems
  QList<QStandardItem *> row;
  row << id <<
         new QStandardItem(next) <<
         new QStandardItem("") <<
         new QStandardItem(last) <<
         new QStandardItem("") <<
         new QStandardItem(unitToActivate);

  return row;
}

void kcmsystemd::updateUnitCount()
{
  QString systemUnits = i18ncp("First part of 'Total: %1, %2, %3'",
                               "1 unit", "%1 units", QString::number(systemUnitModel->rowCount()));
  QString systemActive = i18ncp("Second part of 'Total: %1, %2, %3'",
                                "1 active", "%1 active", QString::number(noActSystemUnits));
  QString systemDisplayed = i18ncp("Third part of 'Total: %1, %2, %3'",
                                   "1 displayed", "%1 displayed", QString::number(systemUnitFilterModel->rowCount()));
  ui.lblUnitCount->setText(i18nc("%1 is '%1 units' and %2 is '%2 active' and %3 is '%3 displayed'",
                                 "Total: %1, %2, %3", systemUnits, systemActive, systemDisplayed));

  QString userUnits = i18ncp("First part of 'Total: %1, %2, %3'",
                             "1 unit", "%1 units", QString::number(userUnitModel->rowCount()));
  QString userActive = i18ncp("Second part of 'Total: %1, %2, %3'",
                              "1 active", "%1 active", QString::number(noActUserUnits));
  QString userDisplayed = i18ncp("Third part of 'Total: %1, %2, %3'",
                                 "1 displayed", "%1 displayed", QString::number(userUnitFilterModel->rowCount()));
  ui.lblUserUnitCount->setText(i18nc("%1 is '%1 units' and %2 is '%2 active' and %3 is '%3 displayed'",
                                     "Total: %1, %2, %3", userUnits, userActive, userDisplayed));
}

void kcmsystemd::authServiceAction(QString service, QString path, QString interface, QString method, QList<QVariant> args)
{
  // Function to call the helper to authenticate a call to systemd over the system DBus

  // Declare a QVariantMap with arguments for the helper
  QVariantMap helperArgs;
  helperArgs["service"] = service;
  helperArgs["path"] = path;
  helperArgs["interface"] = interface;
  helperArgs["method"] = method;
  helperArgs["argsForCall"] = args;
    
  // Call the helper
  Action serviceAction("org.kde.kcontrol.kcmsystemd.dbusaction");
  serviceAction.setHelperId("org.kde.kcontrol.kcmsystemd");
  serviceAction.setArguments(helperArgs);

  ExecuteJob* job = serviceAction.execute();
  job->exec();

  if (!job->exec())
    displayMsgWidget(KMessageWidget::Error,
                     i18n("Unable to authenticate/execute the action: %1", job->error()));
  else
  {
    qDebug() << "DBus action successful.";
    // KMessageBox::information(this, i18n("DBus action successful."));
  }
}

void kcmsystemd::slotUnitContextMenu(const QPoint &pos)
{
  // Slot for creating the right-click menu in unitlists

  // Setup objects which can be used for both system and user units
  QList<SystemdUnit> *list;
  QTableView *tblView;
  dbusBus bus;
  bool requiresAuth = true;
  if (ui.tabWidget->currentIndex() == 0)
  {
    list = &unitslist;
    tblView = ui.tblUnits;
    bus = sys;
  }
  else if (ui.tabWidget->currentIndex() == 1)
  {
    list = &userUnitslist;
    tblView = ui.tblUserUnits;
    bus = user;
    requiresAuth = false;
  }

  // Find name and object path of unit
  QString unit = tblView->model()->index(tblView->indexAt(pos).row(), 3).data().toString();
  QDBusObjectPath pathUnit = list->at(list->indexOf(SystemdUnit(unit))).unit_path;

  // Create rightclick menu items
  QMenu menu(this);
  QAction *start = menu.addAction(i18n("&Start unit"));
  QAction *stop = menu.addAction(i18n("S&top unit"));
  QAction *restart = menu.addAction(i18n("&Restart unit"));
  QAction *reload = menu.addAction(i18n("Re&load unit"));
  menu.addSeparator();
  QAction *edit = menu.addAction(i18n("&Edit unit file"));
  QAction *isolate = menu.addAction(i18n("&Isolate unit"));
  menu.addSeparator();
  QAction *enable = menu.addAction(i18n("En&able unit"));
  QAction *disable = menu.addAction(i18n("&Disable unit"));
  menu.addSeparator();
  QAction *mask = menu.addAction(i18n("&Mask unit"));
  QAction *unmask = menu.addAction(i18n("&Unmask unit"));
  menu.addSeparator();
  QAction *reloaddaemon = menu.addAction(i18n("Rel&oad all unit files"));
  QAction *reexecdaemon = menu.addAction(i18n("Ree&xecute systemd"));
  
  // Get UnitFileState (have to use Manager object for this)
  QList<QVariant> args;
  args << unit;
  QString UnitFileState = callDbusMethod("GetUnitFileState", sysdMgr, bus, args).arguments().at(0).toString();

  // Check capabilities of unit
  QString LoadState, ActiveState;
  bool CanStart, CanStop, CanReload;
  if (!pathUnit.path().isEmpty() && getDbusProperty("Test", sysdUnit, pathUnit, bus).toString() != "invalidIface")
  {
    // Unit has a Unit DBus object, fetch properties
    isolate->setEnabled(getDbusProperty("CanIsolate", sysdUnit, pathUnit, bus).toBool());
    LoadState = getDbusProperty("LoadState", sysdUnit, pathUnit, bus).toString();
    ActiveState = getDbusProperty("ActiveState", sysdUnit, pathUnit, bus).toString();
    CanStart = getDbusProperty("CanStart", sysdUnit, pathUnit, bus).toBool();
    CanStop = getDbusProperty("CanStop", sysdUnit, pathUnit, bus).toBool();
    CanReload = getDbusProperty("CanReload", sysdUnit, pathUnit, bus).toBool();
  }
  else
  {
    // No Unit DBus object, only enable Start
    isolate->setEnabled(false);
    CanStart = true;
    CanStop = false;
    CanReload = false;
  }

  // Enable/disable menu items
  if (CanStart && ActiveState != "active")
    start->setEnabled(true);
  else
    start->setEnabled(false);

  if (CanStop &&
      ActiveState != "inactive" &&
      ActiveState != "failed")
    stop->setEnabled(true);
  else
    stop->setEnabled(false);

  if (CanStart &&
      ActiveState != "inactive" &&
      ActiveState != "failed" &&
      !LoadState.isEmpty())
    restart->setEnabled(true);
  else
    restart->setEnabled(false);

  if (CanReload &&
      ActiveState != "inactive" &&
      ActiveState != "failed")
    reload->setEnabled(true);
  else
    reload->setEnabled(false);

  if (UnitFileState == "disabled")
    disable->setEnabled(false);
  else if (UnitFileState == "enabled")
    enable->setEnabled(false);
  else
  {
    enable->setEnabled(false);    
    disable->setEnabled(false);
  }

  if (LoadState == "masked")
    mask->setEnabled(false);
  else if (LoadState != "masked")
    unmask->setEnabled(false);
  
  // Check if unit has a unit file, if not disable editing
  QString frpath;
  int index = list->indexOf(SystemdUnit(unit));
  if (index != -1)
    frpath = list->at(index).unit_file;
  if (frpath.isEmpty())
    edit->setEnabled(false);

  QAction *a = menu.exec(tblView->viewport()->mapToGlobal(pos));
   
  if (a == edit)
  {
    editUnitFile(frpath);
    return;
  }

  // Setup method and arguments for DBus call
  QStringList unitsForCall = QStringList() << unit;
  QString method;
  QList<QVariant> argsForCall;

  if (a == start)
  {
    argsForCall << unit << "replace";
    method = "StartUnit";
  }
  else if (a == stop)
  {
    argsForCall << unit << "replace";
    method = "StopUnit";
  }
  else if (a == restart)
  {
    argsForCall << unit << "replace";
    method = "RestartUnit";
  }
  else if (a == reload)
  {
    argsForCall << unit << "replace";
    method = "ReloadUnit";
  }
  else if (a == isolate)
  {
    argsForCall << unit << "isolate";
    method = "StartUnit";
  }
  else if (a == enable)
  {
    argsForCall << QVariant(unitsForCall) << false << true;
    method = "EnableUnitFiles";
  }
  else if (a == disable)
  {
    argsForCall << QVariant(unitsForCall) << false;
    method = "DisableUnitFiles";
  }
  else if (a == mask)
  {
    argsForCall << QVariant(unitsForCall) << false << true;
    method = "MaskUnitFiles";
  }
  else if (a == unmask)
  {
    argsForCall << QVariant(unitsForCall) << false;
    method = "UnmaskUnitFiles";
  }
  else if (a == reloaddaemon)
    method = "Reload";
  else if (a == reexecdaemon)
    method = "Reexecute";

  // Execute the DBus actions
  if (!method.isEmpty() && requiresAuth)
    authServiceAction(connSystemd, pathSysdMgr, ifaceMgr, method, argsForCall);
  else if (!method.isEmpty())
  {
    // user unit
    callDbusMethod(method, sysdMgr, bus, argsForCall);
    if (method == "EnableUnitFiles" || method == "DisableUnitFiles" || method == "MaskUnitFiles" || method == "UnmaskUnitFiles")
      callDbusMethod("Reload", sysdMgr, bus);
  }
}

void kcmsystemd::slotSessionContextMenu(const QPoint &pos)
{
  // Slot for creating the right-click menu in the session list

  // Setup DBus interfaces
  QDBusObjectPath pathSession = QDBusObjectPath(ui.tblSessions->model()->index(ui.tblSessions->indexAt(pos).row(),1).data().toString());

  // Create rightclick menu items
  QMenu menu(this);
  QAction *activate = menu.addAction(i18n("&Activate session"));
  QAction *terminate = menu.addAction(i18n("&Terminate session"));
  QAction *lock = menu.addAction(i18n("&Lock session"));

  if (ui.tblSessions->model()->index(ui.tblSessions->indexAt(pos).row(),2).data().toString() == "active")
    activate->setEnabled(false);

  if (getDbusProperty("Type", logdSession, pathSession) == "tty")
    lock->setEnabled(false);

  QAction *a = menu.exec(ui.tblSessions->viewport()->mapToGlobal(pos));

  QString method;
  if (a == activate)
  {
    method = "Activate";
    QList<QVariant> argsForCall;
    authServiceAction(connLogind, pathSession.path(), ifaceSession, method, argsForCall);
  }
  if (a == terminate)
  {
    method = "Terminate";
    QList<QVariant> argsForCall;
    authServiceAction(connLogind, pathSession.path(), ifaceSession, method, argsForCall);
  }
  if (a == lock)
  {
    method = "Lock";
    QList<QVariant> argsForCall;
    authServiceAction(connLogind, pathSession.path(), ifaceSession, method, argsForCall);
  }
}


bool kcmsystemd::eventFilter(QObject *obj, QEvent* event)
{
  // Eventfilter for catching mouse move events over session list
  // Used for dynamically generating tooltips

  if (event->type() == QEvent::MouseMove && obj->parent()->objectName() == "tblSessions")
  {
    // Session list
    QMouseEvent *me = static_cast<QMouseEvent*>(event);
    QModelIndex inSessionModel = ui.tblSessions->indexAt(me->pos());
    if (!inSessionModel.isValid())
      return false;

    if (sessionModel->itemFromIndex(inSessionModel)->row() != lastSessionRowChecked)
    {
      // Cursor moved to a different row. Only build tooltips when moving
      // cursor to a new row to avoid excessive DBus calls.

      QString selSession = ui.tblSessions->model()->index(ui.tblSessions->indexAt(me->pos()).row(),0).data().toString();
      QDBusObjectPath spath = QDBusObjectPath(ui.tblSessions->model()->index(ui.tblSessions->indexAt(me->pos()).row(),1).data().toString());

      QString toolTipText;
      toolTipText.append("<FONT COLOR=white>");
      toolTipText.append("<b>" + selSession + "</b><hr>");

      // Use the DBus interface to get session properties
      if (getDbusProperty("test", logdSession, spath) != "invalidIface")
      {
        // Session has a valid session DBus object
        toolTipText.append(i18n("<b>VT:</b> %1", getDbusProperty("VTNr", logdSession, spath).toString()));

        QString remoteHost = getDbusProperty("RemoteHost", logdSession, spath).toString();
        if (getDbusProperty("Remote", logdSession, spath).toBool())
        {
          toolTipText.append(i18n("<br><b>Remote host:</b> %1", remoteHost));
          toolTipText.append(i18n("<br><b>Remote user:</b> %1", getDbusProperty("RemoteUser", logdSession, spath).toString()));
        }
        toolTipText.append(i18n("<br><b>Service:</b> %1", getDbusProperty("Service", logdSession, spath).toString()));

        QString type = getDbusProperty("Type", logdSession, spath).toString();
        toolTipText.append(i18n("<br><b>Type:</b> %1", type));
        if (type == "x11")
          toolTipText.append(i18n(" (display %1)", getDbusProperty("Display", logdSession, spath).toString()));
        else if (type == "tty")
        {
          QString path, tty = getDbusProperty("TTY", logdSession, spath).toString();
          if (!tty.isEmpty())
            path = tty;
          else if (!remoteHost.isEmpty())
            path = getDbusProperty("Name", logdSession, spath).toString() + '@' + remoteHost;
          toolTipText.append(" (" + path + ')');
        }
        toolTipText.append(i18n("<br><b>Class:</b> %1", getDbusProperty("Class", logdSession, spath).toString()));
        toolTipText.append(i18n("<br><b>State:</b> %1", getDbusProperty("State", logdSession, spath).toString()));
        toolTipText.append(i18n("<br><b>Scope:</b> %1", getDbusProperty("Scope", logdSession, spath).toString()));


        toolTipText.append(i18n("<br><b>Created: </b>"));
        if (getDbusProperty("Timestamp", logdSession, spath).toULongLong() == 0)
          toolTipText.append("n/a");
        else
        {
          QDateTime time;
          time.setMSecsSinceEpoch(getDbusProperty("Timestamp", logdSession, spath).toULongLong()/1000);
          toolTipText.append(time.toString());
        }
      }

      toolTipText.append("</FONT");
      sessionModel->itemFromIndex(inSessionModel)->setToolTip(toolTipText);

      lastSessionRowChecked = sessionModel->itemFromIndex(inSessionModel)->row();
      return true;

    } // Row was different
  }
  return false;
  // return true;
}

void kcmsystemd::slotSystemSystemdReloading(bool status)
{
  if (status)
    qDebug() << "System systemd reloading...";
  else
    slotRefreshUnitsList(false, sys);
}

void kcmsystemd::slotUserSystemdReloading(bool status)
{
  if (status)
    qDebug() << "User systemd reloading...";
  else
    slotRefreshUnitsList(false, user);
}

/*
void kcmsystemd::slotUnitLoaded(QString id, QDBusObjectPath path)
{
  qDebug() << "Unit loaded: " << id << " (" << path.path() << ")";
}

void kcmsystemd::slotUnitUnloaded(QString id, QDBusObjectPath path)
{
  qDebug() << "Unit unloaded: " << id << " (" << path.path() << ")";
}
*/

void kcmsystemd::slotSystemUnitsChanged()
{
  // qDebug() << "System units changed";
  slotRefreshUnitsList(false, sys);
}

void kcmsystemd::slotUserUnitsChanged()
{
  // qDebug() << "User units changed";
  slotRefreshUnitsList(false, user);
}

void kcmsystemd::slotLogindPropertiesChanged(QString, QVariantMap, QStringList)
{
  // qDebug() << "Logind properties changed on iface " << iface_name;
  slotRefreshSessionList();
}

void kcmsystemd::slotLeSearchUnitChanged(QString term)
{
  if (QObject::sender()->objectName() == "leSearchUnit")
  {
    systemUnitFilterModel->addFilterRegExp(unitName, term);
    systemUnitFilterModel->invalidate();
    ui.tblUnits->sortByColumn(ui.tblUnits->horizontalHeader()->sortIndicatorSection(),
                              ui.tblUnits->horizontalHeader()->sortIndicatorOrder());
  }
  else if (QObject::sender()->objectName() == "leSearchUserUnit")
  {
    userUnitFilterModel->addFilterRegExp(unitName, term);
    userUnitFilterModel->invalidate();
    ui.tblUserUnits->sortByColumn(ui.tblUserUnits->horizontalHeader()->sortIndicatorSection(),
                                  ui.tblUserUnits->horizontalHeader()->sortIndicatorOrder());
  }
  updateUnitCount();
}

void kcmsystemd::slotCmbConfFileChanged(int index)
{
  ui.lblConfFile->setText(i18n("File to be written: %1/%2", etcDir, listConfFiles.at(index)));

  proxyModelConf->setFilterRegExp(ui.cmbConfFile->itemText(index));
  proxyModelConf->setFilterKeyColumn(2);
}

void kcmsystemd::slotUpdateTimers()
{
  // Updates the left and passed columns in the timers list
  for (int row = 0; row < timerModel->rowCount(); ++row)
  {
    QDateTime next = timerModel->index(row, 1).data().toDateTime();
    QDateTime last = timerModel->index(row, 3).data().toDateTime();
    QDateTime current = QDateTime().currentDateTime();
    qlonglong leftSecs = current.secsTo(next);
    qlonglong passedSecs = last.secsTo(current);

    QString left;
    if (leftSecs >= 31536000)
      left = QString::number(leftSecs / 31536000) + " years";
    else if (leftSecs >= 604800)
      left = QString::number(leftSecs / 604800) + " weeks";
    else if (leftSecs >= 86400)
      left = QString::number(leftSecs / 86400) + " days";
    else if (leftSecs >= 3600)
      left = QString::number(leftSecs / 3600) + " hr";
    else if (leftSecs >= 60)
      left = QString::number(leftSecs / 60) + " min";
    else if (leftSecs < 0)
      left = "0 s";
    else
      left = QString::number(leftSecs) + " s";
    timerModel->setData(timerModel->index(row, 2), left);

    QString passed;
    if (timerModel->index(row, 3).data() == "n/a")
      passed = "n/a";
    else if (passedSecs >= 31536000)
      passed = QString::number(passedSecs / 31536000) + " years";
    else if (passedSecs >= 604800)
      passed = QString::number(passedSecs / 604800) + " weeks";
    else if (passedSecs >= 86400)
      passed = QString::number(passedSecs / 86400) + " days";
    else if (passedSecs >= 3600)
      passed = QString::number(passedSecs / 3600) + " hr";
    else if (passedSecs >= 60)
      passed = QString::number(passedSecs / 60) + " min";
    else if (passedSecs < 0)
      passed = "0 s";
    else
      passed = QString::number(passedSecs) + " s";
    timerModel->setData(timerModel->index(row, 4), passed);
  }
}

void kcmsystemd::editUnitFile(const QString &filename)
{
  // Using a QPointer is safer for modal dialogs.
  // See: https://blogs.kde.org/node/3919
  QPointer<QDialog> dlgEditor = new QDialog(this);
  dlgEditor->setWindowTitle(i18n("Editing %1", filename.section('/', -1)));

  QPlainTextEdit *textEdit = new QPlainTextEdit(dlgEditor);
  textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save |
                                                     QDialogButtonBox::Cancel,
                                                     dlgEditor);
  connect(buttonBox, SIGNAL(accepted()), dlgEditor, SLOT(accept()));
  connect(buttonBox, SIGNAL(rejected()), dlgEditor, SLOT(reject()));

  QVBoxLayout *vlayout = new QVBoxLayout(dlgEditor);
  vlayout->addWidget(textEdit);
  vlayout->addWidget(buttonBox);

  // Read contents of unit file.
  QFile f(filename);
  if (!f.open(QFile::ReadOnly | QFile::Text)) {
    displayMsgWidget(KMessageWidget::Error,
                     i18n("Failed to open the unit file:\n%1", filename));
    return;
  }
  QTextStream in(&f);
  textEdit->setPlainText(in.readAll());
  textEdit->setMinimumSize(500,300);

  if (dlgEditor->exec() == QDialog::Accepted) {

    // Declare a QVariantMap with arguments for the helper.
    QVariantMap helperArgs;
    helperArgs["file"] = filename;
    helperArgs["contents"] = textEdit->toPlainText();

    // Create the action.
    Action action("org.kde.kcontrol.kcmsystemd.saveunitfile");
    action.setHelperId("org.kde.kcontrol.kcmsystemd");
    action.setArguments(helperArgs);

    ExecuteJob *job = action.execute();
    if (!job->exec()) {
      displayMsgWidget(KMessageWidget::Error,
                       i18n("Unable to authenticate/execute the action: %1", job->error()));
    } else {
      displayMsgWidget(KMessageWidget::Positive,
                       i18n("Unit file successfully written."));
    }
  }
  delete dlgEditor;
}

QList<SystemdUnit> kcmsystemd::getUnitsFromDbus(dbusBus bus)
{
  // get an updated list of units via dbus

  QList<SystemdUnit> list;
  QList<unitfile> unitfileslist;
  QDBusMessage dbusreply;

  dbusreply = callDbusMethod("ListUnits", sysdMgr, bus);

  if (dbusreply.type() == QDBusMessage::ReplyMessage)
  {

    const QDBusArgument argUnits = dbusreply.arguments().at(0).value<QDBusArgument>();
    int tal = 0;
    if (argUnits.currentType() == QDBusArgument::ArrayType)
    {
      argUnits.beginArray();
      while (!argUnits.atEnd())
      {
        SystemdUnit unit;
        argUnits >> unit;
        list.append(unit);

        // qDebug() << "Added unit " << unit.id;
        tal++;
      }
      argUnits.endArray();
    }
    // qDebug() << "Added " << tal << " units on bus " << bus;
    tal = 0;

    // Get a list of unit files
    dbusreply = callDbusMethod("ListUnitFiles", sysdMgr, bus);
    const QDBusArgument argUnitFiles = dbusreply.arguments().at(0).value<QDBusArgument>();
    argUnitFiles.beginArray();
    while (!argUnitFiles.atEnd())
    {
      unitfile u;
      argUnitFiles.beginStructure();
      argUnitFiles >> u.name >> u.status;
      argUnitFiles.endStructure();
      unitfileslist.append(u);
    }
    argUnitFiles.endArray();

    // Add unloaded units to the list
    for (int i = 0;  i < unitfileslist.size(); ++i)
    {
      int index = list.indexOf(SystemdUnit(unitfileslist.at(i).name.section('/',-1)));
      if (index > -1)
      {
        // The unit was already in the list, add unit file and its status
        list[index].unit_file = unitfileslist.at(i).name;
        list[index].unit_file_status = unitfileslist.at(i).status;
      }
      else
      {
        // Unit not in the list, add it
        QFile unitfile(unitfileslist.at(i).name);
        if (unitfile.symLinkTarget().isEmpty())
        {
          SystemdUnit unit;
          unit.id = unitfileslist.at(i).name.section('/',-1);
          unit.load_state = "unloaded";
          unit.active_state = '-';
          unit.sub_state = '-';
          unit.unit_file = unitfileslist.at(i).name;
          unit.unit_file_status= unitfileslist.at(i).status;
          list.append(unit);

          // qDebug() << "Added unit " << unit.id;
          tal++;
        }
      }
    }
    // qDebug() << "Added " << tal << " units from files on bus " << bus;

  }

  return list;
}

QVariant kcmsystemd::getDbusProperty(QString prop, dbusIface ifaceName, QDBusObjectPath path, dbusBus bus)
{
  // qDebug() << "Fetching property" << prop << ifaceName << path.path() << "on bus" << bus;
  QString conn, ifc;
  QDBusConnection abus("");
  if (bus == user)
    abus = QDBusConnection::connectToBus(userBusPath, connSystemd);
  else
    abus = systembus;

  if (ifaceName == sysdMgr)
  {
    conn = connSystemd;
    ifc = ifaceMgr;
  }
  else if (ifaceName == sysdUnit)
  {
    conn = connSystemd;
    ifc = ifaceUnit;
  }
  else if (ifaceName == sysdTimer)
  {
    conn = connSystemd;
    ifc = ifaceTimer;
  }
  else if (ifaceName == logdSession)
  {
    conn = connLogind;
    ifc = ifaceSession;
  }
  QVariant r;
  QDBusInterface *iface = new QDBusInterface (conn, path.path(), ifc, abus, this);
  if (iface->isValid())
  {
    r = iface->property(prop.toLatin1());
    delete iface;
    return r;
  }
  qDebug() << "Interface" << ifc << "invalid for" << path.path();
  return QVariant("invalidIface");
}

QDBusMessage kcmsystemd::callDbusMethod(QString method, dbusIface ifaceName, dbusBus bus, const QList<QVariant> &args)
{
  // qDebug() << "Calling method" << method << "with iface" << ifaceName << "on bus" << bus;
  QDBusConnection abus("");
  if (bus == user)
    abus = QDBusConnection::connectToBus(userBusPath, connSystemd);
  else
    abus = systembus;

  QDBusInterface *iface;
  if (ifaceName == sysdMgr)
    iface = new QDBusInterface (connSystemd, pathSysdMgr, ifaceMgr, abus, this);
  else if (ifaceName == logdMgr)
    iface = new QDBusInterface (connLogind, pathLogdMgr, ifaceLogdMgr, abus, this);

  QDBusMessage msg;
  if (iface->isValid())
  {
    if (args.isEmpty())
      msg = iface->call(QDBus::AutoDetect, method.toLatin1());
    else
      msg = iface->callWithArgumentList(QDBus::AutoDetect, method.toLatin1(), args);
    delete iface;
    if (msg.type() == QDBusMessage::ErrorMessage)
      qDebug() << "DBus method call failed: " << msg.errorMessage();
  }
  else
  {
    qDebug() << "Invalid DBus interface on bus" << bus;
    delete iface;
  }
  return msg;
}

void kcmsystemd::displayMsgWidget(KMessageWidget::MessageType type, QString msg)
{
  KMessageWidget *msgWidget = new KMessageWidget;
  msgWidget->setText(msg);
  msgWidget->setMessageType(type);
  ui.verticalLayoutMsg->insertWidget(0, msgWidget);
  msgWidget->animatedShow();
}

#include "kcmsystemd.moc"
