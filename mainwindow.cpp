#include "mainwindow.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QRectF>
#include <QSizeF>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include <queue>

namespace {
QString resolveAssetPath(const QString &name)
{
    const QString resourcePath = QString(":/assets/%1").arg(name);
    if (QFile::exists(resourcePath)) {
        return resourcePath;
    }

    const QString appPath = QCoreApplication::applicationDirPath() + "/assets/" + name;
    if (QFileInfo::exists(appPath)) {
        return appPath;
    }

    const QString relativePath = QString("assets/%1").arg(name);
    if (QFileInfo::exists(relativePath)) {
        return relativePath;
    }

    return name;
}

QRect opaqueBounds(const QImage &image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) == 0) {
                continue;
            }

            if (bounds.isNull()) {
                bounds = QRect(x, y, 1, 1);
            } else {
                bounds = bounds.united(QRect(x, y, 1, 1));
            }
        }
    }
    return bounds;
}

QPointF opaqueCentroid(const QImage &image)
{
    qint64 totalX = 0;
    qint64 totalY = 0;
    qint64 count = 0;

    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha == 0) {
                continue;
            }

            totalX += x * alpha;
            totalY += y * alpha;
            count += alpha;
        }
    }

    if (count == 0) {
        return QPointF(image.width() / 2.0, image.height() / 2.0);
    }

    return QPointF(static_cast<qreal>(totalX) / count, static_cast<qreal>(totalY) / count);
}

QPixmap loadSvgIcon(const QString &name, const QSize &size, bool trimTransparent = false, bool centerByMass = false)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QSvgRenderer renderer(resolveAssetPath(name));
    if (!renderer.isValid()) {
        return pixmap;
    }

    const int sourceSide = qMax(size.width(), size.height()) * 4;
    QImage image(sourceSide, sourceSide, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter);
    painter.end();

    QImage source = image;
    if (trimTransparent) {
        const QRect bounds = opaqueBounds(image);
        if (!bounds.isNull()) {
            source = image.copy(bounds);
        }
    }

    QPointF sourceCenter(source.width() / 2.0, source.height() / 2.0);
    QPointF massCenter = opaqueCentroid(source);
    QPointF offset(0.0, 0.0);
    if (centerByMass) {
        offset = sourceCenter - massCenter;
    }

    QPainter output(&pixmap);
    output.setRenderHint(QPainter::Antialiasing, true);

    const QPixmap scaled = QPixmap::fromImage(source).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPointF topLeft((size.width() - scaled.width()) / 2.0 + offset.x() * scaled.width() / source.width(),
                          (size.height() - scaled.height()) / 2.0 + offset.y() * scaled.height() / source.height());
    output.drawPixmap(topLeft, scaled);
    return pixmap;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    generateMap();
    updateMapCellSize();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateMapCellSize();
}

void MainWindow::setupUi()
{
    setWindowTitle("Побег из подземелья");
    setMinimumSize(820, 720);
    resize(980, 860);
    setFocusPolicy(Qt::StrongFocus);

    QWidget *central = new QWidget(this);
    central->setStyleSheet("background-color: #111827; color: #f9fafb;");
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    topPanel = new QFrame(central);
    topPanel->setStyleSheet(
        "QFrame { background-color: #172033; border: 1px solid #334155; border-radius: 8px; }"
        "QLabel { border: none; }");
    QVBoxLayout *topLayout = new QVBoxLayout(topPanel);
    topLayout->setContentsMargins(12, 8, 12, 8);

    titleLabel = new QLabel("Побег из подземелья", topPanel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #fbbf24;");

    statsLabel = new QLabel(topPanel);
    statsLabel->setAlignment(Qt::AlignCenter);
    statsLabel->setStyleSheet("font-size: 14px; color: #e5e7eb;");

    topLayout->addWidget(titleLabel);
    topLayout->addWidget(statsLabel);
    mainLayout->addWidget(topPanel);

    mapWidget = new QWidget(central);
    mapWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mapLayout = new QGridLayout(mapWidget);
    mapLayout->setSpacing(0);
    mapLayout->setContentsMargins(0, 0, 0, 0);

    for (int row = 0; row < Rows; ++row) {
        QVector<QLabel*> line;
        for (int col = 0; col < Cols; ++col) {
            QLabel *cell = new QLabel(mapWidget);
            cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            cell->setMinimumSize(0, 0);
            cell->setAlignment(Qt::AlignCenter);
            mapLayout->addWidget(cell, row, col);
            line.push_back(cell);
        }
        cells.push_back(line);
    }

    for (int row = 0; row < Rows; ++row) {
        mapLayout->setRowStretch(row, 1);
    }
    for (int col = 0; col < Cols; ++col) {
        mapLayout->setColumnStretch(col, 1);
    }

    QHBoxLayout *centerLayout = new QHBoxLayout;
    centerLayout->addStretch();
    centerLayout->addWidget(mapWidget);
    centerLayout->addStretch();
    mainLayout->addLayout(centerLayout, 1);

    bottomPanel = new QFrame(central);
    bottomPanel->setStyleSheet(
        "QFrame { background-color: #172033; border: 1px solid #334155; border-radius: 8px; }"
        "QPushButton { background-color: #2563eb; color: white; border: none; border-radius: 6px;"
        " padding: 8px 16px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3b82f6; }"
        "QPushButton:pressed { background-color: #1d4ed8; }");
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(12, 8, 12, 8);
    bottomLayout->setSpacing(8);

    newGameButton = new QPushButton("Новая игра", bottomPanel);
    newGameButton->setMinimumWidth(140);

    bottomLayout->addStretch();
    bottomLayout->addWidget(newGameButton);
    bottomLayout->addStretch();
    mainLayout->addWidget(bottomPanel);

    connect(newGameButton, &QPushButton::clicked, this, [this]() {
        dungeonNumber = 1;
        generateMap();
        setFocus();
    });

}

void MainWindow::updateMapCellSize()
{
    if (!mapWidget || cells.isEmpty() || cells.first().isEmpty() || map.size() != Rows) {
        return;
    }

    QWidget *central = centralWidget();
    if (!central) {
        return;
    }

    const QMargins margins = (central && central->layout()) ? central->layout()->contentsMargins() : QMargins();
    const int availableWidth = central->width() - margins.left() - margins.right();
    const int availableHeight = central->height()
                                - margins.top() - margins.bottom()
                                - (topPanel ? topPanel->height() : 0)
                                - (bottomPanel ? bottomPanel->height() : 0)
                                - 16;

    const int cellSize = qMax(28, qMin(availableWidth / Cols, availableHeight / Rows));

    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            cells[row][col]->setFixedSize(cellSize, cellSize);
        }
    }

    mapWidget->setFixedSize(cellSize * Cols, cellSize * Rows);
    drawMap();
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
    const int cellSize = cells.isEmpty() || cells.first().isEmpty() ? 28 : cells[0][0]->width();
    const int iconSize = qMax(16, cellSize - 12);

    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            QChar cell = map[row][col];
            QString color;
            QPixmap icon;

            if (cell == QChar('#')) {
                color = "#020617";
            } else if (cell == QChar('P')) {
                color = "#38bdf8";
                icon = loadSvgIcon("player.svg", QSize(iconSize, iconSize));
            } else if (cell == QChar('C')) {
                color = "#f59e0b";
                icon = loadSvgIcon("coin.svg", QSize(iconSize, iconSize));
            } else if (cell == QChar('K')) {
                color = "#7c5e10";
                icon = loadSvgIcon("key.svg", QSize(iconSize, iconSize), true, true);
            } else if (cell == QChar('E')) {
                color = "#166534";
                icon = loadSvgIcon("door.svg", QSize(iconSize, iconSize));
            } else if (cell == QChar('T')) {
                color = "#7f1d1d";
                icon = loadSvgIcon("alert.svg", QSize(iconSize, iconSize));
            } else if (cell == QChar('H')) {
                color = "#9f1239";
                icon = loadSvgIcon("medicine.svg", QSize(iconSize, iconSize));
            } else if (cell == QChar('M')) {
                color = "#581c87";
                icon = loadSvgIcon("monster.svg", QSize(iconSize, iconSize));
            } else {
                color = "#334155";
            }

            cells[row][col]->setText(QString());
            cells[row][col]->setPixmap(icon);
            QString style = "QLabel { background-color: " + color + "; border: 1px solid #475569;"
                            " border-radius: 6px; }";
            cells[row][col]->setStyleSheet(style);
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
