#include "test2.hpp"
#include <QPainter>

WhiteSquareWidget::WhiteSquareWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(150, 150);  // Tamaño mínimo para que el cuadrado se vea bien
}

void WhiteSquareWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::black);  // Fondo negro (opcional)

    int size = 100;
    int x = (width() - size) / 2;
    int y = (height() - size) / 2;

    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawRect(x, y, size, size);
}
