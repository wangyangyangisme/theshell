#include "menu.h"
#include "ui_menu.h"

extern void EndSession(EndSessionWait::shutdownType type);
extern MainWindow* MainWin;
extern DbusEvents* DBusEvents;

Menu::Menu(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Menu)
{
    ui->setupUi(this);

    ui->offFrame->setParent(this);
    ui->thewaveFrame->setParent(this);
    this->layout()->removeWidget(ui->offFrame);
    this->layout()->removeWidget(ui->thewaveFrame);
    ui->offFrame->setGeometry(10, -this->height(), this->width() - 20, this->height() - 20);
    ui->thewaveFrame->setGeometry(10, -this->height(), this->width() - 20, this->height() - 20);
    ui->timerIcon->setPixmap(QIcon::fromTheme("player-time").pixmap(16));
    ui->userIcon->setPixmap(QIcon::fromTheme("system-users").pixmap(16));
    ui->thewave_connection_disconnection_label->setPixmap(QIcon::fromTheme("network-disconnect").pixmap(16));
    ui->timeIcon->setPixmap(QIcon::fromTheme("player-time").pixmap(32));
    ui->callIcon->setPixmap(QIcon::fromTheme("call-start").pixmap(32));
    ui->messageIcon->setPixmap(QIcon::fromTheme("message-send").pixmap(32));
    ui->launchIcon->setPixmap(QIcon::fromTheme("system-run").pixmap(32));
    ui->infoIcon->setPixmap(QIcon::fromTheme("text-html").pixmap(32));

    this->setMouseTracking(true);

    QString name = qgetenv("USER");
    if (name.isEmpty()) {
        name = qgetenv("USERNAME");
    }

    QProcess* fullNameProc = new QProcess(this);
    fullNameProc->start("getent passwd " + name);
    fullNameProc->waitForFinished();
    QString parseName(fullNameProc->readAll());
    delete fullNameProc;
    QString fullname = parseName.split(",").at(0).split(":").last();
    if (fullname == "") {
        ui->label_2->setText("Hey, " + name + "!");
        //ui->label_4->setText(name + ", what do you want to do now?");
    } else {
        ui->label_2->setText("Hey, " + fullname + "!");
        //ui->label_4->setText(fullname + ", what do you want to do now?");
    }


    apps = new QList<App*>();
    appsShown = new QList<App*>();

    ui->listWidget->installEventFilter(this);

    this->installEventFilter(this);

    if (QFile("/usr/bin/install_theos").exists()) {
        ui->InstallLayout->setVisible(true);
    } else {
        ui->InstallLayout->setVisible(false);
    }

    QNetworkAccessManager networkManager;
    if (networkManager.networkAccessible() == QNetworkAccessManager::Accessible) {
        ui->thewaveInternetFrame->setVisible(false);
    } else {
        ui->thewaveInternetFrame->setVisible(true);
    }

    QString seatPath = QString(qgetenv("XDG_SEAT_PATH"));
    if (seatPath == "") {
        ui->commandLinkButton_4->setEnabled(false);
    } else {
        ui->commandLinkButton_4->setEnabled(true);
    }

    QPalette listPal = ui->listWidget->palette(); //Set List Palette so active colours aren't greyed out
    listPal.setBrush(QPalette::Inactive, QPalette::Highlight, listPal.brush(QPalette::Active, QPalette::Highlight));
    listPal.setBrush(QPalette::Inactive, QPalette::HighlightedText, listPal.brush(QPalette::Active, QPalette::HighlightedText));
    ui->listWidget->setPalette(listPal);

    this->theWaveFrame = ui->thewaveFrame;
}

Menu::~Menu()
{
    delete ui;
}

void Menu::show(bool openTotheWave) {
    unsigned long desktop = 0xFFFFFFFF;
    XChangeProperty(QX11Info::display(), this->winId(), XInternAtom(QX11Info::display(), "_NET_WM_DESKTOP", False),
                     XA_CARDINAL, 32, PropModeReplace, (unsigned char*) &desktop, 1); //Set visible on all desktops

    QDialog::show();
    doCheckForClose = true;

    QDir appFolder("/usr/share/applications/");
    QDirIterator iterator(appFolder, QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        QString appFile = iterator.next();
        QFile file(appFile);
        if (file.exists() & QFileInfo(file).suffix().contains("desktop")) {
            file.open(QFile::ReadOnly);
            QString appinfo(file.readAll());

            QStringList desktopLines;
            QString currentDesktopLine;
            for (QString desktopLine : appinfo.split("\n")) {
                if (desktopLine.startsWith("[") && currentDesktopLine != "") {
                    desktopLines.append(currentDesktopLine);
                    currentDesktopLine = "";
                }
                currentDesktopLine.append(desktopLine + "\n");
            }
            desktopLines.append(currentDesktopLine);

            for (QString desktopPart : desktopLines) {
                App *app = new App();
                bool isApplication = false;
                bool display = true;
                for (QString line : desktopPart.split("\n")) {
                    if (line.startsWith("genericname=", Qt::CaseInsensitive)) {
                        app->setDescription(line.split("=")[1]);
                    } else if (line.startsWith("name=", Qt::CaseInsensitive)) {
                        app->setName(line.split("=")[1]);
                    } else if (line.startsWith("icon=", Qt::CaseInsensitive)) {
                        QString iconname = line.split("=")[1];
                        QIcon icon;
                        if (QFile(iconname).exists()) {
                            icon = QIcon(iconname);
                        } else {
                            icon = QIcon::fromTheme(iconname, QIcon::fromTheme("application-x-executable"));
                        }
                        app->setIcon(icon);
                    } else if (line.startsWith("exec=", Qt::CaseInsensitive)) {
                        app->setCommand(line.split("=")[1].remove(QRegExp("%.")));
                    } else if (line.startsWith("description=", Qt::CaseInsensitive)) {
                        app->setDescription(line.split("=")[1]);
                    } else if (line.startsWith("type=", Qt::CaseInsensitive)) {
                        if (line.split("=")[1] == "Application") {
                            isApplication = true;
                        }
                    } else if (line.startsWith("nodisplay=", Qt::CaseInsensitive)) {
                        if (line.split("=")[1] == "true") {
                            display = false;
                            break;
                        }
                    } else if (line.startsWith("onlyshowin=", Qt::CaseInsensitive)) {
                        if (!line.split("=")[1].contains("theshell;")) {
                            display = false;
                        }
                    } else if (line.startsWith("notshowin=", Qt::CaseInsensitive)) {
                        if (line.split("=")[1].contains("theshell;")) {
                            display = false;
                        }
                    }
                }
                if (isApplication && display) {
                    apps->append(app);
                }
            }
        }
    }

    App *waveApp = new App();
    waveApp->setCommand("thewave");
    waveApp->setIcon(QIcon(":/icons/thewave.svg"));
    waveApp->setName("theWave");
    waveApp->setDescription("Personal Assistant");
    apps->append(waveApp);

    //hotkeyManager->registerHotkey("Power");

    for (App *app : *apps) {
        QListWidgetItem *i = new QListWidgetItem();
        if (app->description() == "") {
            i->setText(app->name());
        } else {
            i->setText(app->description() + " | " + app->name());
        }
        i->setIcon(app->icon());
        i->setData(Qt::UserRole, app->command());
        appsShown->append(app);
        ui->listWidget->addItem(i);
    }

    QPropertyAnimation* animation = new QPropertyAnimation(this, "geometry");
    animation->setStartValue(this->geometry());
    animation->setEndValue(QRect(this->x() + this->width(), this->y(), this->width(), this->height()));
    animation->setDuration(500);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start();
    connect(animation, SIGNAL(finished()), animation, SLOT(deleteLater()));

    if (openTotheWave) {
        ui->activateTheWave->click();
    }

    /*QTimer *t = new QTimer(this);
    t->setInterval(1);
    connect(t, SIGNAL(timeout()), this, SLOT(checkForclose()));
    t->start();*/
}

void Menu::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ActivationChange) {
        if (!istheWaveOpen) {
            if (!this->isActiveWindow()) {
                this->close();
            }
        }
    }
}

void Menu::close() {
    QPropertyAnimation* animation = new QPropertyAnimation(this, "geometry");
    animation->setStartValue(this->geometry());
    animation->setEndValue(QRect(this->x() - this->width(), this->y(), this->width(), this->height()));
    animation->setDuration(500);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start();
    connect(animation, SIGNAL(finished()), animation, SLOT(deleteLater()));
    connect(animation, &QPropertyAnimation::finished, [=]() {
        emit menuClosing();
        QDialog::close();
    });

    doCheckForClose = false;
}

void Menu::checkForclose() {
    if (doCheckForClose) {
        if (!checkFocus(this->layout())) {
            this->close();
        }
    }
}

bool Menu::checkFocus(QLayout *layout) {
    bool hasFocus = this->isActiveWindow();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != NULL) {
        if (item->layout() != 0) {
            if (checkFocus(item->layout())) {
                hasFocus = true;
            }
        } else if (item->widget() != 0) {
            if (item->widget()->hasFocus()) {
                hasFocus = true;
            }
        }
    }
    return hasFocus;
}

void Menu::on_pushButton_clicked()
{
    bool showWarningPane = false;
    if (MainWin->getInfoPane()->isTimerRunning()) {
        showWarningPane = true;
        ui->timerIcon->setVisible(true);
        ui->timerLabel->setVisible(true);
    } else {
        ui->timerIcon->setVisible(false);
        ui->timerLabel->setVisible(false);
    }

    if (false) {
        showWarningPane = true;
        ui->userIcon->setVisible(true);
        ui->userLabel->setVisible(true);
    } else {
        ui->userIcon->setVisible(false);
        ui->userLabel->setVisible(false);
    }

    if (showWarningPane) {
        ui->shutdownText->setText("Before you power off your PC, you may want to check this.");
        ui->shutdownWarnings->setVisible(true);
    } else {
        ui->shutdownText->setText("You're about to power off your PC. Are you sure?");
        ui->shutdownWarnings->setVisible(false);
    }

    QPropertyAnimation* anim = new QPropertyAnimation(ui->offFrame, "geometry");
    anim->setStartValue(QRect(10, this->height(), this->width() - 20, this->height() - 20));
    anim->setEndValue(QRect(10, 10, this->width() - 20, this->height() - 20));
    anim->setDuration(500);

    anim->setEasingCurve(QEasingCurve::OutCubic);

    anim->start();
}

void Menu::on_pushButton_2_clicked()
{
    QPropertyAnimation* anim = new QPropertyAnimation(ui->offFrame, "geometry");
    anim->setStartValue(QRect(10, 10, this->width() - 20, this->height() - 20));
    anim->setEndValue(QRect(10, this->height(), this->width() - 20, this->height() - 20));
    anim->setDuration(500);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    anim->start();
}

void Menu::on_commandLinkButton_clicked()
{
    EndSession(EndSessionWait::powerOff);
}

void Menu::on_commandLinkButton_2_clicked()
{
    EndSession(EndSessionWait::reboot);
}

void Menu::on_commandLinkButton_3_clicked()
{
    EndSession(EndSessionWait::logout);
}

void Menu::on_listWidget_itemClicked(QListWidgetItem *item)
{
    if (item->data(Qt::UserRole).toString().startsWith("thewave")) {
        ui->activateTheWave->click();
        if (item->data(Qt::UserRole).toString().split(":").count() > 1) {
            ui->thewave_line->setText(item->data(Qt::UserRole).toString().split(":").at(1));
            on_thewave_line_returnPressed();
        }
    } else if (item->data(Qt::UserRole).toString().startsWith("power:")) {
        QString operation = item->data(Qt::UserRole).toString().split(":").at(1);
        if (operation == "off") {
            EndSession(EndSessionWait::powerOff);
        } else if (operation == "reboot") {
            EndSession(EndSessionWait::reboot);
        } else if (operation ==  "logout") {
            EndSession(EndSessionWait::logout);
        }
    } else {
        QProcess::startDetached(item->data(Qt::UserRole).toString().remove("%u"));
        this->close();
    }
}

void Menu::on_lineEdit_textEdited(const QString &arg1)
{
    ui->listWidget->clear();
    appsShown->clear();
    if (arg1 == "") {
        for (App *app : *apps) {
            QListWidgetItem *i = new QListWidgetItem();
            if (app->description() == "") {
                i->setText(app->name());
            } else {
                i->setText(app->description() + " | " + app->name());
            }
            i->setIcon(app->icon());
            i->setData(Qt::UserRole, app->command());
            appsShown->append(app);
            ui->listWidget->addItem(i);
        }
    } else {
        bool showtheWaveOption = true;
        if (arg1.toLower() == "emergency call") {
            QListWidgetItem *callItem = new QListWidgetItem();
            callItem->setText("Place a call");
            callItem->setIcon(QIcon::fromTheme("call-start"));
            callItem->setData(Qt::UserRole, "thewave:emergency call");
            ui->listWidget->addItem(callItem);

            QListWidgetItem *call = new QListWidgetItem();
            call->setText("Emergency Call");
            call->setData(Qt::UserRole, "thewave:emergency call");
            call->setIcon(QIcon(":/icons/blank.svg"));
            QFont font = call->font();
            font.setPointSize(30);
            call->setFont(font);
            ui->listWidget->addItem(call);
            showtheWaveOption = false;
        } else if ((arg1.startsWith("call") && arg1.count() == 4) || arg1.startsWith("call ")) {
            QListWidgetItem *call = new QListWidgetItem();
            QString parse = arg1;
            if (arg1.count() == 4 || arg1.count() == 5) {
                call->setText("Place a call");
                call->setData(Qt::UserRole, "thewave:call");
                call->setIcon(QIcon::fromTheme("call-start"));
            } else {
                parse.remove(0, 5);
                QListWidgetItem *callItem = new QListWidgetItem();
                callItem->setText("Place a call");
                callItem->setIcon(QIcon::fromTheme("call-start"));
                callItem->setData(Qt::UserRole, "thewave:call " + parse);
                ui->listWidget->addItem(callItem);

                call->setText((QString) parse.at(0).toUpper() + parse.right(parse.length() - 1) + "");
                call->setData(Qt::UserRole, "thewave:call " + parse);
                call->setIcon(QIcon(":/icons/blank.svg"));

                QFont font = call->font();
                font.setPointSize(30);
                call->setFont(font);
            }
            ui->listWidget->addItem(call);
            showtheWaveOption = false;
        } else if (arg1.startsWith("pause") && arg1.count() == 5) {
            QListWidgetItem *pause = new QListWidgetItem();
            pause->setText("Pause");
            pause->setIcon(QIcon::fromTheme("media-playback-pause"));
            pause->setData(Qt::UserRole, "media:pause");
            ui->listWidget->addItem(pause);
            showtheWaveOption = false;
        } else if (arg1.startsWith("weather")) {
            QListWidgetItem *weather = new QListWidgetItem();

            QListWidgetItem *weatherItem = new QListWidgetItem();
            weatherItem->setText("Weather");
            weatherItem->setIcon(QIcon::fromTheme("weather-clear"));
            weatherItem->setData(Qt::UserRole, "thewave:weather");
            ui->listWidget->addItem(weatherItem);

            weather->setText("Unknown");
            weather->setData(Qt::UserRole, "thewave:weather");
            weather->setIcon(QIcon::fromTheme("dialog-error"));

            QFont font = weather->font();
            font.setPointSize(30);
            weather->setFont(font);

            ui->listWidget->addItem(weather);
            showtheWaveOption = false;
        }

        for (App *app : *apps) {
            if (app->name().contains(arg1, Qt::CaseInsensitive) || app->description().contains(arg1, Qt::CaseInsensitive)) {
                QListWidgetItem *i = new QListWidgetItem();
                if (app->description() == "") {
                    i->setText(app->name());
                } else {
                    i->setText(app->description() + " | " + app->name());
                }
                i->setIcon(app->icon());
                i->setData(Qt::UserRole, app->command());
                appsShown->append(app);
                ui->listWidget->addItem(i);

            }
        }

        if (QString("shutdown").contains(arg1, Qt::CaseInsensitive) || QString("power off").contains(arg1, Qt::CaseInsensitive)) {
            QListWidgetItem *i = new QListWidgetItem();
            i->setText("Power Off");
            i->setIcon(QIcon::fromTheme("system-shutdown"));
            i->setData(Qt::UserRole, "power:off");
            ui->listWidget->addItem(i);
        } else if (QString("restart").contains(arg1, Qt::CaseInsensitive) || QString("reboot").contains(arg1, Qt::CaseInsensitive)) {
            QListWidgetItem *i = new QListWidgetItem();
            i->setText("Reboot");
            i->setIcon(QIcon::fromTheme("system-reboot"));
            i->setData(Qt::UserRole, "power:reboot");
            ui->listWidget->addItem(i);
        } else if (QString("logout").contains(arg1, Qt::CaseInsensitive) || QString("logoff").contains(arg1, Qt::CaseInsensitive)) {
            QListWidgetItem *i = new QListWidgetItem();
            i->setText("Log Out");
            i->setIcon(QIcon::fromTheme("system-log-out"));
            i->setData(Qt::UserRole, "power:logout");
            ui->listWidget->addItem(i);
        }

        QString pathEnv = QProcessEnvironment::systemEnvironment().value("PATH");
        for (QString env : pathEnv.split(":")) {
            if (QFile(env.append("/" + arg1.split(" ")[0])).exists()) {
                App* app = new App();
                app->setName(arg1);
                app->setCommand(arg1);
                app->setIcon(QIcon::fromTheme("system-run"));
                appsShown->append(app);

                QListWidgetItem *i = new QListWidgetItem();
                i->setText(app->name());
                i->setIcon(app->icon());
                i->setData(Qt::UserRole, app->command());
                ui->listWidget->addItem(i);
                break;
            }
        }

        QUrl uri = QUrl::fromUserInput(arg1);
        if (uri.scheme() == "http" || uri.scheme() == "https") {
            App* app = new App();
            app->setName("Go to " + uri.toDisplayString());
            app->setCommand("xdg-open \"" + uri.toString() + "\"");
            app->setIcon(QIcon::fromTheme("text-html"));
            appsShown->append(app);

            QListWidgetItem *i = new QListWidgetItem();
            i->setText(app->name());
            i->setIcon(app->icon());
            i->setData(Qt::UserRole, app->command());
            ui->listWidget->addItem(i);
        } else if (uri.scheme() == "file") {
            if (QDir(uri.path() + "/").exists()) {
                App* app = new App();
                app->setName("Open " + uri.path());
                app->setCommand("xdg-open \"" + uri.toString() + "\"");
                app->setIcon(QIcon::fromTheme("system-file-manager"));
                appsShown->append(app);

                QListWidgetItem *i = new QListWidgetItem();
                i->setText(app->name());
                i->setIcon(app->icon());
                i->setData(Qt::UserRole, app->command());
                ui->listWidget->addItem(i);
            } else if (QFile(uri.path()).exists()) {
                App* app = new App();
                app->setName("Open " + uri.path());
                app->setCommand("xdg-open \"" + uri.toString() + "\"");
                QFile f(uri.toString());
                QFileInfo info(f);
                QMimeType mime = (new QMimeDatabase())->mimeTypeForFile(info);
                app->setIcon(QIcon::fromTheme(mime.iconName(), QIcon::fromTheme("application-octet-stream")));
                appsShown->append(app);

                QListWidgetItem *i = new QListWidgetItem();
                i->setText(app->name());
                i->setIcon(app->icon());
                i->setData(Qt::UserRole, app->command());
                ui->listWidget->addItem(i);
            }
        }

        if (showtheWaveOption) {
            QListWidgetItem *wave = new QListWidgetItem();
            wave->setText("Ask theWave about \"" + arg1 + "\"");
            wave->setIcon(QIcon(":/icons/thewave.svg"));
            wave->setData(Qt::UserRole, "thewave:" + arg1);
            ui->listWidget->addItem(wave);
        }
    }


}

bool Menu::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress && ((QKeyEvent*) event)->key() == Qt::Key_Escape) {
        this->close();
        return true;
    } else {
        if (object != ui->thewave_line && object != ui->lineEdit) {
            if (event->type() == QEvent::KeyPress) {
                QKeyEvent *e = (QKeyEvent*) event;
                if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
                    if (!istheWaveReady) {
                        if (object != ui->lineEdit && object != ui->thewave_line && object != this) {
                            /*if (object == ui->listWidget) {
                                on_lineEdit_returnPressed();
                            } else {
                                ((QPushButton*) object)->click();
                            }*/
                            on_lineEdit_returnPressed();
                        }
                    }
                    return false;
                } else {
                    if (istheWaveReady) {
                        ui->thewave_line->setText(ui->thewave_line->text().append(e->text())); //Type letter into theWave
                        ui->thewave_line->setFocus();
                    } else {
                        if (e->key() != Qt::Key_Up && e->key() != Qt::Key_Down) {
                            ui->lineEdit->setText(ui->lineEdit->text().append(e->text())); //Type letter into search box
                            ui->lineEdit->setFocus();
                            on_lineEdit_textEdited(ui->lineEdit->text());
                        } else {
                            int currentRow;
                            if (ui->listWidget->selectedItems().count() == 0) {
                                currentRow = -1;
                            } else {
                                currentRow = ui->listWidget->selectionModel()->selectedIndexes().first().row();
                            }

                            if (e->key() == Qt::Key_Down) {
                                if (currentRow == -1) {
                                    ui->listWidget->item(0)->setSelected(true);
                                    ui->listWidget->scrollToItem(ui->listWidget->item(0));
                                } else if (currentRow == ui->listWidget->count() - 1) {
                                    ui->listWidget->item(0)->setSelected(true);
                                    ui->listWidget->scrollToItem(ui->listWidget->item(0));
                                } else {
                                    ui->listWidget->item(currentRow + 1)->setSelected(true);
                                    ui->listWidget->scrollToItem(ui->listWidget->item(currentRow + 1));
                                }
                            } else if (e->key() == Qt::Key_Up) {
                                if (currentRow == -1) {
                                    ui->listWidget->item(ui->listWidget->count() - 1)->setSelected(true);
                                    ui->listWidget->scrollToItem(ui->listWidget->item(ui->listWidget->count() - 1));
                                } else if (currentRow == 0) {
                                    ui->listWidget->item(ui->listWidget->count() - 1)->setSelected(true);
                                    ui->listWidget->scrollToItem(ui->listWidget->item(ui->listWidget->count() - 1));
                                } else {
                                    ui->listWidget->item(currentRow - 1)->setSelected(true);
                                    ui->listWidget->scrollToItem(ui->listWidget->item(currentRow - 1));
                                }
                            }
                        }
                    }
                }
                e->ignore();
                return true;
            } else {
                return QDialog::eventFilter(object, event);
            }
        } else {
            return QDialog::eventFilter(object, event);
        }
    }
}

void Menu::on_lineEdit_returnPressed()
{
    if (ui->listWidget->count() > 0) {
        if (ui->listWidget->selectedItems().count() == 1) {
            on_listWidget_itemClicked(ui->listWidget->selectedItems().first());
        } else {
            ui->listWidget->selectedItems().append(ui->listWidget->item(0));
            on_listWidget_itemClicked(ui->listWidget->item(0));
        }
    }
}

void Menu::on_pushButton_3_clicked()
{
    QProcess::startDetached("install_theos");
    this->close();
}

void Menu::on_commandLinkButton_5_clicked()
{
    EndSession(EndSessionWait::suspend);
    this->close();
}

void Menu::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setPen(this->palette().color(QPalette::WindowText));
    painter.drawLine(this->width() - 1, 0, this->width() - 1, this->height());
    event->accept();
}


void Menu::setGeometry(int x, int y, int w, int h) { //Use wmctrl command because KWin has a problem with moving windows offscreen.
    QDialog::setGeometry(x, y, w, h);
    QProcess::execute("wmctrl -r " + this->windowTitle() + " -e 0," +
                      QString::number(x) + "," + QString::number(y) + "," +
                      QString::number(w) + "," + QString::number(h));
}

void Menu::setGeometry(QRect geometry) {
    this->setGeometry(geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

void Menu::on_commandLinkButton_7_clicked()
{
    EndSession(EndSessionWait::hibernate);
    this->close();
}

void Menu::on_commandLinkButton_8_clicked()
{
    this->close();
    QProcess::startDetached("xset dpms force off");
}

void Menu::on_activateTheWave_clicked()
{
    istheWaveOpen = true;
    this->resetFrames();
    QPropertyAnimation* anim = new QPropertyAnimation(ui->thewaveFrame, "geometry");
    anim->setStartValue(QRect(10, this->height(), this->width() - 20, this->height() - 20));
    anim->setEndValue(QRect(10, 10, this->width() - 20, this->height() - 20));
    anim->setDuration(500);

    anim->setEasingCurve(QEasingCurve::OutCubic);

    anim->start();

    QThread *t = new QThread();
    waveWorker = new theWaveWorker(this);
    waveWorker->moveToThread(t);
    //connect(t, SIGNAL(started()), waveWorker, SLOT(begin()));
    connect(t, &QThread::started, [=]() {
       this->istheWaveReady = true;
    });
    connect(waveWorker, SIGNAL(finished()), waveWorker, SLOT(deleteLater()));
    connect(t, SIGNAL(finished()), t, SLOT(deleteLater()));
    connect(waveWorker, &theWaveWorker::outputResponse, [=](QString response) {
        ui->thewave_response->setText(response);
    });
    connect(waveWorker, SIGNAL(outputSpeech(QString)), this, SLOT(thewave_outputSpeech(QString)));
    connect(waveWorker, &theWaveWorker::startedListening, [=]() {
        //ui->pushButton->setIcon(QIcon::fromTheme("mic-on"));
        isListening = true;
    });
    connect(waveWorker, &theWaveWorker::stoppedListening, [=]() {
        //ui->pushButton->setIcon(QIcon::fromTheme("mic-off"));
        isListening = false;
    });
    connect(waveWorker, &theWaveWorker::doLaunchApp, [=](QString appName) {
        for (App *app : *this->apps) {
            if (app->name() == appName) {
                QProcess::startDetached(app->command().remove("%u"));
                this->close();
            }
        }
    });
    connect(waveWorker, SIGNAL(showCallFrame(bool)), this, SLOT(showCallFrame(bool))); //Call
    connect(waveWorker, SIGNAL(resetFrames()), this, SLOT(resetFrames())); //Reset
    connect(waveWorker, SIGNAL(showMessageFrame()), this, SLOT(showMessageFrame())); //Text Message
    connect(waveWorker, SIGNAL(showHelpFrame()), this, SLOT(showHelpFrame())); //Help
    connect(waveWorker, SIGNAL(showWikipediaFrame(QString,QString)), this, SLOT(showWikipediaFrame(QString,QString))); //Wikipedia
    connect(waveWorker, SIGNAL(launchApp(QString)), this, SLOT(thewave_launchapp(QString))); //Launch
    connect(waveWorker, SIGNAL(setTimer(QTime)), MainWin->getInfoPane(), SLOT(startTimer(QTime))); //Start a timer
    connect(waveWorker, SIGNAL(showFlightFrame(QString)), this, SLOT(showFlightFrame(QString))); //Flight
    connect(waveWorker, SIGNAL(loudnessChanged(qreal)), this, SLOT(thewaveLoudnessChanged(qreal))); //Input Loudness
    connect(waveWorker, SIGNAL(showSettingsFrame(QIcon,QString,bool)), this, SLOT(showSettingFrame(QIcon,QString,bool))); //Settings
    connect(waveWorker, SIGNAL(showMathematicsFrame(QString,QString)), this, SLOT(showMathematicsFrame(QString,QString))); //Mathematics
    connect(waveWorker, SIGNAL(showMediaFrame(QPixmap,QString,QString,bool)), this, SLOT(showMediaFrame(QPixmap,QString,QString,bool))); //Media Player
    connect(this, SIGNAL(thewave_processText(QString,bool)), waveWorker, SLOT(processSpeech(QString,bool))); //Manual Input Text Processing
    connect(this, SIGNAL(thewaveBegin()), waveWorker, SLOT(begin())); //Begin
    connect(this, SIGNAL(thewaveStop()), waveWorker, SLOT(endAndProcess())); //Stop
    connect(this, SIGNAL(thewave_sayLaunchApp(QString)), waveWorker, SLOT(launchAppReply(QString))); //Launch App User Reply
    connect(this, SIGNAL(thewave_sayLaunchApp_disambiguation(QStringList)), waveWorker, SLOT(launchApp_disambiguation(QStringList))); //Lauch App Disambiguation Reply
    connect(this, SIGNAL(currentSettingChanged(bool)), waveWorker, SLOT(currentSettingChanged(bool)));
    /*connect(w, &speechWorker::outputFrame, [=](QFrame *frame) {
        ui->frame->layout()->addWidget(frame);
    });*/

    t->start();
}

void Menu::thewaveLoudnessChanged(qreal loudness) {
    if (loudness == -1) {
        ui->speechBar->setMaximum(0);
        ui->speechBar->setValue(0);
    } else if (loudness == -2) {
        ui->speechBar->setVisible(false);
    } else {
        ui->speechBar->setMaximum(100);
        ui->speechBar->setValue(loudness * 100);
        ui->speechBar->setVisible(true);
    }
}

void Menu::thewave_outputSpeech(QString speech) {
    QString displaySpeech = speech;
    if (settings.value("thewave/blockOffensiveWords").toBool()) {
        displaySpeech.replace("shit", "s***");
        displaySpeech.replace("fuck", "f***");
    }
    ui->thewave_line->setText(displaySpeech);
}

void Menu::on_closetheWaveButton_clicked()
{
    this->istheWaveReady = false;
    this->resetFrames();
    ui->thewave_response->setText("Hit \"Speak\" to start speaking.");
    ui->thewave_line->setText("");

    QPropertyAnimation* anim = new QPropertyAnimation(ui->thewaveFrame, "geometry");
    anim->setStartValue(QRect(10, 10, this->width() - 20, this->height() - 20));
    anim->setEndValue(QRect(10, this->height(), this->width() - 20, this->height() - 20));
    anim->setDuration(500);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    anim->start();

    if (waveWorker != NULL) {
        //waveWorker->quit(); //This deletes the worker thread.
        waveWorker->deleteLater();
        waveWorker = NULL;
    }

    istheWaveOpen = false;
}

void Menu::showCallFrame(bool emergency) {
    ui->thewave_callFrame->setVisible(true);
}

void Menu::showMessageFrame() {
    ui->thewave_messageframe->setVisible(true);
}

void Menu::showHelpFrame() {
    ui->thewave_helpFrame->setVisible(true);
}

void Menu::resetFrames() {
    ui->thewave_callFrame->setVisible(false);
    ui->thewave_messageframe->setVisible(false);
    ui->thewave_helpFrame->setVisible(false);
    ui->wikipediaFrame->setVisible(false);
    ui->thewave_spacerFrame->setVisible(true);
    ui->thewave_launchFrame->setVisible(false);
    ui->thewaveWeatherFrame->setVisible(false);
    ui->thewave_flightFrame->setVisible(false);
    ui->thewaveSettingsFrame->setVisible(false);
    ui->thewaveMathematicsFrame->setVisible(false);
    ui->thewaveMediaFrame->setVisible(false);
}

void Menu::showMediaFrame(QPixmap art, QString title, QString artist, bool isPlaying) {
    ui->thewaveMedia_Art->setPixmap(art.scaled(32, 32));
    ui->thewaveMedia_Title->setText(title);
    ui->thewaveMedia_Artist->setText(artist);
    if (isPlaying) {
        ui->thewaveMedia_Play->setIcon(QIcon::fromTheme("media-playback-pause"));
    } else {
        ui->thewaveMedia_Play->setIcon(QIcon::fromTheme("media-playback-start"));
    }
    ui->thewaveMediaFrame->setVisible(true);
}

void Menu::showWikipediaFrame(QString title, QString text) {
    ui->wikipediaTitle->setText(title);
    ui->wikipediaText->setHtml(text);
    ui->wikipediaFrame->setVisible(true);
    ui->thewave_spacerFrame->setVisible(false);
}

void Menu::showFlightFrame(QString flight) {
    ui->flightNumber->setText(flight);
    ui->flightImage->setPixmap(QIcon(":/icons/flight/unknown.svg").pixmap(500, 70));
    ui->thewave_flightFrame->setVisible(true);
}

void Menu::showSettingFrame(QIcon icon, QString text, bool isOn) {
    ui->thewaveSettingsFrame_icon->setPixmap(icon.pixmap(64, 64));
    ui->thewaveSettingsFrame_Name->setText(text);
    ui->thewaveSettingsFrame_Switch->setChecked(isOn);
    ui->thewaveSettingsFrame->setVisible(true);
}

void Menu::showMathematicsFrame(QString expression, QString answer) {
    ui->thewaveMathematicsFrame->setVisible(true);
    ui->thewaveMathematics_expression->setText(expression);
    ui->thewaveMathematics_answer->setText(answer);
}

void Menu::on_thewave_line_returnPressed()
{
    while (!istheWaveReady) {
        QApplication::processEvents();
    }
    emit thewave_processText(ui->thewave_line->text());
    thewave_outputSpeech(ui->thewave_line->text());
}

void Menu::on_closetheWaveButton_2_clicked()
{
    QProcess::startDetached("xdg-open https://en.wikipedia.org/wiki/" + ui->wikipediaTitle->text().replace(" ", "_"));
    this->close();
}

void Menu::thewave_launchapp(QString appName) {
    ui->thewave_launchFrame->setVisible(true);
    QList<App*> apps;
    for (App *app : *this->apps) {
        if (app->name().remove(" ").contains(appName.remove(" "), Qt::CaseInsensitive)) {
            apps.append(app);
        }
    }

    if (apps.count() == 0) {
        ui->thewave_launch_appIcon->setVisible(false);
        ui->thewave_launch_appName->setVisible(false);
        ui->thewave_launch_error->setVisible(true);
        ui->thewave_launch_launchapp->setVisible(false);
        ui->thewave_spacerFrame->setVisible(true);
    } else if (apps.count() == 1) {
        ui->thewave_launch_appIcon->setVisible(true);
        ui->thewave_launch_appName->setVisible(true);
        ui->thewave_launch_error->setVisible(false);
        ui->thewave_launchOneAppFrame->setVisible(true);
        ui->thewave_launch_disambiguation->setVisible(false);
        ui->thewave_launch_launchapp->setVisible(true);
        ui->thewave_spacerFrame->setVisible(true);

        ui->thewave_launch_appName->setText(apps.first()->name());
        ui->thewave_launch_appIcon->setPixmap(apps.first()->icon().pixmap(64));
        ui->thewave_launch_launchapp->setProperty("appcommand", apps.first()->command());


        emit thewave_sayLaunchApp(apps.first()->name());
    } else {
        ui->thewave_launch_error->setVisible(false);
        ui->thewave_launchOneAppFrame->setVisible(false);
        ui->thewave_launch_disambiguation->setVisible(true);
        ui->thewave_spacerFrame->setVisible(false);

        ui->thewave_launch_disambiguation->clear();
        QStringList appNames;
        for (App *app : apps) {
            QListWidgetItem* item = new QListWidgetItem();
            item->setText(app->name());
            item->setIcon(app->icon());
            item->setData(Qt::UserRole, app->command());
            ui->thewave_launch_disambiguation->addItem(item);
            appNames.append(app->name());
        }

        emit (thewave_sayLaunchApp_disambiguation(appNames));
    }
}

void Menu::on_thewave_launch_launchapp_clicked()
{
    QProcess::startDetached(ui->thewave_launch_launchapp->property("appcommand").toString().remove("%u"));
    this->close();
}

void Menu::on_commandLinkButton_6_clicked()
{
    this->close();
    DBusEvents->LockScreen();
}

void Menu::on_commandLinkButton_4_clicked()
{
    this->close();
    DBusEvents->LockScreen();

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DisplayManager", QString(qgetenv("XDG_SEAT_PATH")), "org.freedesktop.DisplayManager.Seat", "SwitchToGreeter");
    QDBusConnection::systemBus().send(message);
}

void Menu::on_listentheWave_clicked()
{
    if (this->isListening) {
        emit thewaveStop();
    } else {
        emit thewaveBegin();
    }
}

void Menu::on_thewave_launch_disambiguation_itemClicked(QListWidgetItem *item)
{
    QProcess::startDetached(item->data(Qt::UserRole).toString().remove("%u"));
    this->close();
}

void Menu::on_thewaveSettingsFrame_Switch_toggled(bool checked)
{
    emit currentSettingChanged(checked);
}

void Menu::on_thewaveMedia_Next_clicked()
{
    MainWin->nextSong();
}

void Menu::on_thewaveMedia_Play_clicked()
{
    MainWin->playPause();
}

void Menu::on_thewaveMedia_Back_clicked()
{
    MainWin->previousSong();
}
