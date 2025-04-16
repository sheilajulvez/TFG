#ifndef TEST2_H
#define TEST2_H

#include <QWidget>

class WhiteSquareWidget : public QWidget {
    Q_OBJECT

public:
    explicit WhiteSquareWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // WHITE_SQUARE_WIDGET_HPP
