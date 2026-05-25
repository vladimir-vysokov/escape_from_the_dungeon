#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <vector>

class QGridLayout;
class QKeyEvent;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void generateMap();
    void placeRandomObjects();
    bool isSafeZone(int row, int col);
    bool isMapValid();
    QVector<QVector<bool>> bfsReachable();
    int countReachableCoins(const QVector<QVector<bool>> &visited);
    void drawMap();
    void updateStats();
    void movePlayer(int dr, int dc);
    bool handleCell(QChar cell);
    void checkMonsterAttack();
    void restartGame();
    void showRules();

    static const int Rows = 12, Cols = 12, NeedCoins = 4;

    QLabel *titleLabel = nullptr, *statsLabel = nullptr;
    QGridLayout *mapLayout = nullptr;
    QVector<QVector<QLabel*>> cells;

    QPushButton *newGameButton = nullptr;
    QPushButton *rulesButton = nullptr, *exitButton = nullptr;

    int dungeonNumber = 1, wallCount = 20;
    int playerRow = 1, playerCol = 1;
    int health = 100, coins = 0, moves = 0;
    bool hasKey = false;
    bool gameEnded = false;

    std::vector<QString> map;
};

#endif
