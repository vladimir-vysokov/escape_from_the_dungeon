#include "mainwindow.h"

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <queue>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    generateMap();
}

void MainWindow::setupUi()
{
    setWindowTitle("Побег из подземелья");
    setFixedSize(820, 720);
    setFocusPolicy(Qt::StrongFocus);

    QWidget *central = new QWidget(this);
    central->setStyleSheet("background-color: #111827; color: #f9fafb;");
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    QFrame *topPanel = new QFrame(central);
    topPanel->setStyleSheet(
        "QFrame { background-color: #172033; border: 1px solid #334155; border-radius: 8px; }"
        "QLabel { border: none; }");
    QVBoxLayout *topLayout = new QVBoxLayout(topPanel);
    topLayout->setContentsMargins(18, 10, 18, 10);

    titleLabel = new QLabel("Побег из подземелья", topPanel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 26px; font-weight: bold; color: #fbbf24;");

    statsLabel = new QLabel(topPanel);
    statsLabel->setAlignment(Qt::AlignCenter);
    statsLabel->setStyleSheet("font-size: 17px; color: #e5e7eb;");

    topLayout->addWidget(titleLabel);
    topLayout->addWidget(statsLabel);
    mainLayout->addWidget(topPanel);

    QWidget *mapWidget = new QWidget(central);
    mapLayout = new QGridLayout(mapWidget);
    mapLayout->setSpacing(2);
    mapLayout->setContentsMargins(0, 0, 0, 0);

    for (int row = 0; row < Rows; ++row) {
        QVector<QLabel*> line;
        for (int col = 0; col < Cols; ++col) {
            QLabel *cell = new QLabel(mapWidget);
            cell->setFixedSize(44, 44);
            cell->setAlignment(Qt::AlignCenter);
            mapLayout->addWidget(cell, row, col);
            line.push_back(cell);
        }
        cells.push_back(line);
    }

    QHBoxLayout *centerLayout = new QHBoxLayout;
    centerLayout->addStretch();
    centerLayout->addWidget(mapWidget);
    centerLayout->addStretch();
    mainLayout->addLayout(centerLayout, 1);

    QFrame *bottomPanel = new QFrame(central);
    bottomPanel->setStyleSheet(
        "QFrame { background-color: #172033; border: 1px solid #334155; border-radius: 8px; }"
        "QPushButton { background-color: #2563eb; color: white; border: none; border-radius: 6px;"
        " padding: 10px 18px; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3b82f6; }"
        "QPushButton:pressed { background-color: #1d4ed8; }");
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(16, 12, 16, 12);
    bottomLayout->setSpacing(12);

    newGameButton = new QPushButton("Новая игра", bottomPanel);
    rulesButton = new QPushButton("Правила", bottomPanel);
    exitButton = new QPushButton("Выход", bottomPanel);

    bottomLayout->addStretch();
    bottomLayout->addWidget(newGameButton);
    bottomLayout->addWidget(rulesButton);
    bottomLayout->addWidget(exitButton);
    bottomLayout->addStretch();
    mainLayout->addWidget(bottomPanel);

    connect(newGameButton, &QPushButton::clicked, this, [this]() {
        dungeonNumber = 1;
        generateMap();
        setFocus();
    });
    connect(rulesButton, &QPushButton::clicked, this, [this]() {
        showRules();
        setFocus();
    });
    connect(exitButton, &QPushButton::clicked, this, []() {
        QApplication::quit();
    });
}

void MainWindow::generateMap()
{
    int startWalls = 20;
    bool ok = false;

    for (wallCount = startWalls; wallCount >= 8 && !ok; wallCount -= 4) {
        for (int attempt = 0; attempt < 1000 && !ok; ++attempt) {
            map.assign(Rows, QString(Cols, QChar('.')));

            for (int row = 0; row < Rows; ++row) {
                for (int col = 0; col < Cols; ++col) {
                    if (row == 0 || col == 0 || row == Rows - 1 || col == Cols - 1) {
                        map[row][col] = QChar('#');
                    }
                }
            }

            playerRow = 1;
            playerCol = 1;
            map[playerRow][playerCol] = QChar('P');
            map[Rows - 2][Cols - 2] = QChar('E');
            placeRandomObjects();
            ok = isMapValid();
        }
    }

    if (!ok) {
        QMessageBox::warning(this, "Генератор", "Не удалось создать хорошее подземелье.");
    }

    health = 100;
    coins = 0;
    moves = 0;
    hasKey = false;
    gameEnded = false;
    updateStats();
    drawMap();
}

void MainWindow::placeRandomObjects()
{
    auto placeOne = [this](QChar object, bool dangerous) {
        while (true) {
            int row = QRandomGenerator::global()->bounded(1, Rows - 1);
            int col = QRandomGenerator::global()->bounded(1, Cols - 1);
            if (map[row][col] != QChar('.')) {
                continue;
            }
            if (dangerous && isSafeZone(row, col)) {
                continue;
            }
            map[row][col] = object;
            break;
        }
    };

    for (int i = 0; i < wallCount; ++i) placeOne(QChar('#'), true);
    for (int i = 0; i < 6; ++i) placeOne(QChar('C'), false);
    for (int i = 0; i < 5; ++i) placeOne(QChar('T'), true);
    for (int i = 0; i < 3; ++i) placeOne(QChar('H'), false);
    for (int i = 0; i < 3; ++i) placeOne(QChar('M'), true);
    placeOne(QChar('K'), false);
}

bool MainWindow::isSafeZone(int row, int col)
{
    bool nearPlayer = qAbs(row - 1) <= 1 && qAbs(col - 1) <= 1;
    bool nearExit = qAbs(row - (Rows - 2)) <= 1 && qAbs(col - (Cols - 2)) <= 1;
    return nearPlayer || nearExit;
}

bool MainWindow::isMapValid()
{
    QVector<QVector<bool>> visited = bfsReachable();
    bool keyFound = false;
    bool exitFound = visited[Rows - 2][Cols - 2];

    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            if (map[row][col] == QChar('K') && visited[row][col]) {
                keyFound = true;
            }
        }
    }

    return keyFound && exitFound && countReachableCoins(visited) >= NeedCoins;
}

QVector<QVector<bool>> MainWindow::bfsReachable()
{
    QVector<QVector<bool>> visited(Rows, QVector<bool>(Cols, false));
    std::queue<QPair<int, int>> queue;

    visited[1][1] = true;
    queue.push(qMakePair(1, 1));

    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};

    while (!queue.empty()) {
        QPair<int, int> current = queue.front();
        queue.pop();

        for (int i = 0; i < 4; ++i) {
            int row = current.first + dr[i];
            int col = current.second + dc[i];
            if (row < 0 || row >= Rows || col < 0 || col >= Cols) {
                continue;
            }
            if (!visited[row][col] && map[row][col] != QChar('#')) {
                visited[row][col] = true;
                queue.push(qMakePair(row, col));
            }
        }
    }

    return visited;
}

int MainWindow::countReachableCoins(const QVector<QVector<bool>> &visited)
{
    int count = 0;
    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            if (map[row][col] == QChar('C') && visited[row][col]) {
                ++count;
            }
        }
    }
    return count;
}

void MainWindow::drawMap()
{
    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            QChar cell = map[row][col];
            QString text;
            QString color;

            if (cell == QChar('#')) {
                color = "#020617";
            } else if (cell == QChar('P')) {
                text = "🧍";
                color = "#38bdf8";
            } else if (cell == QChar('C')) {
                text = "🟡";
                color = "#475569";
            } else if (cell == QChar('K')) {
                text = "🔑";
                color = "#7c5e10";
            } else if (cell == QChar('E')) {
                text = "🚪";
                color = "#166534";
            } else if (cell == QChar('T')) {
                text = "⚠️";
                color = "#7f1d1d";
            } else if (cell == QChar('H')) {
                text = "❤️";
                color = "#9f1239";
            } else if (cell == QChar('M')) {
                text = "👾";
                color = "#581c87";
            } else {
                color = "#334155";
            }

            cells[row][col]->setText(text);
            cells[row][col]->setStyleSheet(
                "QLabel { background-color: " + color + "; border: 1px solid #475569;"
                " border-radius: 6px; font-size: 23px; }");
        }
    }
}

void MainWindow::updateStats()
{
    statsLabel->setText(QString("Подземелье: %1 | HP: %2 | Монеты: %3/%4 | Ключ: %5 | Ходы: %6")
                            .arg(dungeonNumber)
                            .arg(health)
                            .arg(coins)
                            .arg(NeedCoins)
                            .arg(hasKey ? "Да" : "Нет")
                            .arg(moves));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W || event->key() == Qt::Key_Up) {
        movePlayer(-1, 0);
    } else if (event->key() == Qt::Key_S || event->key() == Qt::Key_Down) {
        movePlayer(1, 0);
    } else if (event->key() == Qt::Key_A || event->key() == Qt::Key_Left) {
        movePlayer(0, -1);
    } else if (event->key() == Qt::Key_D || event->key() == Qt::Key_Right) {
        movePlayer(0, 1);
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::movePlayer(int dr, int dc)
{
    if (gameEnded) {
        return;
    }

    int newRow = playerRow + dr;
    int newCol = playerCol + dc;
    QChar nextCell = map[newRow][newCol];

    if (nextCell == QChar('#')) {
        return;
    }
    if (!handleCell(nextCell)) {
        return;
    }

    map[playerRow][playerCol] = QChar('.');
    playerRow = newRow;
    playerCol = newCol;
    map[playerRow][playerCol] = QChar('P');
    ++moves;

    checkMonsterAttack();
    updateStats();
    drawMap();

    if (health <= 0) {
        gameEnded = true;
        QMessageBox::information(this, "Поражение", "Вы проиграли. Здоровье закончилось.");
        restartGame();
    }
}

bool MainWindow::handleCell(QChar cell)
{
    if (cell == QChar('C')) {
        ++coins;
    } else if (cell == QChar('K')) {
        hasKey = true;
    } else if (cell == QChar('T')) {
        health -= 20;
    } else if (cell == QChar('H')) {
        health += 20;
        if (health > 100) {
            health = 100;
        }
    } else if (cell == QChar('M')) {
        health -= 30;
    } else if (cell == QChar('E')) {
        if (!hasKey) {
            QMessageBox::information(this, "Выход закрыт", "Сначала найдите ключ!");
            return false;
        }
        if (coins < NeedCoins) {
            QMessageBox::information(this, "Нужны монеты", "Соберите больше монет, чтобы сбежать!");
            return false;
        }
        gameEnded = true;
        QMessageBox::information(
            this,
            "Победа",
            QString("Подземелье пройдено!\nМонеты: %1\nХоды: %2\nHP: %3")
                .arg(coins)
                .arg(moves)
                .arg(health));
        ++dungeonNumber;
        restartGame();
        return false;
    }

    return true;
}

void MainWindow::checkMonsterAttack()
{
    int dr[4] = {-1, 1, 0, 0};
    int dc[4] = {0, 0, -1, 1};

    for (int i = 0; i < 4; ++i) {
        int row = playerRow + dr[i];
        int col = playerCol + dc[i];
        if (row >= 0 && row < Rows && col >= 0 && col < Cols && map[row][col] == QChar('M')) {
            health -= 10;
            return;
        }
    }
}

void MainWindow::restartGame()
{
    generateMap();
}

void MainWindow::showRules()
{
    QMessageBox::information(
        this,
        "Правила",
        "Найдите ключ 🔑 и соберите минимум 4 монеты 🟡.\n"
        "После этого дойдите до выхода 🚪.\n\n"
        "W/A/S/D или стрелки - движение.\n"
        "Ловушка ⚠️: -20 HP и исчезает.\n"
        "Аптечка ❤️: +20 HP, максимум 100.\n"
        "Монстр 👾: -30 HP, если наступить на него.\n"
        "Если после хода стоять рядом с монстром: -10 HP.");
}
